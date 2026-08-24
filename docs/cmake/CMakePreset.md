# CMakePresets.json 해설

`CMakePresets.json`은 빌드 규칙을 정의하지 않는다. `CMakeLists.txt`에 정의된 빌드 규칙을 어떤 옵션과 경로로 실행할지 이름이 있는 프리셋으로 저장한다.

현재 프로젝트는 다음 세 프리셋을 제공한다.

| 단계               | 프리셋 이름 | 실행 명령                          |
| ------------------ | ----------- | ---------------------------------- |
| Configure/Generate | `dev`       | `cmake --preset dev`               |
| Build              | `build-dev` | `cmake --build --preset build-dev` |
| Test               | `test-dev`  | `ctest --preset test-dev`          |

## 현재 내용

```json
{
    "version": 3,
    "cmakeMinimumRequired": {
        "major": 3,
        "minor": 24,
        "patch": 0
    },
    "configurePresets": [
        {
            "name": "dev",
            "displayName": "ToolKit Development",
            "description": "Build the tools and tests",
            "binaryDir": "${sourceDir}/out/build/${presetName}",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Debug",
                "PSTK_BUILD_PACKET_TOOL": "ON",
                "BUILD_TESTING": "ON"
            }
        }
    ],
    "buildPresets": [
        {
            "name": "build-dev",
            "configurePreset": "dev",
            "configuration": "Debug"
        }
    ],
    "testPresets": [
        {
            "name": "test-dev",
            "configurePreset": "dev",
            "configuration": "Debug",
            "output": {
                "outputOnFailure": true
            }
        }
    ]
}
```

JSON 파일에서는 변수 이름의 underscore를 escape하지 않는다. 예를 들어 `CMAKE_BUILD_TYPE`이 올바르며 `CMAKE\_BUILD\_TYPE`은 올바른 JSON 문자열이 아니다.

## 루트 필드

### `version`

```json
"version": 3
```

`CMakePresets.json` 형식의 스키마 버전이다. 프로젝트 버전이나 설치된 CMake 버전을 의미하지 않는다. 사용할 수 있는 프리셋 필드는 이 스키마 버전에 의해 결정된다.

### `cmakeMinimumRequired`

```json
"cmakeMinimumRequired": {
  "major": 3,
  "minor": 24,
  "patch": 0
}
```

이 프리셋 파일을 사용하기 위한 최소 CMake 버전을 `3.24.0`으로 선언한다. 루트 `CMakeLists.txt`의 최소 버전과 맞춰 둔다.

## Configure preset: `dev`

```json
{
    "name": "dev",
    "displayName": "ToolKit Development",
    "description": "Build the tools and tests",
    "binaryDir": "${sourceDir}/out/build/${presetName}",
    "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "PSTK_BUILD_PACKET_TOOL": "ON",
        "BUILD_TESTING": "ON"
    }
}
```

다음 명령으로 사용한다.

```shell
cmake --preset dev
```

이 명령은 C++ 소스를 실제로 빌드하지 않는다. CMake 설정을 적용하고 빌드 시스템을 생성한다.

### `name`

```json
"name": "dev"
```

명령행에서 사용하는 기계 친화적인 프리셋 식별자다.

```shell
cmake --preset dev
```

### `displayName`

```json
"displayName": "ToolKit Development"
```

IDE나 CMake GUI에서 사람에게 보여주는 이름이다. 명령행에서는 `displayName`이 아니라 `name`을 사용한다.

### `description`

```json
"description": "Build the tools and tests"
```

프리셋의 목적을 설명하는 사용자용 문장이다. 빌드 동작에는 영향을 주지 않는다.

### `binaryDir`

```json
"binaryDir": "${sourceDir}/out/build/${presetName}"
```

Configure/Generate 결과를 저장할 디렉터리를 지정한다.

- `${sourceDir}`: `CMakePresets.json`이 위치한 프로젝트 루트
- `${presetName}`: 현재 configure preset 이름인 `dev`

현재 프로젝트에서는 다음 경로로 계산된다.

```text
/Users/home/playground/PrivateServerToolKit/out/build/dev
```

소스와 생성물을 분리하는 out-of-source build 구조다.

### `cacheVariables`

```json
"cacheVariables": {
  "CMAKE_BUILD_TYPE": "Debug",
  "PSTK_BUILD_PACKET_TOOL": "ON",
  "BUILD_TESTING": "ON"
}
```

Configure 단계에서 CMake cache에 전달할 값이다. 명령행의 `-D<변수>=<값>`과 같은 역할을 한다.

개념적으로 다음 명령에 해당한다.

```shell
cmake \
  -S . \
  -B out/build/dev \
  -DCMAKE_BUILD_TYPE=Debug \
  -DPSTK_BUILD_PACKET_TOOL=ON \
  -DBUILD_TESTING=ON
```

| 변수                        | 의미                                        |
| --------------------------- | ------------------------------------------- |
| `CMAKE_BUILD_TYPE=Debug`    | single-config generator에서 Debug 구성 사용 |
| `PSTK_BUILD_PACKET_TOOL=ON` | Packet Tool 하위 프로젝트 포함              |
| `BUILD_TESTING=ON`          | 테스트 target과 CTest 등록 활성화           |

## Build preset: `build-dev`

```json
{
    "name": "build-dev",
    "configurePreset": "dev",
    "configuration": "Debug"
}
```

다음 명령으로 실제 컴파일과 링크를 수행한다.

```shell
cmake --build --preset build-dev
```

### `configurePreset`

```json
"configurePreset": "dev"
```

어느 configure preset이 생성한 빌드 디렉터리를 사용할지 지정한다. 따라서 `build-dev`는 `dev`의 `out/build/dev`를 사용한다.

Configure를 한 번도 실행하지 않아 해당 디렉터리에 빌드 시스템이 없다면 build preset만으로는 정상적으로 빌드할 수 없다.

### `configuration`

```json
"configuration": "Debug"
```

Visual Studio처럼 한 빌드 트리에 Debug와 Release를 함께 제공하는 multi-config generator에서 Debug 구성을 선택한다. 개념적으로 `--config Debug`와 같다.

`CMAKE_BUILD_TYPE`과 `configuration`이 모두 Debug인 이유는 generator 종류에 따라 사용하는 설정이 다르기 때문이다.

| generator 종류                    | 주로 사용하는 설정 |
| --------------------------------- | ------------------ |
| Makefiles, Ninja                  | `CMAKE_BUILD_TYPE` |
| Visual Studio, Ninja Multi-Config | `configuration`    |

## Test preset: `test-dev`

```json
{
    "name": "test-dev",
    "configurePreset": "dev",
    "configuration": "Debug",
    "output": {
        "outputOnFailure": true
    }
}
```

다음 명령으로 등록된 테스트를 실행한다.

```shell
ctest --preset test-dev
```

### `configurePreset`과 `configuration`

Build preset과 마찬가지로 `dev`가 생성한 `out/build/dev`를 사용하고 multi-config 환경에서는 Debug 테스트 실행 파일을 선택한다.

### `outputOnFailure`

```json
"output": {
  "outputOnFailure": true
}
```

실패한 테스트가 출력한 표준 출력과 표준 오류를 표시한다. 다음 명령행 옵션과 같은 의도다.

```shell
ctest --test-dir out/build/dev --output-on-failure
```

## CTest 테스트 목록 확인

Configure가 생성한 test metadata만 확인하려면 `-N` 또는 `--show-only`를 사용한다. 이 옵션은 테스트를 실행하지 않는다.

```shell
ctest --preset test-dev -N
```

현재 Packet smoke test가 등록되어 있다면 다음 이름이 표시된다.

```text
Test #1: pstk.packet.api.version
Total Tests: 1
```

Preset을 사용하지 않고 build directory를 직접 지정할 수도 있다.

```shell
ctest --test-dir out/build/dev -N
```

또는 build directory로 이동한 뒤 실행한다.

```shell
cd out/build/dev
ctest -N
```

CTest는 기본적으로 현재 디렉터리의 `CTestTestfile.cmake`를 읽는다. 따라서 프로젝트의 source root에서 경로 지정 없이 다음 명령만 실행하면:

```shell
ctest -N
```

source root에 test metadata가 없기 때문에 등록된 테스트가 있어도 `Total Tests: 0`으로 보일 수 있다. `test-dev` preset은 연결된 `dev` configure preset의 `binaryDir`을 사용하므로 프로젝트 루트에서도 올바른 build directory를 선택한다.

테스트를 새로 추가하거나 `CMakeLists.txt`의 `add_test()`를 변경했다면 목록 확인 전에 configure를 다시 수행한다.

```shell
cmake --preset dev
ctest --preset test-dev -N
```

`-N`은 테스트 실행 파일을 실행하지 않지만 test metadata가 configure 과정에서 생성되어 있어야 한다.

## CTest 테스트 실행

등록된 전체 테스트는 다음 명령으로 실행한다.

```shell
ctest --preset test-dev
```

특정 테스트만 이름으로 선택하려면 `-R` 정규식 필터를 사용한다.

```shell
ctest --preset test-dev -R '^pstk\.packet\.api\.version$'
```

Preset 없이 동일한 build directory를 직접 지정할 수도 있다.

```shell
ctest \
  --test-dir out/build/dev \
  -R '^pstk\.packet\.api\.version$' \
  --output-on-failure
```

CTest는 테스트 실행 파일을 일반적으로 자동 빌드하지 않는다. 먼저 build preset으로 DLL과 smoke executable을 최신 상태로 만든 뒤 실행한다.

```shell
cmake --preset dev
cmake --build --preset build-dev
ctest --preset test-dev
```

## 전체 실행 흐름

```shell
cmake --preset dev
cmake --build --preset build-dev
ctest --preset test-dev
```

```text
cmake --preset dev
  -> CMakeLists.txt 해석
  -> Packet Tool 및 테스트 옵션 활성화
  -> out/build/dev에 빌드 시스템 생성

cmake --build --preset build-dev
  -> out/build/dev의 target 컴파일
  -> DLL과 테스트 실행 파일 링크

ctest --preset test-dev
  -> out/build/dev에 등록된 Debug 테스트 실행
  -> 성공/실패와 실행 시간 집계
  -> 실패 시 상세 출력 표시
```

## 프리셋 확인 명령

```shell
cmake --list-presets
cmake --build --list-presets
ctest --list-presets

# 등록된 테스트 이름만 확인
ctest --preset test-dev -N
```

각 명령은 configure, build, test 종류에 해당하는 프리셋을 보여준다. 세 종류에서 같은 이름을 사용해도 명령 종류가 다르기 때문에 충돌하지 않지만, 현재 프로젝트는 역할을 바로 구분할 수 있도록 `dev`, `build-dev`, `test-dev`를 사용한다.

## 복기 체크

다음을 설명할 수 있으면 현재 프리셋을 이해한 것이다.

1. Configure와 Build를 별도 명령으로 실행하는 이유
2. `${sourceDir}`와 `${presetName}`이 실제 어떤 경로로 변환되는지
3. `cacheVariables`와 명령행 `-D`의 관계
4. `CMAKE_BUILD_TYPE`과 `configuration`이 모두 존재하는 이유
5. `outputOnFailure`가 테스트 실패 시 무엇을 바꾸는지
6. source root의 `ctest -N`과 `ctest --preset test-dev -N` 결과가 다를 수 있는 이유
7. `ctest -N`이 테스트를 빌드하거나 실행하지 않는 이유

## 공식 참고 자료

- [CMake Presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
- [CMake command-line tool](https://cmake.org/cmake/help/latest/manual/cmake.1.html)
- [CTest command-line tool](https://cmake.org/cmake/help/latest/manual/ctest.1.html)
