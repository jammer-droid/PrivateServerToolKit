# .NET build와 packet codec test

이 디렉터리는 ToolKit의 .NET 프로젝트를 둔다. 현재 `tests/packet`은 생성된 C# packet DTO와 codec을 `net8.0`으로 빌드하고 xUnit/VSTest로 검증한다. SDK 배포나 Godot consumer 연결을 제공하는 디렉터리는 아니다.

Generated C# source는 C# codec support와 함께 컴파일하며 ToolKit native DLL, `TkResult` 또는 `TkDiagnostic`을 요구하지 않는다. 언어별 의존성 경계는 [Generated-code consumer 계약](../docs/adr/0005-generated-code-consumer-boundary.md)을 따른다.

## 준비 사항

- 테스트 source를 생성할 native build 환경의 CMake, C++ compiler와 vcpkg 설정: [루트 README](../README.md)
- `net8.0`을 빌드할 .NET SDK와 테스트를 실행할 .NET 8 runtime

[.NET 8 SDK](https://dotnet.microsoft.com/en-us/download/dotnet/8.0)를 설치하면 필요한 runtime도 함께 설치된다. 다른 SDK를 사용하는 환경에서도 .NET 8 runtime이 있는지 확인한다.

```sh
dotnet --list-sdks
dotnet --list-runtimes
```

현재 `global.json`으로 SDK 버전을 고정하지 않는다. C# 의존성은 [PstkPacketTests.csproj](tests/packet/PstkPacketTests.csproj)의 NuGet `PackageReference`로 관리하며 native 의존성 관리 도구인 vcpkg와 별개다. xUnit v3, VSTest adapter와 `Microsoft.NET.Test.Sdk`의 버전은 해당 파일에 명시돼 있다.

## 파일 역할

```text
dotnet/
  Directory.Build.props                 Debug/Release generated source 경로
  tests/packet/
    PstkPacketTests.csproj               framework, package와 compile input
    PstkPacketTests.cs                   xUnit 테스트
```

- CMake build step이 같은 `AllTypes` descriptor에서 `.generated.h`와 `.generated.cs`를 함께 생성한다.
- `.NET` project는 이미 생성된 `.cs`와 [canonical C# codec support](../tools/packet/support/csharp/TkPacketCodecSupport.cs)를 `Compile Include`로 직접 컴파일한다. 테스트 디렉터리에 복사하지 않는다.
- `.NET`은 CMake를 호출하지 않고, CMake/CTest도 `.NET`을 호출하지 않는다. Native build로 입력 파일을 준비한 뒤 `dotnet` 명령을 실행한다. CTest 실행은 파일 생성의 선행 조건이 아니다.

## 빌드와 테스트

아래 명령은 모두 **저장소 루트**에서 실행한다. 첫 `dotnet build`는 NuGet package restore도 수행하므로 package 다운로드가 가능한 환경이 필요하다.

### Debug

```sh
cmake --preset dev
cmake --build --preset build-dev --target pstk_packet_generated_codec_tests

dotnet build dotnet/tests/packet/PstkPacketTests.csproj -c Debug
dotnet test dotnet/tests/packet/PstkPacketTests.csproj -c Debug --no-build
```

### Release

```sh
cmake --preset release
cmake --build --preset build-release --target pstk_packet_generated_codec_tests

dotnet build dotnet/tests/packet/PstkPacketTests.csproj -c Release
dotnet test dotnet/tests/packet/PstkPacketTests.csproj -c Release --no-build
```

`dotnet test --no-build`는 앞에서 빌드한 테스트를 실행한다. C# source나 compile input을 변경했다면 먼저 다시 빌드하거나 `--no-build`를 생략한다. 이 경우에도 CMake source 생성은 별도로 진행해야 한다. 옵션의 의미는 [dotnet test 문서](https://learn.microsoft.com/en-us/dotnet/core/tools/dotnet-test)를 참고한다.

C++ golden wire 검증도 함께 실행하려면 대응하는 focused CTest를 사용한다. 이 명령은 C# 테스트를 실행하지 않는다.

```sh
ctest --preset test-dev -R '^pstk\.packet\.generated-codec\.'
ctest --preset test-release -R '^pstk\.packet\.generated-codec\.'
```

## 구성별 경로와 생성물

[Directory.Build.props](Directory.Build.props)가 `Configuration`에 맞춰 `PstkGeneratedSourceDir`를 지정한다.

| .NET 구성 | CMake configure / build preset | Generated source 디렉터리 |
| --- | --- | --- |
| Debug | `dev` / `build-dev` | `out/build/dev/tools/packet/tests/generated` |
| Release | `release` / `build-release` | `out/build/release/tools/packet/tests/generated` |

각 경로의 `AllTypes.generated.cs`가 `.csproj`의 compile input이다. Debug에서 만든 파일로 Release 경로가 채워지는 것은 아니므로 구성별로 native build를 진행한다.

`Directory.Build.props`는 하위 프로젝트에 적용하는 공통 MSBuild 설정이다. Generated source 경로는 `MSBuildThisFileDirectory`, 즉 props 파일 위치를 기준으로 계산하고, `.csproj`에 직접 적은 상대 include 경로는 `.csproj` 위치를 기준으로 해석한다. `dotnet/tests/packet`에서 `dotnet build -c Debug`처럼 실행해도 source 경로는 동일하다.

표준 build tree와 다른 위치를 사용할 때는 property를 명시적으로 덮어쓸 수 있다.

```sh
dotnet build dotnet/tests/packet/PstkPacketTests.csproj -c Debug "-p:PstkGeneratedSourceDir=/absolute/path/to/generated"
```

- Generated source는 CMake build tree인 `out/`에 두고 직접 편집하지 않는다.
- .NET build 결과는 `dotnet/tests/packet/bin/<Debug|Release>/net8.0/`, 중간 결과는 `dotnet/tests/packet/obj/`에 생긴다.
- `dotnet test`의 통과·실패 결과는 기본적으로 터미널에 표시된다.
- `out/`, `dotnet/**/bin/`, `dotnet/**/obj/`는 Git에서 제외한다.

## 테스트가 확인하는 계약

- `RoundTripsSignedBoundaries`: signed 최소값, `-1`, `0`, 최대값과 unsigned 최소·최대값을 포함한 8개 정수 primitive의 encode/decode를 확인한다.
- `MatchesGoldenWireFormat`: C++ 테스트와 동일한 32-byte golden에 대해 encode 결과와 decode한 field 값을 확인한다.
- `RejectsInvalidOutputSizesWithoutMutation`: 짧거나 긴 output에서 `TryEncode`가 `false`를 반환하고 기존 buffer를 보존하는지 확인한다.
- `RejectsInvalidInputSizeAndVersion`: 잘못된 input 크기나 payload version에서 `TryDecode`가 `false`와 `default`를 반환하는지 확인한다.

Golden bytes는 C++과 C# 테스트 코드에 각각 직접 명시한다. 외부 JSON fixture나 parser는 사용하지 않는다. C# decode 실패의 `default` 반환은 C++ decode 실패의 기존 객체 보존과 구분되는 언어별 API 계약이다.

## 문제 해결

- `AllTypes.generated.cs`를 찾지 못하면 선택한 Debug/Release 경로에 파일이 있는지 확인하고 대응하는 CMake target을 먼저 빌드한다. `.NET` build가 누락된 파일을 생성하지는 않는다.
- Generator나 descriptor를 수정했다면 native target을 다시 빌드한 뒤 `.NET`을 다시 빌드한다. Support나 C# 테스트만 수정했다면 `.NET` build부터 진행한다.
- 예전 테스트 결과가 나온다면 `--no-build`로 이전 assembly를 실행 중인지 확인한다.
- CLI build는 성공하지만 에디터에서 generated type을 인식하지 못하면 에디터의 language server가 읽는 프로젝트·구성을 확인한다. 진단을 없애기 위해 generated source를 테스트 폴더에 복사하지 않는다.

세부 wire 계약과 검증 범위는 [Issue #3 디자인 문서](../docs/design/issue-3-generated-csharp-conformance.md)를 참고한다.
