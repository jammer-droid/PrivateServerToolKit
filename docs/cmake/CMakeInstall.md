# CMake install과 Packet 배포

`install()` 규칙은 빌드 산출물과 공개 파일을 하나의 배포 폴더에 모은다.
시스템 전역 경로 설치가 필수라는 뜻은 아니며, 설치 기준 폴더는 Release
preset의 `installDir` 또는 명령행의 `--prefix`로 정한다. 문법은
[CMake install command](https://cmake.org/cmake/help/v3.24/command/install.html),
preset 경로는 [CMake Presets manual](https://cmake.org/cmake/help/v3.24/manual/cmake-presets.7.html)을
참고한다.

현재 범위는 Packet CLI 배포이며 CMake package export나 compiler SDK 전체가
아니다.

## 현재 설정

다음 명령은 repository root에서 실행한다.

Release configure preset은 다음 경로를 사용한다.

```text
binaryDir: out/build/release
installDir: out/build/dist/release/pstk-packet
```

`build-release`는 `install` target을 기본 target으로 선택한다.

```shell
cmake --preset release
cmake --build --preset build-release
```

프리셋에 `installDir`를 새로 추가하거나 변경했다면 먼저
`cmake --preset release`로 설정을 다시 적용한다. Build 명령만으로는 변경된
configure preset의 값이 적용되지 않는다.

이미 빌드된 Release 결과를 설정된 `installDir`에 다시 설치하려면 다음을
사용한다.

```shell
cmake --install out/build/release --config Release
```

설치 위치를 한 번만 바꾸려면 `--prefix`로 덮어쓴다. 다음 예시는 macOS/Linux
shell 문법이다.

```shell
cmake --install out/build/release \
  --config Release \
  --prefix "$PWD/out/build/dist/custom/pstk-packet"
```

`cmake --install`은 지정된 build directory의 설치 스크립트를 실행하며,
직접 빌드하지 않는다. 자세한 명령행 동작은 [CMake install a
project](https://cmake.org/cmake/help/v3.24/manual/cmake.1.html#install-a-project)를
참고한다.

## `TARGETS`와 `FILES`

`CMakeLists.txt`의 `install(...)`은 configure/generate 단계에 설치 규칙을
등록한다. 그 시점에 파일을 복사하거나 source/build 파일을 이동·삭제하지
않으며, 실제 배치는 `install` target 또는 `cmake --install` 실행 때 일어난다.
설치 후에도 원본 source와 build 산출물은 남는다.

`install(TARGETS ...)`에는 CMake target 이름을 전달한다. 소스 파일을
복사하는 명령이 아니라 target이 빌드한 실행 파일이나 라이브러리를 설치하는
규칙이다.

```cmake
install(
    TARGETS pstk_packet
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION bin
)
```

현재 Packet target 규칙은 [src/tools/packet/CMakeLists.txt](../../src/tools/packet/CMakeLists.txt)에
있다.

`install(FILES ...)`에는 현재 소스 트리의 공개 파일을 전달한다. 지정한
파일의 `basename`을 유지한 채 `DESTINATION` 아래에 복사한다. `FILES`는
소스 트리 파일로만 제한되는 문법은 아니며, 현재 배포에서는 공개 source
header와 support source를 지정한다.

```cmake
install(
    FILES
        include/pstk/TkResult.h
    DESTINATION include/pstk
)
```

Common header 규칙은 [src/common/CMakeLists.txt](../../src/common/CMakeLists.txt)에
있다. 이 예시의 상대 `FILES` 경로는 해당 `CMakeLists.txt`의 source directory를
기준으로 해석된다.

`DESTINATION`이 절대 경로가 아니면 설치 기준 폴더에 이어 붙는다. 따라서
같은 install 규칙을 유지한 채 `--prefix`만 바꾸어 다른 배포 폴더를 만들 수
있다. 이 경로 해석과 install 단계는 [CMake install
command](https://cmake.org/cmake/help/v3.24/command/install.html)의
설명을 따른다.

### 산출물 종류

| 종류 | 의미 | 이 프로젝트의 사용 |
| --- | --- | --- |
| `RUNTIME` | 실행 파일, Windows DLL | CLI와 Windows runtime DLL |
| `LIBRARY` | macOS dylib, Linux shared library | `pstk_packet` shared library |
| `ARCHIVE` | 정적 라이브러리, Windows DLL import `.lib` | 이 배포에서는 제외 |

Packet shared library 규칙에는 `RUNTIME`과 `LIBRARY`만 명시한다. 따라서
Windows import `.lib`, 내부 static library와 같은 compiler 구현 산출물은 이
배포 폴더에 포함하지 않는다.

## 설치 결과

현재 Release install prefix를 기준으로 결과는 다음과 같다.

```text
pstk-packet/
├── bin/
│   ├── pstk-packet[.exe]
│   ├── <platform packet shared library>
│   └── packet.ini
├── include/
│   └── pstk/
│       ├── TkByteView.h
│       ├── TkDiagnostic.h
│       ├── TkResult.h
│       └── packet/
│           └── TkPacketCodecSupport.h
└── support/
    └── csharp/
        └── TkPacketCodecSupport.cs
```

대표적인 shared library 파일명은 macOS의 `libpstk_packet.dylib`와 Windows의
`pstk_packet.dll`이다. 실제 파일명은 플랫폼과 CMake generator가 결정한다.

macOS에서는 설치된 CLI에 `@loader_path` install RPATH를 설정한다. 따라서
CLI가 있는 `bin`을 기준으로 sibling shared library를 찾을 수 있다. 이
target property의 의미는 [INSTALL_RPATH property](https://cmake.org/cmake/help/latest/prop_tgt/INSTALL_RPATH.html)를
참고한다.

## Consumer 경계

이 배포는 schema를 읽어 source를 생성하는 CLI와 생성된 source가 사용할
언어별 support를 제공한다.

- CLI consumer는 `bin/pstk-packet`과 같은 위치의 shared library를 사용한다.
- 생성된 C++ consumer는 배포 루트의 `include`를 include 검색 경로로 추가하고,
  그 아래 `pstk/...` Common header와 Packet codec support header를 include한다.
- 생성된 C# consumer는 `support/csharp/TkPacketCodecSupport.cs`를 자신의
  .NET 프로젝트에 포함한다.
- 생성된 packet을 사용하는 runtime consumer는 generator CLI나
  `pstk_packet` shared library를 링크할 필요가 없다.

`packet.ini`는 예제 설정만 제공한다. schema JSON은 배포에 포함하지 않는다.
현재 기본 설정의 `input=./schemas`와 같은 상대 경로는 실행 working directory가
아니라 INI 파일이 있는 directory를 기준으로 해석되므로, 기본 설정을 그대로
사용하려면 `bin/schemas`를 별도로 준비하거나 INI의 input을 수정한다.

다음 항목은 설치 결과에 포함하지 않는다.

- `TkPacketTool.h`와 generated export header
- Windows import `.lib`
- 내부 static library와 CMake export/package config
- schema 또는 generated packet source

따라서 이것은 compiler API를 직접 링크하는 CMake SDK가 아니다. CLI를 통한
전처리와 생성된 코드 소비를 분리한 배포다.

## Build preset과 target 선택 주의

`build-release`는 `install` target을 기본으로 선택한다. 현재 CMake 기본값에서는
이 target이 기본 `all` dependency를 먼저 빌드하고 설치한다. 이 동작은
[`CMAKE_SKIP_INSTALL_ALL_DEPENDENCY`](https://cmake.org/cmake/help/v3.24/variable/CMAKE_SKIP_INSTALL_ALL_DEPENDENCY.html)의
기본값과 관련되며 테스트나 Packet CLI를 실행하지는 않는다.

명령행에 `--target`을 지정하면 preset의 기본 target을 대신한다. 예를 들어
`cmake --build --preset build-release --target pstk_packet_cli`는 설치 없이 CLI
target만 빌드한다.

`cmake --install`에는 CMake install preset을 선택하는 `--preset` 명령이 없다.
따라서 build directory를 직접 전달하며, 프리셋 흐름은 [CMakePreset.md](CMakePreset.md)를
참고한다.

## CLI 설정 적용 확인

설치 폴더의 CLI가 INI 설정을 반영하는지 확인할 때는 기존
[CLI smoke 가이드](../../src/tools/packet/cli/tests/README.md)를 재사용한다.
이 검증은 배포된 CLI 경로를 직접 전달하며 CTest나 기본 build target에
자동 등록되지 않는다. generated source compile과 .NET 실행은 이 문서의
검증 범위가 아니다.

macOS 설치 루트에서 CLI를 실행할 때는 명시적으로 설정 파일을 전달한다.

```shell
bin/pstk-packet bin/packet.ini
```

## 범위

현재 배포 확인 대상은 macOS arm64와 Windows x64다. 이 문서는 설치 layout과
명령을 설명할 뿐 특정 플랫폼의 설치·실행 완료를 주장하지 않는다. Linux
package, installer, 자동 Release upload와 consumer별 build/CI hook은 후속 범위다.
