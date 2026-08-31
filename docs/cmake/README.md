# PrivateServerToolKit CMake 가이드

- [`CMakeLists.md`](CMakeLists.md): 루트 `CMakeLists.txt`에 작성한 명령의 의미
- [`CMakePreset.md`](CMakePreset.md): `CMakePresets.json`에 작성한 프리셋의 의미
- [`CMakeInstall.md`](CMakeInstall.md): `install()` 규칙, CLI 배포 구성과 설치 명령
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

## Packet 배포: CLI와 언어별 support

현재 Packet 배포물은 미리 빌드한 `pstk-packet` CLI와 CLI 실행에 필요한 shared library, 기본 INI, generated code용 support다. Consumer는 CLI로 packet source를 생성하고, 생성된 source와 대상 언어의 support를 자신의 빌드 도구로 컴파일한다.

- `bin/`: CLI, Packet shared library, `packet.ini`
- `include/`: generated C++ code가 사용하는 common header와 codec support
- `support/csharp/`: generated C# code와 함께 컴파일하는 codec support

Generated packet을 사용하는 것만으로 Packet shared library를 링크할 필요는 없다. Shared library는 compiler API를 호출하는 CLI에 필요하며, generated-code consumer 경계는 [ADR 0005](../adr/0005-generated-code-consumer-boundary.md)를 따른다.

현재 배포물은 compiler API용 binary SDK가 아니므로 Windows import library, compiler API header, CMake exported target과 `PSTKConfig.cmake`는 포함하지 않는다. 저장소 내부의 `PSTK::Packet` target과 외부에서 `find_package()`로 가져오는 installed target을 구분한다.

프로젝트 루트에서 Release 빌드와 설치를 함께 실행한다.

```shell
cmake --preset release
cmake --build --preset build-release
```

배포 폴더는 `out/build/dist/release/pstk-packet/`다. 이 명령은 테스트나 CLI를 실행하지 않는다. 설치 규칙과 직접 `cmake --install`을 사용하는 방법은 [CMake install 가이드](CMakeInstall.md), preset에 설치를 묶은 방법은 [preset 해설](CMakePreset.md)을 참고한다.

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
  - install()로 배포 대상과 prefix 아래의 배치 경로 정의

CMakePresets.json
  - 자주 사용하는 configure/build/test 옵션을 이름으로 저장
  - 빌드 디렉터리와 Debug/Release 설정 선택
  - installDir로 설치 prefix, build preset의 targets로 실행할 target 선택
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
