# PrivateServerToolKit

PrivateServer에서 공통으로 사용할 native tool과 C ABI 형태의 C++ 계약을 제공하는 프로젝트다.

## 요구 사항

- Git
- CMake 3.24 이상
- C11과 C++17을 지원하는 compiler
- 커밋 hook을 사용할 경우 `clang-format`
- [vcpkg](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started)

의존성은 root [`vcpkg.json`](vcpkg.json)의 manifest와 `builtin-baseline`으로 관리한다. 현재 `tests` feature가 GoogleTest를 개발 의존성으로 추가하며, header-only `PSTK::Common` 자체에는 runtime 의존성이 없다.

## vcpkg 설정

macOS/Linux에서는 vcpkg를 clone하고 bootstrap한다.

```sh
git clone https://github.com/microsoft/vcpkg.git /path/to/vcpkg
cd /path/to/vcpkg
./bootstrap-vcpkg.sh

export VCPKG_ROOT="/path/to/vcpkg"
export PATH="$VCPKG_ROOT:$PATH"
```

환경 변수를 계속 사용하려면 shell profile(`~/.zshrc`, `~/.bashrc` 등)에 `export` 두 줄을 추가한다. macOS에서 GoogleTest 설치 중 `pkg-config`를 찾지 못하면 다음 도구를 설치한다.

```sh
brew install pkg-config
```

Windows PowerShell에서는 다음과 같이 설정한다.

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\path\to\vcpkg
cd C:\path\to\vcpkg
.\bootstrap-vcpkg.bat

$env:VCPKG_ROOT = "C:\path\to\vcpkg"
$env:PATH = "$env:VCPKG_ROOT;$env:PATH"
```

IDE에서 CMake preset을 사용할 때도 IDE process에 `VCPKG_ROOT`가 전달되어야 한다.

## Build와 Test

`dev` preset은 vcpkg toolchain과 manifest의 `tests` feature를 활성화한다. 첫 configure에서 필요한 package가 자동으로 설치되므로 별도의 `vcpkg install` 명령은 필요하지 않다.

```sh
cmake --preset dev
cmake --build --preset build-dev
ctest --preset test-dev
```

Generated C# packet code의 준비, Debug/Release 빌드와 xUnit 테스트는 [.NET 가이드](src/dotnet/README.md)를 참고한다. 위 CTest 명령은 .NET 테스트를 실행하지 않는다.

이미 빌드한 CLI의 단일 파일·중첩 디렉터리 입력과 C++/C# 생성 설정을 확인하려면 [CLI smoke 가이드](src/tools/packet/cli/tests/README.md)를 참고한다. 이 검증은 `cmake -P`로 직접 실행하며 CTest나 기본 빌드에 등록하지 않는다.

toolchain 또는 manifest 설정을 변경한 뒤 CMake cache를 새로 구성하려면 다음 명령을 사용한다.

```sh
cmake --fresh --preset dev
```

vcpkg manifest와 CMake 연동에 대한 자세한 내용은 [Manifest mode](https://learn.microsoft.com/en-us/vcpkg/concepts/manifest-mode)와 [CMake integration](https://learn.microsoft.com/en-us/vcpkg/users/buildsystems/cmake-integration)을 참고한다.

## Release 배포

프로젝트 루트에서 다음 명령을 실행한다.

```sh
cmake --preset release
cmake --build --preset build-release
```

`build-release`는 `install` target을 실행하므로 빌드 후 `out/build/dist/release/pstk-packet/`에 CLI, runtime shared library, 기본 INI와 C++/C# codec support를 모은다. 테스트 실행과 CLI를 통한 packet 생성은 포함하지 않으며, `build-dev`에는 설치가 묶여 있지 않다.

직접 `cmake --install`을 실행하거나 배포 경로를 바꾸는 방법은 [CMake install 가이드](docs/cmake/CMakeInstall.md)를, `installDir`와 `targets: ["install"]`의 연결은 [preset 해설](docs/cmake/CMakePreset.md)을 참고한다.

## Pre-commit formatting

저장소의 hook을 활성화하면 커밋 대상으로 stage한 `.cpp`와 `.h` 파일을 root `.clang-format`으로 자동 포맷하고 다시 stage한다.

```sh
git config core.hooksPath .githooks
```

macOS에서는 `PATH`에 `clang-format`이 없으면 `xcrun`으로 Command Line Tools의 실행 파일을 찾는다. 다른 위치를 사용하려면 실행 파일 경로를 `CLANG_FORMAT_BIN` 환경 변수로 전달할 수 있다. 부분 stage한 파일에 unstaged 변경이 함께 있으면 의도하지 않은 코드가 커밋되지 않도록 hook이 중단된다.
