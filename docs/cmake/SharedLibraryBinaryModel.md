# Shared library binary와 inline 모델

이 문서는 특정 Tool target에 종속되지 않는 shared library 공통 모델을 설명한다. Windows의 implicit linking을 기준으로 compile, link, load, call 단계를 나누고 C++ translation unit과 inline 함수가 DLL 경계에서 어떻게 처리되는지 연결한다.

Linux와 macOS는 object format과 loader 용어가 다르지만, compile 단계에서 외부 symbol reference를 만들고 link/load 단계에서 shared library의 public symbol에 연결한다는 큰 구조는 같다.

## 학습 목표

이 문서를 읽은 뒤 다음을 설명할 수 있어야 한다.

- 각 translation unit이 object file과 symbol reference를 만드는 과정
- static `.lib`와 DLL import `.lib`의 차이
- 링크 단계에서 import library가 실행 파일에 남기는 정보
- 로드 단계에서 DLL Export Table과 executable IAT가 연결되는 과정
- 호출할 때마다 Export Table을 다시 검색하지 않는 이유
- 일반 public inline과 MSVC exported/imported inline의 차이
- CMake visibility property와 export macro의 역할 차이

## 주요 용어

| 용어 | 역할 |
|---|---|
| Translation unit | 전처리된 하나의 `.cpp`를 독립적으로 컴파일하는 단위 |
| Object file | 기계어, 정의된 symbol과 아직 해결되지 않은 외부 symbol reference를 담는 컴파일 결과 |
| Symbol | 함수나 데이터의 이름과 linkage identity |
| Shared library | 실행 파일과 분리되어 로드되는 코드와 데이터 binary |
| Export Table | DLL이 다른 binary에 제공하는 public symbol 이름과 주소 정보 |
| Import library | 링크 단계에서 어떤 DLL이 어떤 symbol을 제공하는지 linker에 알려주는 `.lib` |
| Import Directory | 실행 파일이 필요로 하는 DLL과 import 목록을 기록한 PE 영역 |
| Import Address Table | loader가 실제 DLL 함수 주소를 기록하는 실행 파일의 table |
| Loader | 실행 시 DLL을 적재하고 import를 실제 주소에 연결하는 운영체제 구성 요소 |

## Binary와 단계별 관계

Windows의 일반적인 implicit linking 결과물은 다음 책임을 가진다.

```text
Public headers
    -> Compile 단계의 함수와 타입 선언

consumer.obj
    -> Consumer 기계어와 unresolved/import symbol reference

pstk_packet.lib
    -> Link 단계에서 DLL 이름과 import symbol 정보 제공

consumer.exe
    -> Link 결과로 Import Directory와 IAT slot 보유

pstk_packet.dll
    -> Runtime 기계어와 Export Table 보유
```

가장 중요한 구분은 다음과 같다.

> Import `.lib`는 링크 단계의 입력이며 runtime loader가 다시 읽는 파일이 아니다.

링커가 `.lib` 정보를 사용해 DLL 이름과 symbol import를 `consumer.exe`의 PE 정보로 기록한다. Runtime loader는 실행 파일에 이미 기록된 Import Directory와 DLL Export Table을 사용한다.

## 1. Compile: translation unit별 symbol 생성

각 `.cpp`는 독립적인 translation unit으로 컴파일된다.

```cpp
#include <pstk/packet/PacketToolApi.h>

int main()
{
    return PstkPacketGetApiVersion() == 1U ? 0 : 1;
}
```

Consumer를 컴파일할 때 public export macro는 Windows에서 개념적으로 다음 선언을 만든다.

```cpp
__declspec(dllimport) uint32_t PstkPacketGetApiVersion(void);
```

Compiler는 함수 타입과 calling convention을 검사하고 호출 코드를 생성한다. 실제 runtime 주소는 아직 알 수 없으므로 object file에는 import 대상 symbol reference가 남는다.

```text
ConsumerA.cpp -> ConsumerA.obj
                  └─ PstkPacketGetApiVersion 필요

ConsumerB.cpp -> ConsumerB.obj
                  └─ PstkPacketGetApiVersion 필요
```

Compiler는 이 단계에서 `pstk_packet.lib`를 읽어 최종 DLL 주소를 정하지 않는다. `.lib`는 다음 link 단계의 입력이다.

## 2. Link: object와 import library 연결

Linker는 Consumer object들과 `pstk_packet.lib`를 함께 처리한다.

```text
ConsumerA.obj ─┐
ConsumerB.obj ─┼─ Linker -> consumer.exe
pstk_packet.lib┘
```

Import library는 `PstkPacketGetApiVersion`의 구현 기계어를 Consumer에 복사하지 않는다. 이 symbol이 `pstk_packet.dll`에서 제공된다는 연결 정보를 제공한다.

Linker는 그 정보를 사용해 최종 실행 파일에 다음을 기록한다.

- 필요한 DLL 이름
- 이름 또는 ordinal로 식별되는 import symbol
- 실제 함수 주소가 기록될 IAT slot

해결할 정의도 import 정보도 찾지 못하면 unresolved external symbol link 오류가 발생한다.

### Static `.lib`와 Import `.lib`

Windows의 두 파일은 같은 확장자를 사용하지만 책임이 다르다.

```text
Static library
    -> 실제 object code 포함
    -> Link 시 구현이 Consumer binary에 포함

Import library
    -> DLL 이름과 public symbol 연결 정보 포함
    -> 실제 함수 구현은 DLL에 존재
```

따라서 `.lib`라는 확장자만으로 static library인지 import library인지 판단하면 안 된다.

## 3. Load: DLL Export와 executable Import 매핑

프로그램을 실행하면 Windows Loader는 `consumer.exe`의 Import Directory를 읽고 필요한 DLL을 프로세스 주소 공간에 적재한다.

```text
consumer.exe Import Directory
    -> pstk_packet.dll 필요
    -> PstkPacketGetApiVersion 필요

pstk_packet.dll Export Table
    -> PstkPacketGetApiVersion의 실제 runtime 주소
```

Loader는 DLL Export Table에서 이름 또는 ordinal이 맞는 항목을 찾고 실제 주소를 Consumer IAT slot에 기록한다.

```text
consumer.exe IAT
    [PstkPacketGetApiVersion]
        -> 0x... DLL의 실제 함수 주소
```

이 단계에서는 `pstk_packet.lib`를 다시 열지 않는다. Linker가 import library의 정보를 이미 `consumer.exe`에 반영했기 때문이다.

DLL 파일을 찾지 못하면 모듈 로드가 실패하고, DLL은 찾았지만 필요한 export symbol이 없으면 import resolution이 실패한다.

## 4. Call: IAT를 통한 DLL 함수 호출

Loader가 IAT를 채운 뒤 Consumer는 해당 slot을 통해 DLL 함수를 호출한다.

```text
Consumer call site
    -> IAT slot 또는 linker thunk
    -> pstk_packet.dll 함수 주소
    -> DLL 함수 실행
```

`__declspec(dllimport)`를 확인한 MSVC는 직접 `__imp_Function` IAT slot을 사용하는 간접 호출 코드를 만들 수 있다. `dllimport`가 없어도 linker thunk를 통해 DLL 호출을 연결할 수 있지만, `dllimport`는 불필요한 thunk를 줄일 수 있다.

호출할 때마다 DLL Export Table을 이름으로 다시 검색하는 것은 아니다. 일반 load-time linking에서는 loader가 시작 시 연결해 둔 IAT 주소를 재사용한다.

### DLL 내부의 함수 호출

이미 DLL 내부에서 실행 중인 코드가 같은 DLL의 다른 함수를 호출하면 linker가 같은 binary 안에서 직접 연결할 수 있다.

```text
Consumer -> IAT -> DLL PublicFunction
                     -> DLL InternalFunction
```

`DLL PublicFunction -> DLL InternalFunction` 구간은 Consumer의 IAT를 다시 거칠 필요가 없다.

## Translation unit과 일반 public inline

C++의 `inline`은 호출 위치에 본문을 반드시 삽입하라는 명령이 아니다. 언어적으로 중요한 특징은 external linkage를 가진 동일한 inline 정의를 여러 translation unit에 둘 수 있다는 것이다.

```cpp
inline bool IsSupportedVersion(const uint32_t version)
{
    return version >= 1U;
}
```

이 정의가 public header에 있으면 각 Consumer translation unit이 본문을 확인하고 컴파일할 수 있다.

```text
ConsumerA.cpp
├─ 호출 위치에 본문 삽입 가능
└─ Consumer용 out-of-line 사본 생성 가능

ConsumerB.cpp
├─ 호출 위치에 본문 삽입 가능
└─ Consumer용 out-of-line 사본 생성 가능
```

여러 object에 out-of-line 사본이 생기면 compiler/linker의 COMDAT 또는 weak symbol 처리로 중복을 정리할 수 있다. 이 함수 자체에는 DLL import가 필요하지 않으며 public header가 구현을 제공한다.

## Public inline이 exported 함수를 호출하는 경우

일반 public inline helper가 DLL의 exported 함수를 호출할 수 있다.

```cpp
PSTK_PACKET_API uint32_t PstkPacketGetApiVersion(void);

inline bool PstkPacketSupportsV1()
{
    return PstkPacketGetApiVersion() >= 1U;
}
```

Compiler가 helper를 호출 위치에 펼치면 Consumer 코드는 직접 DLL import 함수를 호출한다.

```text
Consumer에 펼쳐진 비교 코드
    -> IAT
    -> DLL PstkPacketGetApiVersion
```

Helper를 펼치지 않으면 Consumer binary에 생성된 helper 사본이 DLL import 함수를 호출한다.

```text
Consumer -> Consumer helper 사본
             -> IAT
             -> DLL PstkPacketGetApiVersion
```

따라서 helper 자체는 Consumer가 제공하지만, 본문에서 직접 참조하는 DLL 내부 함수는 public export symbol이어야 한다. Header 정의도 없고 DLL에서 export하지도 않은 internal 함수는 Consumer linker가 해결할 수 없다.

## MSVC exported/imported inline

MSVC는 inline 함수 자체에 `dllexport`와 `dllimport`를 적용하는 별도 규칙을 제공한다.

```cpp
PSTK_PACKET_API inline bool PstkPacketSupportsV1()
{
    return PstkPacketGetApiVersion() >= 1U;
}
```

### DLL을 빌드할 때

`PSTK_PACKET_API`가 `__declspec(dllexport)`로 확장된다.

- DLL compiler는 inline 여부와 관계없이 out-of-line 사본을 생성한다.
- 해당 사본을 DLL Export Table에 공개한다.

### Consumer를 빌드할 때

`PSTK_PACKET_API`가 `__declspec(dllimport)`로 확장된다.

- Compiler는 함수 본문을 Consumer 호출 위치에 펼칠 수 있다.
- Consumer용 독립 out-of-line 함수 사본은 생성하지 않는다.
- 펼치지 않은 호출이나 함수 주소가 필요하면 DLL에 있는 사본을 사용한다.

두 실행 경로는 다음과 같다.

```text
본문을 Consumer에 펼침
    -> 비교 코드는 Consumer에서 실행
    -> 내부 exported 함수는 IAT를 통해 DLL 호출

본문을 펼치지 않음
    -> Consumer가 DLL의 inline export symbol 호출
    -> DLL 사본이 같은 DLL의 내부 함수를 호출
```

| 함수 형태 | Consumer 본문 펼치기 | Consumer 독립 함수 사본 | DLL export 사본 |
|---|---:|---:|---:|
| 일반 public inline | 가능 | 가능 | 없음 |
| MSVC exported/imported inline | 가능 | 불가능 | 있음 |
| 일반 exported 함수 | 해당 없음 | 없음 | 있음 |

### 버전 불일치 위험

Exported/imported inline의 본문이 Consumer에 펼쳐지면 DLL만 교체해도 이미 빌드된 Consumer의 본문은 바뀌지 않는다.

```text
기존 Consumer에 삽입된 inline 본문
    -> 이전 버전

새 DLL의 out-of-line export 사본
    -> 새 버전
```

같은 API가 호출 경로에 따라 서로 다른 버전으로 실행될 수 있으므로 DLL을 변경할 때 Consumer 재컴파일이 필요할 수 있다. 또한 이 동작은 MSVC 특화 규칙이므로 크로스 플랫폼 public ABI의 기본 계약으로 사용하지 않는다.

## CMake symbol visibility 설정

Shared library target은 다음 설정으로 기본 symbol 공개 범위를 줄일 수 있다.

```cmake
set_target_properties(
    library_target
    PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN TRUE
)
```

### `CXX_VISIBILITY_PRESET hidden`

지원하는 compiler에서 일반 C++ symbol을 기본 dynamic symbol로 공개하지 않도록 하는 target property다. 개념적으로 `-fvisibility=hidden` 같은 compiler option과 연결된다.

### `VISIBILITY_INLINES_HIDDEN TRUE`

지원하는 compiler에서 inline 함수 사본의 symbol을 기본적으로 숨기는 option을 추가한다. 이는 inline 최적화를 강제하지 않고 symbol visibility만 제어한다.

### `GenerateExportHeader`

CMake의 `GenerateExportHeader`는 target을 빌드할 때와 consumer가 header를 포함할 때 서로 다른 import/export 속성을 제공하는 매크로 header를 생성한다.

```text
Shared library 자신을 빌드
    API_MACRO -> export

Consumer를 빌드
    API_MACRO -> import 또는 default visibility
```

관계는 다음과 같다.

```text
기본 visibility property
    -> 공개하지 않은 symbol의 기본 정책

GenerateExportHeader의 API macro
    -> public ABI에 포함할 symbol을 명시적으로 공개
```

Windows에서는 `__declspec(dllexport/dllimport)`, GCC/Clang 계열에서는 visibility attribute처럼 플랫폼에 맞는 선언으로 변환된다.

## Public ABI 설계 규칙

크로스 플랫폼 shared library의 public ABI에는 다음 규칙을 우선한다.

- Public ABI는 `extern "C"` 함수와 고정 폭 정수처럼 단순한 타입을 사용한다.
- C++ STL container, exception, RTTI와 compiler별 class layout을 경계에 직접 노출하지 않는다.
- 한 모듈에서 할당한 메모리는 같은 모듈에서 해제할 수 있는 API를 제공한다.
- Shared state나 자주 변경되는 정책을 public inline 함수에 두지 않는다.
- 핵심 동작은 non-inline exported 함수로 두고 단순 convenience helper만 일반 public inline으로 제공한다.
- MSVC exported/imported inline은 필요한 Windows 전용 ABI에서만 명시적으로 선택한다.

## 현재 Packet Tool에 적용한 모델

현재 Packet Tool은 다음과 같은 안정적인 기본형을 사용한다.

```cpp
PSTK_PACKET_API uint32_t PstkPacketGetApiVersion(void);
```

```text
PacketToolApi.h
    -> C ABI 선언과 import/export macro

pstk_packet.lib
    -> Windows link 단계의 import 정보

pstk_packet.dll
    -> 실제 구현과 Export Table

Consumer executable
    -> Import Directory와 IAT
```

현재 API는 일반 exported non-inline 함수이며 exported/imported inline을 public ABI에 사용하지 않는다.

## 확인 도구

Binary에 실제로 기록된 symbol과 dependency는 다음 도구로 확인할 수 있다.

```text
Windows
    dumpbin /EXPORTS pstk_packet.dll
    dumpbin /IMPORTS consumer.exe

macOS
    nm -gU libpstk_packet.dylib
    otool -L consumer

Linux
    nm -D libpstk_packet.so
    readelf --dyn-syms libpstk_packet.so
    ldd consumer
```

## 복기 체크

1. Compiler가 public header만으로 최종 DLL runtime 주소를 알 수 없는 이유는 무엇인가?
2. Import `.lib`는 어떤 단계에서 사용되며 loader가 다시 읽지 않는 이유는 무엇인가?
3. Loader가 DLL Export Table에서 찾은 주소를 IAT에 기록하는 이유는 무엇인가?
4. 일반 public inline과 MSVC imported inline은 Consumer out-of-line 사본에서 어떻게 다른가?
5. Imported inline 본문이 Consumer에 펼쳐질 때 DLL 교체만으로 구현이 갱신되지 않는 이유는 무엇인가?
6. `VISIBILITY_INLINES_HIDDEN`이 강제 inline option이 아닌 이유는 무엇인가?

## 공식 참고 자료

- [PE Format: Export, Import Directory와 IAT](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format)
- [MSVC `dllimport` 함수 호출과 IAT](https://learn.microsoft.com/en-us/cpp/build/importing-function-calls-using-declspec-dllimport?view=msvc-170)
- [MSVC inline 함수의 `dllexport`와 `dllimport`](https://learn.microsoft.com/en-us/cpp/cpp/defining-inline-cpp-functions-with-dllexport-and-dllimport?view=msvc-170)
- [CMake `GenerateExportHeader`](https://cmake.org/cmake/help/latest/module/GenerateExportHeader.html)
- [CMake `CXX_VISIBILITY_PRESET`](https://cmake.org/cmake/help/latest/prop_tgt/LANG_VISIBILITY_PRESET.html)
- [CMake `VISIBILITY_INLINES_HIDDEN`](https://cmake.org/cmake/help/latest/prop_tgt/VISIBILITY_INLINES_HIDDEN.html)
