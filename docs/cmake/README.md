# PrivateServerToolKit CMake 가이드

- [`CMakeLists.md`](CMakeLists.md): 루트 `CMakeLists.txt`에 작성한 명령의 의미
- [`CMakePreset.md`](CMakePreset.md): `CMakePresets.json`에 작성한 프리셋의 의미
- [`PacketTarget.md`](PacketTarget.md): Packet DLL target, public include, export header와 smoke test 구성
- [`SharedLibraryBinaryModel.md`](../cpp/SharedLibraryBinaryModel.md): shared library의 compile-link-load-call 흐름, symbol visibility와 inline 모델

## CMake가 담당하는 것

CMake는 `CMakeLists.txt`에 작성한 프로젝트와 target 정의를 읽고, 현재 환경에 맞는 빌드 시스템을 생성한다.

```text
CMakeLists.txt + CMakePresets.json
              |
              | configure / generate
              v
       out/build/dev
              |
              | build
              v
  DLL, 실행 파일, 테스트 실행 파일
              |
              | test
              v
       CTest 결과 집계
```

현재 프로젝트의 기본 흐름은 세 단계로 나뉜다.

1. Configure/Generate: 프로젝트 옵션을 확정하고 빌드 시스템을 생성한다.
2. Build: C++ 소스를 컴파일하고 target을 링크한다.
3. Test: 빌드된 테스트 실행 파일을 실행하고 결과를 집계한다.

## 배포 기본 방향: shared library SDK

PrivateServerToolKit은 consumer의 빌드 시스템에 소스 프로젝트로 편입되는 방식보다, 빌드가 완료된 shared library SDK로 배포하는 것을 기본 방향으로 삼는다. Windows에서는 DLL과 import library, public header가 SDK의 핵심 산출물이다.

```text
PSTK SDK
├── include/                 public header
├── bin/                     runtime shared library (Windows DLL)
├── lib/                     link library (Windows import library)
└── lib/cmake/PSTK/          CMake consumer용 package metadata
```

모든 consumer는 동일한 public header와 binary library를 사용하며, 빌드 시스템에 따라 이를 연결하는 방법만 달라진다.

- Visual Studio/MSBuild consumer: include 경로, import library, runtime DLL 경로를 직접 설정하거나 추후 제공할 property sheet를 사용한다.
- CMake consumer: 설치된 SDK의 package metadata를 `find_package(PSTK CONFIG REQUIRED)`로 불러오고 `PSTK::Packet` target을 링크한다.

```cmake
find_package(PSTK CONFIG REQUIRED)

add_executable(Consumer main.cpp)

target_link_libraries(
    Consumer
    PRIVATE
        PSTK::Packet
)
```

`PSTK::Packet`은 별도의 라이브러리가 아니라 SDK에 포함된 header 경로와 platform별 link 정보를 캡슐화한 imported target이다. 따라서 CMake package 파일은 SDK의 새로운 배포 형식이 아니라 CMake consumer를 위한 편의 계층이다.

소스 트리의 CMake 구성은 SDK를 빌드하고 테스트하며 설치 가능한 형태로 패키징할 책임을 가진다. Consumer에게 ToolKit 소스 트리나 동일한 CMake project 구성을 요구하지 않는다.

현재 구성은 build tree 내부의 Packet DLL과 테스트 target을 만드는 단계다. 외부 consumer 배포를 완료하려면 이후 install rule, exported target, `PSTKConfig.cmake`와 version 파일을 추가하고 설치된 SDK만으로 consumer build가 가능한지 검증해야 한다.

## 최초 실행 순서

프로젝트 루트에서 다음 순서로 실행한다.

```shell
cmake --preset dev
cmake --build --preset build-dev
ctest --preset test-dev
```

각 명령의 역할은 다음과 같다.

| 명령                               | 역할               | 주요 결과                          |
| ---------------------------------- | ------------------ | ---------------------------------- |
| `cmake --preset dev`               | Configure/Generate | `out/build/dev`에 빌드 시스템 생성 |
| `cmake --build --preset build-dev` | Build              | 활성화된 target 컴파일 및 링크     |
| `ctest --preset test-dev`          | Test               | 등록된 테스트 실행 및 결과 집계    |

`cmake --preset dev`만 실행하면 빌드 준비까지만 완료된다. 실제 DLL이나 테스트 실행 파일을 만들려면 `cmake --build --preset build-dev`를 별도로 실행해야 한다.

## 변경 종류별 다시 실행할 명령

### C++ 소스만 변경한 경우

이미 configure가 완료됐다면 다시 빌드하면 된다.

```shell
cmake --build --preset build-dev
ctest --preset test-dev
```

### `CMakeLists.txt` 또는 프리셋을 변경한 경우

설정을 명확하게 다시 적용한 뒤 빌드한다.

```shell
cmake --preset dev
cmake --build --preset build-dev
ctest --preset test-dev
```

### 깨끗한 빌드가 필요한 경우

`out/build/dev`는 생성 결과물이므로 삭제 후 다시 configure할 수 있다. 소스 파일이나 설정 파일은 삭제하지 않는다.

```shell
cmake --preset dev
cmake --build --preset build-dev
```

## CMake 파일의 책임

```text
CMakeLists.txt
  - 프로젝트와 target의 구조를 정의
  - 하위 디렉터리를 빌드에 포함
  - 라이브러리, 실행 파일, 테스트 관계 정의

CMakePresets.json
  - 자주 사용하는 configure/build/test 옵션을 이름으로 저장
  - 빌드 디렉터리와 Debug/Release 설정 선택
  - 명령행 입력을 짧고 재현 가능하게 유지
```

프리셋은 빌드 규칙을 대신하지 않는다. `CMakeLists.txt`가 정의한 규칙을 어떤 설정으로 실행할지 저장한다.

## CTest와 테스트 프레임워크의 관계

```text
테스트 프레임워크 또는 직접 작성한 테스트 코드
                    |
                    v
              테스트 실행 파일
                    |
                    | CTest가 실행
                    v
              성공/실패 결과 집계
```

- `CTest`: 테스트 실행 파일을 실행하고 종료 코드와 출력을 집계한다.
- GoogleTest/Catch2: 테스트 실행 파일 내부에서 테스트 사례를 작성하고 수행하는 선택적 프레임워크다.
- 테스트 실행 파일: 프로젝트가 직접 빌드하는 프로그램이다.

CTest는 프로젝트가 생성하는 실행 파일이 아니라 CMake와 함께 제공되는 별도 명령행 도구다.

## 자주 사용하는 확인 명령

```shell
# 사용할 수 있는 configure 프리셋 확인
cmake --list-presets

# 사용할 수 있는 build 프리셋 확인
cmake --build --list-presets

# 사용할 수 있는 test 프리셋 확인
ctest --list-presets

# 등록된 테스트 이름만 확인
ctest --preset test-dev -N

# 실패한 테스트의 출력 확인
ctest --preset test-dev --output-on-failure
```

`test-dev` 프리셋에는 이미 `outputOnFailure`가 설정되어 있으므로 마지막 옵션은 생략해도 같은 의도로 동작한다.

## 공식 참고 자료

- [CMake User Interaction Guide](https://cmake.org/cmake/help/latest/guide/user-interaction/index.html)
- [CMake Presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
- [CTest](https://cmake.org/cmake/help/latest/manual/ctest.1.html)
