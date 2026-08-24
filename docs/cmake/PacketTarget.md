# Packet DLL CMake target 해설

이 문서는 `tools/packet/CMakeLists.txt`에 정의된 Packet DLL target을 설명한다. 현재 목표는 다음과 같다.

- Packet Tool 구현을 플랫폼별 shared library로 빌드한다.
- 외부에 공개할 C API만 명시적으로 export한다.
- 현재 build tree의 테스트와 이후 설치된 외부 consumer가 public header를 찾을 수 있는 경계를 준비한다.
- 내부 C++ symbol을 public ABI에서 제외한다.

번역 단위, object symbol, import library, DLL Export Table, executable IAT와 inline 함수의 일반 원리는 [`SharedLibraryBinaryModel.md`](SharedLibraryBinaryModel.md)에서 별도로 설명한다. 이 문서는 해당 원리가 현재 Packet target에 어떻게 적용됐는지만 다룬다.

## 현재 target 구성

```cmake
include(GenerateExportHeader)

add_library(
    pstk_packet
    SHARED
    src/PacketToolApi.cpp
)

add_library(
    PSTK::Packet
    ALIAS
    pstk_packet
)

target_compile_features(
    pstk_packet
    PUBLIC
    cxx_std_17
)

target_include_directories(
    pstk_packet
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

set_target_properties(
    pstk_packet
    PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN TRUE
)

generate_export_header(
    pstk_packet
    EXPORT_FILE_NAME
        "${CMAKE_CURRENT_BINARY_DIR}/pstk_packet_export.h"
    EXPORT_MACRO_NAME
        PSTK_PACKET_API
)
```

## `include(GenerateExportHeader)`

```cmake
include(GenerateExportHeader)
```

CMake가 제공하는 `GenerateExportHeader` 모듈을 불러온다. 이 모듈을 포함해야 이후 `generate_export_header()` 명령을 사용할 수 있다.

이 모듈은 shared library 자체를 만들지 않는다. 플랫폼별 symbol export/import 매크로가 들어 있는 C/C++ header를 생성하는 기능을 제공한다.

## `add_library(... SHARED ...)`

```cmake
add_library(
    pstk_packet
    SHARED
    src/PacketToolApi.cpp
)
```

`pstk_packet`이라는 실제 CMake target을 만들고 `PacketToolApi.cpp`를 shared library로 컴파일한다.

| 플랫폼 | 대표 결과물 |
|---|---|
| Windows | `pstk_packet.dll`, `pstk_packet.lib` import library |
| Linux | `libpstk_packet.so` |
| macOS | `libpstk_packet.dylib` |

`SHARED`는 동적 라이브러리 형식을 선택하는 것이며 외부에 공개할 symbol을 자동으로 설계해주지는 않는다. Public API symbol은 export macro로 별도 표시한다.

## Alias target

```cmake
add_library(
    PSTK::Packet
    ALIAS
    pstk_packet
)
```

`PSTK::Packet`은 실제 target `pstk_packet`을 가리키는 CMake 별칭이다.

```text
pstk_packet
    -> 실제 소스 컴파일과 DLL 생성

PSTK::Packet
    -> 같은 build tree의 consumer가 사용하는 읽기 좋은 이름
```

테스트 target은 다음처럼 사용할 수 있다.

```cmake
target_link_libraries(
    pstk_packet_tests
    PRIVATE
    PSTK::Packet
)
```

Alias는 다음 특성을 갖는다.

- 새로운 DLL을 추가로 만들지 않는다.
- 원본 target의 usage requirement를 그대로 전달한다.
- `PSTK::Packet`처럼 namespace가 있는 이름으로 target 오타를 configure 단계에서 찾기 쉽게 한다.
- Alias 자체에는 compile feature나 include directory 같은 속성을 설정할 수 없다.
- 현재 build tree 안에서만 존재하며 설치된 외부 패키지 target을 자동으로 만들지는 않는다.

외부 CMake 프로젝트에서도 `PSTK::Packet`을 제공하려면 이후 `install(EXPORT ...)`와 package config가 필요하다.

## C++17 요구사항

```cmake
target_compile_features(
    pstk_packet
    PUBLIC
    cxx_std_17
)
```

`pstk_packet` target이 최소 C++17을 요구한다고 CMake에 알린다. 필요한 경우 CMake가 컴파일러에 표준 선택 옵션을 추가한다.

`PUBLIC`은 요구사항을 다음 양쪽에 적용한다.

```text
pstk_packet 자신
PSTK::Packet을 링크하는 consumer
```

현재 public API는 C ABI 중심이어서 public header가 C++17 타입을 노출하지 않는다. 이후에도 이 상태가 유지된다면 `PRIVATE cxx_std_17`로 좁혀도 consumer 계약에는 지장이 없는지 다시 판단할 수 있다. 현재 `PUBLIC`은 consumer도 C++17 이상을 사용해야 한다는 명시적 계약이다.

## Public include directory

```cmake
target_include_directories(
    pstk_packet
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)
```

컴파일러가 public header를 검색할 디렉터리를 target usage requirement로 등록한다.

`PUBLIC`이므로 다음 양쪽에 경로가 전달된다.

- `pstk_packet` 자체를 컴파일할 때
- `PSTK::Packet`을 링크하는 consumer를 컴파일할 때

### Source include directory

```cmake
$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
```

현재 `CMakeLists.txt`가 `tools/packet`에 있으므로 source 경로는 다음과 같다.

```text
tools/packet/include
└─ pstk/packet/PacketToolApi.h
```

따라서 다음 include를 사용할 수 있다.

```cpp
#include <pstk/packet/PacketToolApi.h>
```

### Binary include directory

```cmake
$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
```

`generate_export_header()`가 build tree에 생성하는 다음 header를 찾기 위해 필요하다.

```text
out/build/dev/tools/packet/pstk_packet_export.h
```

Public API header는 이 생성 header를 포함한다.

```cpp
#include "pstk_packet_export.h"
```

### Build tree란 무엇인가

Build tree는 CMake가 한 번의 configure 과정에서 생성한 빌드 시스템과 그 결과가 놓이는 디렉터리 영역이다.

현재 preset에서는 다음 명령이 source tree를 읽어 build tree를 만든다.

```bash
cmake --preset dev
```

```text
Source tree                         Build tree
PrivateServerToolKit/               PrivateServerToolKit/out/build/dev/
├─ CMakeLists.txt                   ├─ CMakeCache.txt
└─ tools/                           ├─ 생성된 빌드 시스템 파일
   └─ packet/                       └─ tools/packet/
      ├─ CMakeLists.txt                ├─ pstk_packet_export.h
      ├─ include/                       └─ library/test 빌드 결과
      └─ src/
```

여기서 **같은 build tree**라는 말은 target들이 같은 `CMakeLists.txt` 파일에 정의되어 있다는 뜻이 아니다. 루트에서 시작한 한 번의 configure 과정에 `add_subdirectory()`로 연결되어, 같은 CMake target graph에 포함되었다는 뜻이다.

예를 들어 이후 Packet test를 다음처럼 분리할 수 있다.

```text
Root CMakeLists.txt
└─ add_subdirectory(tools/packet)
   └─ tools/packet/CMakeLists.txt
      ├─ add_library(pstk_packet ...)
      └─ add_subdirectory(tests)
         └─ tools/packet/tests/CMakeLists.txt
            ├─ add_executable(pstk_packet_tests ...)
            └─ target_link_libraries(
                   pstk_packet_tests
                   PRIVATE
                   PSTK::Packet
               )
```

두 target은 서로 다른 `CMakeLists.txt`에 정의되어도 모두 `out/build/dev`에 생성된 하나의 target graph에 속한다. 따라서 `pstk_packet_tests`가 `PSTK::Packet`을 링크하면, CMake는 `pstk_packet`의 build-tree usage requirement를 test target에 전달한다.

현재 프로젝트에는 `pstk_packet_tests`가 아직 정의되어 있지 않으며, 위 구조는 이후 test target을 추가했을 때의 예시다.

### `BUILD_INTERFACE`

```cmake
$<BUILD_INTERFACE:...>
```

설치된 package가 아니라, 현재 configure로 만들어진 target을 build tree 안에서 직접 사용할 때 적용되는 generator expression이다.

현재 설정에서는 다음 두 경로가 build-tree usage requirement가 된다.

| 경로 | 역할 |
|---|---|
| `${CMAKE_CURRENT_SOURCE_DIR}/include` | source tree에 있는 `PacketToolApi.h` 제공 |
| `${CMAKE_CURRENT_BINARY_DIR}` | build tree에 생성된 `pstk_packet_export.h` 제공 |

예를 들어 이후 `pstk_packet_tests`에 다음 연결을 추가한다고 가정한다.

```cmake
target_link_libraries(
    pstk_packet_tests
    PRIVATE
    PSTK::Packet
)
```

CMake는 `PSTK::Packet`이 가리키는 `pstk_packet` target의 `PUBLIC` include directory를 test target에 전파한다. 따라서 test의 compile 명령에는 위 두 include 경로가 자동으로 추가된다.

같은 저장소에 있어야만 하는 것은 아니다. 다른 source directory를 `add_subdirectory()`로 현재 configure에 참여시켰다면 그 target도 같은 build tree에서 `BUILD_INTERFACE`를 사용할 수 있다. 핵심은 파일의 물리적 위치가 아니라 같은 configure와 target graph에 참여하는지다.

### `INSTALL_INTERFACE`

`INSTALL_INTERFACE`는 모든 외부 DLL consumer에게 필수인 설정이 아니다. `install(EXPORT ...)`로 내보낸 target을 **설치된 CMake package target으로 제공할 때**, 외부 CMake 프로젝트에 public include 경로를 자동으로 전달하기 위한 설정이다.

```cmake
target_include_directories(
    pstk_packet
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:include>
)
```

설치 prefix가 `C:/PSTK`라면 `INSTALL_INTERFACE:include`는 다음 경로를 뜻한다.

```text
C:/PSTK/include
```

`INSTALL_INTERFACE`는 runtime DLL 검색 경로가 아니다. 설치된 target을 사용하는 외부 프로그램을 **컴파일할 때** public header를 찾기 위한 usage requirement다.

즉 이 구조에서 두 interface가 맡는 책임은 다음처럼 나뉜다.

| Consumer가 library를 얻는 방식 | 사용하는 target | 적용되는 include 설정 |
|---|---|---|
| Toolkit 내부 test 또는 `add_subdirectory()` consumer | 현재 configure에서 생성한 `pstk_packet` | `BUILD_INTERFACE` |
| `install()` 후 `find_package(PSTK)`를 사용하는 consumer | 설치·export된 imported target | `INSTALL_INTERFACE` |
| `.h`, `.lib`, `.dll`을 직접 전달받은 수동 SDK consumer | CMake target 없이 경로 직접 설정 | 어느 interface도 자동 적용되지 않음 |

`INSTALL_INTERFACE:include`는 source tree의 절대 경로를 외부에 노출하지 않고, 설치 prefix를 기준으로 옮겨 다닐 수 있는 include 경로를 package target에 기록하는 역할을 한다. Consumer가 installed target을 링크하면 이 경로가 compile 설정으로 전달된다.

또한 `INSTALL_INTERFACE`만 추가해도 파일이 복사되지는 않는다. 다음 install 규칙이 별도로 필요하다.

- Packet DLL과 Windows import library 설치
- 직접 작성한 public header 설치
- 생성된 `pstk_packet_export.h` 설치
- 외부 `find_package()`를 위한 target export와 package config 설치

현재 target은 build-tree 사용까지만 구성되어 있고 install/package 규칙은 아직 추가되지 않았다.

### Packet SDK로 배포할 때 필요한 결과물

현재 target을 Windows binary SDK로 패키징하려면 다음 결과물이 필요하다.

```text
PSTK Packet SDK/
├─ include/
│  ├─ pstk_packet_export.h
│  └─ pstk/packet/PacketToolApi.h
├─ lib/pstk_packet.lib
└─ bin/pstk_packet.dll
```

수동 SDK, 설치된 CMake package와 동일 build tree consumer의 binary 관계는 [`SharedLibraryBinaryModel.md`](SharedLibraryBinaryModel.md)에서 설명한다. 현재 target에는 아직 `install()`과 package export 규칙이 없다.

## Packet target의 symbol visibility 설정

```cmake
set_target_properties(
    pstk_packet
    PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN TRUE
)
```

`pstk_packet`은 지원하는 컴파일러에서 일반 C++ symbol과 inline symbol을 기본적으로 숨기고, `PSTK_PACKET_API`가 붙은 C API만 명시적으로 공개한다. 두 property는 보안 기능이 아니라 Packet DLL의 public ABI 표면을 제한하는 설정이다.

`CXX_VISIBILITY_PRESET`, `VISIBILITY_INLINES_HIDDEN`, 일반 public inline과 exported/imported inline의 차이는 [`SharedLibraryBinaryModel.md`](SharedLibraryBinaryModel.md)의 symbol visibility와 inline 절에서 설명한다.

## Export header 생성

```cmake
generate_export_header(
    pstk_packet
    EXPORT_FILE_NAME
        "${CMAKE_CURRENT_BINARY_DIR}/pstk_packet_export.h"
    EXPORT_MACRO_NAME
        PSTK_PACKET_API
)
```

다음 파일을 build tree에 생성한다.

```text
out/build/dev/tools/packet/pstk_packet_export.h
```

이 header에는 플랫폼별 symbol import/export 차이를 감추는 `PSTK_PACKET_API` 매크로가 정의된다.

개념적인 동작은 다음과 같다.

```cpp
#if defined(_WIN32)
    #if defined(pstk_packet_EXPORTS)
        #define PSTK_PACKET_API __declspec(dllexport)
    #else
        #define PSTK_PACKET_API __declspec(dllimport)
    #endif
#else
    #define PSTK_PACKET_API __attribute__((visibility("default")))
#endif
```

실제 생성 코드는 compiler와 target 종류를 고려하는 조건을 더 포함할 수 있다.

```text
Packet DLL 자체를 빌드할 때
    PSTK_PACKET_API -> export

Packet DLL consumer를 빌드할 때
    PSTK_PACKET_API -> import
```

생성된 header는 public API 선언에 사용되므로 직접 작성한 public header와 함께 설치해야 한다.

## 현재 Public C API

```cpp
#pragma once

#include <stdint.h>

#include "pstk_packet_export.h"

#ifdef __cplusplus
extern "C" {
#endif

PSTK_PACKET_API uint32_t PstkPacketGetApiVersion(void);

#ifdef __cplusplus
}
#endif
```

각 요소의 역할은 다음과 같다.

- `PSTK_PACKET_API`: shared library public symbol로 export/import한다.
- `extern "C"`: C++ name mangling을 제거해 compiler별 차이가 적은 C symbol 이름을 제공한다.
- `#ifdef __cplusplus`: 같은 header를 C와 C++ 양쪽에서 포함할 수 있게 한다.
- 고정 폭 정수형: 플랫폼별 기본 정수 크기 차이를 줄인다.

초기 API는 DLL 로딩과 ABI 연결을 검증하기 위한 최소 함수다.

```cpp
uint32_t PstkPacketGetApiVersion(void)
{
    return 1U;
}
```

## Packet Public API의 소비 흐름

Windows consumer는 `PacketToolApi.h`와 생성된 export header로 선언을 컴파일하고, `pstk_packet.lib`를 링크한 뒤 runtime에 `pstk_packet.dll`을 로드한다. `.lib`, executable Import Directory/IAT와 DLL Export Table의 정확한 역할은 [`SharedLibraryBinaryModel.md`](SharedLibraryBinaryModel.md)의 compile-link-load-call 절에서 설명한다.

## 현재 경계와 이후 작업

현재 완료된 경계는 다음과 같다.

- `pstk_packet` SHARED target
- build-tree 전용 public include 경로
- hidden-by-default symbol visibility
- `PSTK_PACKET_API` 생성 header
- C ABI API version 함수

기본 DLL 검증을 위해 아직 필요한 작업은 다음과 같다.

- Public API를 실제로 호출하는 smoke test target
- CTest 등록
- Windows에서 import library와 DLL 소비 검증

배포 자동화 범위는 선택한 소비 방식에 따라 나뉜다.

- Binary SDK 방식: public/generated header, DLL과 import library를 정해진 SDK 디렉터리에 패키징
- CMake package 방식: install directory, `INSTALL_INTERFACE`, target export와 package config 추가

## 복기 체크

다음을 설명할 수 있으면 현재 Packet target의 CMake 경계를 이해한 것이다.

1. `pstk_packet`과 `PSTK::Packet`이 각각 무엇인지
2. `PUBLIC cxx_std_17`이 consumer에게도 영향을 주는 이유
3. source include 경로와 binary include 경로가 모두 필요한 이유
4. `BUILD_INTERFACE`와 `INSTALL_INTERFACE`가 적용되는 시점과 `INSTALL_INTERFACE`가 필수가 아닌 이유
5. Packet target에서 기본 visibility를 숨기고 `PSTK_PACKET_API`만 공개하는 이유
6. Packet SDK에 public header, 생성 export header, import library와 DLL이 필요한 이유

## 공식 참고 자료

- [add_library](https://cmake.org/cmake/help/latest/command/add_library.html)
- [target_compile_features](https://cmake.org/cmake/help/latest/command/target_compile_features.html)
- [target_include_directories](https://cmake.org/cmake/help/latest/command/target_include_directories.html)
- [GenerateExportHeader](https://cmake.org/cmake/help/latest/module/GenerateExportHeader.html)
- [CXX_VISIBILITY_PRESET](https://cmake.org/cmake/help/latest/prop_tgt/LANG_VISIBILITY_PRESET.html)
- [VISIBILITY_INLINES_HIDDEN](https://cmake.org/cmake/help/latest/prop_tgt/VISIBILITY_INLINES_HIDDEN.html)
