# 루트 CMakeLists.txt 해설

루트 `CMakeLists.txt`는 `PrivateServerToolKit` 전체 빌드의 진입점이다. 현재 단계에서는 프로젝트 정보, 선택 옵션, 테스트 지원과 Packet Tool 하위 디렉터리 연결을 정의한다.

## 현재 내용

```cmake
cmake_minimum_required(VERSION 3.24)

project(
    PrivateServerToolKit
    VERSION 0.1.0
    DESCRIPTION "Cross-platform native tools for PrivateServer"
    LANGUAGES CXX
)

option(
    PSTK_BUILD_PACKET_TOOL
    "Build the packet tool"
    ON
)

include(CTest)

if(PSTK_BUILD_PACKET_TOOL)
    add_subdirectory(tools/packet)
endif()
```

## `cmake_minimum_required`

```cmake
cmake_minimum_required(VERSION 3.24)
```

프로젝트를 구성하는 데 필요한 최소 CMake 버전을 선언한다.

- CMake 3.24 이상이면 구성을 계속한다.
- 더 낮은 버전이면 CMake가 오류를 보고하고 중단한다.
- 해당 버전까지 도입된 CMake policy를 프로젝트 기준으로 확정하는 역할도 한다.

`CMakePresets.json`의 `cmakeMinimumRequired`와 같은 `3.24.0`을 사용해 프로젝트와 프리셋의 최소 요구사항을 맞춘다.

## `project`

```cmake
project(
    PrivateServerToolKit
    VERSION 0.1.0
    DESCRIPTION "Cross-platform native tools for PrivateServer"
    LANGUAGES CXX
)
```

프로젝트의 기본 정보를 CMake에 등록한다.

| 항목                   | 의미                                    |
| ---------------------- | --------------------------------------- |
| `PrivateServerToolKit` | 프로젝트 이름                           |
| `VERSION 0.1.0`        | 프로젝트 버전                           |
| `DESCRIPTION`          | 프로젝트 설명                           |
| `LANGUAGES CXX`        | C++ 컴파일러가 필요한 프로젝트임을 선언 |

이 명령 이후 CMake는 현재 환경에서 사용할 C++ 컴파일러를 탐색한다. 버전 정보는 `PROJECT_VERSION`, `PROJECT_VERSION_MAJOR` 같은 CMake 변수로도 사용할 수 있다.

## `option`

```cmake
option(
    PSTK_BUILD_PACKET_TOOL
    "Build the packet tool"
    ON
)
```

Configure 단계에서 사용자가 켜거나 끌 수 있는 boolean cache 변수를 만든다.

일반 형식은 다음과 같다.

```cmake
option(<변수 이름> <사용자 설명> <최초 기본값>)
```

현재 값의 의미는 다음과 같다.

| 항목        | 값                       |
| ----------- | ------------------------ |
| 변수 이름   | `PSTK_BUILD_PACKET_TOOL` |
| 설명        | `Build the packet tool`  |
| 최초 기본값 | `ON`                     |

`ON`은 Packet Tool을 즉시 컴파일하라는 뜻이 아니다. 이번 CMake 구성에 Packet Tool 하위 프로젝트를 포함할지를 결정한다.

프리셋을 사용하지 않는다면 명령행에서 다음처럼 변경할 수 있다.

```shell
cmake -S . -B out/build/no-packet -DPSTK_BUILD_PACKET_TOOL=OFF
```

같은 빌드 디렉터리를 다시 사용하면 cache에 저장된 값이 유지된다. `option()`의 기본값은 해당 cache 값이 아직 없을 때 적용되는 최초값이다.

## `include(CTest)`

```cmake
include(CTest)
```

CMake 프로젝트에 CTest 지원을 추가한다. 주요 결과는 다음과 같다.

- `BUILD_TESTING` 옵션을 제공한다.
- `BUILD_TESTING=ON`일 때 테스트 등록을 활성화한다.
- 이후 `add_test()`로 등록한 테스트를 `ctest`가 찾을 수 있게 한다.

이 명령이 프로젝트 안에 `ctest` 실행 파일을 만드는 것은 아니다. `ctest`는 CMake와 함께 설치되는 별도 프로그램이다.

CTest와 테스트 프레임워크도 구분해야 한다.

```text
GoogleTest 또는 직접 작성한 테스트
                |
                v
         테스트 실행 파일
                |
                | CTest가 실행
                v
          결과와 종료 코드 집계
```

테스트를 제외하려면 configure 시 `BUILD_TESTING=OFF`로 설정한다.

```shell
cmake -S . -B out/build/no-tests -DBUILD_TESTING=OFF
```

현재 `dev` 프리셋은 `BUILD_TESTING=ON`을 명시한다.

## `if`와 `add_subdirectory`

```cmake
if(PSTK_BUILD_PACKET_TOOL)
    add_subdirectory(tools/packet)
endif()
```

`PSTK_BUILD_PACKET_TOOL` 값에 따라 Packet Tool을 전체 빌드 구성에 포함한다.

```text
PSTK_BUILD_PACKET_TOOL=ON
    -> tools/packet/CMakeLists.txt를 읽음
    -> Packet Tool target을 전체 빌드에 포함

PSTK_BUILD_PACKET_TOOL=OFF
    -> 하위 CMakeLists.txt를 읽지 않음
    -> Packet Tool target도 생성되지 않음
```

`add_subdirectory()`는 디렉터리를 단순히 표시하는 명령이 아니다. 지정한 디렉터리의 `CMakeLists.txt`를 처리하고, 그 안에 정의된 target을 현재 빌드 시스템에 추가한다.

현재 `tools/packet/CMakeLists.txt`에 실제 target이 없다면 Packet Tool 옵션이 `ON`이어도 아직 DLL은 생성되지 않는다.

## 루트에 아직 작성하지 않은 설정

다음 설정은 각 target의 요구사항이므로 루트 전역 설정보다 `tools/packet/CMakeLists.txt`의 Packet target에 지정할 예정이다.

```cmake
add_library(pstk_packet SHARED ...)

target_compile_features(
    pstk_packet
    PUBLIC
    cxx_std_17
)
```

- `add_library(... SHARED ...)`: Windows DLL, Linux `.so`, macOS `.dylib` target 정의
- `target_compile_features(... cxx_std_17)`: Packet target이 요구하는 최소 C++ 표준 선언

전역 `CMAKE_CXX_STANDARD`나 `BUILD_SHARED_LIBS`에 의존하지 않고 target별 요구사항을 명시하면, 앞으로 추가될 다른 툴과 내부 라이브러리의 설정이 불필요하게 결합되지 않는다.

## 복기 체크

다음을 설명할 수 있으면 현재 루트 설정을 이해한 것이다.

1. `option()`의 `ON`이 즉시 컴파일을 의미하지 않는 이유
2. `include(CTest)`와 GoogleTest의 역할 차이
3. `add_subdirectory()`가 어떤 파일을 읽는지
4. Packet DLL 설정을 루트가 아닌 Packet target에 둘 이유

## 공식 참고 자료

- [cmake_minimum_required](https://cmake.org/cmake/help/latest/command/cmake_minimum_required.html)
- [project](https://cmake.org/cmake/help/latest/command/project.html)
- [option](https://cmake.org/cmake/help/latest/command/option.html)
- [add_subdirectory](https://cmake.org/cmake/help/latest/command/add_subdirectory.html)
- [CTest module](https://cmake.org/cmake/help/latest/module/CTest.html)
