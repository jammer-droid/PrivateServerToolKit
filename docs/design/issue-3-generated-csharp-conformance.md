# GitHub Issue #3: Generated C# codec .NET build와 C++/C# wire conformance

- Issue: [jammer-droid/PrivateServerToolKit#3](https://github.com/jammer-droid/PrivateServerToolKit/issues/3)
- Issue state: Open
- Last verified: 2026-08-29

## 문서 역할

Issue #3은 Issue #1 Phase 2에서 생성할 수 있게 된 C# packet source를 실제
`.NET` toolchain으로 compile하고, C++과 C# codec이 같은 fixed-layout wire
계약을 구현하는지 검증한다.

이 문서는 generated test source의 ownership, CMake와 `.NET`의 경계,
golden bytes, xUnit 검증 범위와 dependency 순서가 있는 구현 slice를
관리한다. SDK 배포와 실제 consumer build 연결은 Issue #1 Phase 3가
소유한다.

## 현재 근거

- C# generator는 `AllTypes`와 같은 `PacketDescriptor`에서 `record struct`,
  `PayloadVersion`, `PayloadBytes`, `TryEncode`와 `TryDecode`를 생성할 수 있다.
- `tools/packet/support/csharp/TkPacketCodecSupport.cs`는 8개 정수 primitive의
  little-endian read/write를 제공한다.
- CMake custom command는 같은 `AllTypes` descriptor로
  `AllTypes.generated.h`와 `AllTypes.generated.cs`를 함께 생성해 선택한
  Debug/Release build tree에 남긴다.
- `CMakePresets.json`은 대칭적인 Debug/Release configure, build와 test
  preset을 제공한다.
- `dotnet/tests/packet/PstkPacketTests.csproj`는 generated C# source와 canonical
  support source를 `net8.0`으로 compile하고 xUnit/VSTest로 실행한다.
- C++과 C# generated codec test는 동일한 32-byte golden wire와 integer
  boundary를 검증한다.

Issue 본문은 C++/C#이 함께 소비하는 golden vector 또는 동등한 언어 독립
fixture를 요구한다. 이 design은 별도 fixture file 대신 양쪽 테스트 코드에
동일한 expected wire bytes를 직접 명시하는 방식을 동등한 fixture로 사용한다.

## Issue 경계

포함하는 것:

- 같은 `AllTypes` descriptor에서 C++/C# test source를 함께 생성하고 build
  tree에 보존하는 CMake 경로
- `dotnet/tests/packet/PstkPacketTests.csproj`와 공용
  `dotnet/Directory.Build.props`
- `net8.0`, xUnit v3와 VSTest 기반 `dotnet build`/`dotnet test`
- Debug/Release generated source 경로와 이에 대응하는 CMake preset
- 8개 정수 primitive의 boundary, golden wire와 C# failure 계약 검증
- 기존 C++ generated codec test의 golden decode 보강

포함하지 않는 것:

- CMake나 CTest에서 `dotnet`을 탐색하거나 실행하는 동작
- `.NET` SDK exact version을 고정하는 `global.json`
- 별도 conformance executable, CTest 또는 fixture parser
- JSON golden fixture와 C++/C#용 JSON parsing dependency
- SDK 설치·배포, consumer build hook과 stale generated source 정책
- PrivateServer와 Godot client 적용
- Typed Service Host, middleware와 async unary lifecycle
- Codec benchmark와 최적화

## Toolchain과 ownership

```text
AllTypes PacketDescriptor
    |
    v
pstk_packet_generate_test_sources           CMake build step
    |                          |
    v                          v
AllTypes.generated.h           AllTypes.generated.cs
    |                          |          |
    v                          |          v
C++ generated codec test       |   PstkPacketTests.csproj
                               |          ^
                               v          |
                         CMake build tree  TkPacketCodecSupport.cs
```

- Generated `.h`와 `.cs`는 source tree에 commit하지 않고 CMake build tree가
  소유한다.
- 생성은 CTest 실행 중 발생하는 side effect가 아니라 `add_custom_command()`로
  선언한 build step이다.
- 기존 `pstk_packet_generate_test_header`를
  `pstk_packet_generate_test_sources`로 일반화하고 C++ header와 C# source의
  두 output path를 명시적 argument로 받는다.
- 하나의 custom command가 두 output을 모두 선언한다. 별도
  `conformance-sources` target은 만들지 않는다.
- 기존 `pstk_packet_generated_codec_tests`가 generated header를 요구하면
  같은 build step에서 C# source도 생성된다.
- `.NET` project는 이미 생성된 source를 compile할 뿐 CMake를 호출하지
  않는다. Generated source가 없으면 이해 가능한 오류로 build를 실패시킨다.
- `TkPacketCodecSupport.cs`는 테스트용으로 복사하지 않고 `.csproj`의
  `Compile Include`로 source tree의 원본을 직접 compile한다.

Debug output:

```text
out/build/dev/tools/packet/tests/generated/AllTypes.generated.h
out/build/dev/tools/packet/tests/generated/AllTypes.generated.cs
```

Release output:

```text
out/build/release/tools/packet/tests/generated/AllTypes.generated.h
out/build/release/tools/packet/tests/generated/AllTypes.generated.cs
```

## CMake preset 계약

기존 Debug preset은 유지한다. Release 검증을 위해 다음 preset을 대칭으로
추가한다.

```text
release        configure, out/build/release, CMAKE_BUILD_TYPE=Release
build-release  release configure preset의 Release build
test-release   release configure preset의 CTest 실행
```

Release preset도 Packet Tool, test와 vcpkg의 `tests` feature를 활성화한다.
CMake preset은 native build configuration만 소유하며 `.NET` 명령을 포함하지
않는다.

## .NET project 계약

```text
dotnet/
  Directory.Build.props
  tests/
    packet/
      PstkPacketTests.csproj
      PstkPacketTests.cs
```

- Target framework는 `net8.0`이다.
- Test framework는 xUnit v3, 실행 platform은 VSTest다.
- xUnit v3의 standalone test project 모델에 맞춰 `OutputType`은 `Exe`로
  설정하지만 표준 실행 진입점은 `dotnet test`와 VSTest로 유지한다.
- `dotnet test` 연동을 위해 xUnit VSTest adapter와
  `Microsoft.NET.Test.Sdk`를 사용한다.
- NuGet package는 구현 시점의 호환되는 stable version을 `.csproj`에
  명시적으로 고정하고 floating version을 사용하지 않는다.
- `global.json`과 Microsoft.Testing.Platform 전용 설정은 추가하지 않는다.
- `PstkPacketTests.csproj`는 CMake를 실행하거나 CMake target을 의존성으로
  선언하지 않는다.

`dotnet/Directory.Build.props`는 설정별 generated source 경로를 직접
기술한다. 경로 계산 helper를 만들지 않고 읽기 쉬운 상대 경로를 유지한다.

```xml
<Project>
  <PropertyGroup Condition="'$(Configuration)' == 'Debug' and '$(PstkGeneratedSourceDir)' == ''">
    <PstkGeneratedSourceDir>
      $(MSBuildThisFileDirectory)../out/build/dev/tools/packet/tests/generated
    </PstkGeneratedSourceDir>
  </PropertyGroup>

  <PropertyGroup Condition="'$(Configuration)' == 'Release' and '$(PstkGeneratedSourceDir)' == ''">
    <PstkGeneratedSourceDir>
      $(MSBuildThisFileDirectory)../out/build/release/tools/packet/tests/generated
    </PstkGeneratedSourceDir>
  </PropertyGroup>
</Project>
```

`.csproj`는 다음 두 source를 실제 compile input으로 사용한다.

```xml
<Compile Include="../../../tools/packet/support/csharp/TkPacketCodecSupport.cs" />
<Compile Include="$(PstkGeneratedSourceDir)/AllTypes.generated.cs" />
```

특수한 build tree를 사용할 때는
`-p:PstkGeneratedSourceDir=<path>`로 기본값을 덮어쓸 수 있지만, 표준
Debug/Release 실행에서는 경로 argument를 반복하지 않는다.

## Golden wire 계약

Golden vector는 별도 JSON file로 만들지 않는다. C++과 C# 테스트 코드가
같은 field values와 expected bytes를 각각 직접 명시한다. 중복은 작고
의도적이며, runtime fixture parsing보다 wire 계약을 바로 읽을 수 있는 것을
우선한다.

`AllTypes` golden values:

```text
payloadVersion  1
int8Value       -1
uint8Value      0x12
int16Value      -2
uint16Value     0x3456
int32Value      0x01234567
uint32Value     0x89ABCDEF
int64Value      -2
uint64Value     0x0123456789ABCDEF
```

Expected 32-byte little-endian payload:

```text
01 00 FF 12 FE FF 56 34
67 45 23 01 EF CD AB 89
FE FF FF FF FF FF FF FF
EF CD AB 89 67 45 23 01
```

양쪽 테스트는 golden bytes에 대해 encode와 decode를 모두 확인한다.

```text
C++ values -> golden bytes -> C# values
C# values  -> golden bytes -> C++ values
```

Round-trip test만으로는 양쪽 codec이 같은 방식으로 잘못 구현된 경우를 찾지
못하므로, 정확한 expected bytes 비교가 언어 간 wire parity의 기준이다.

## C# codec failure 계약

Generated C# API는 Toolkit의 C++ `TkResult`와 diagnostic을 client에 강제하지
않고 `TryEncode`/`TryDecode`의 단순한 `bool` 계약을 유지한다.

`TryEncode`:

- `output.Length == PayloadBytes`일 때만 encode한다.
- 너무 작거나 큰 output은 `false`를 반환한다.
- 길이 검사를 첫 write 전에 수행하므로 실패한 output은 호출 전 bytes를
  유지한다.

`TryDecode`:

- 함수 진입 시 `out value`를 `default`로 설정한다.
- `input.Length != PayloadBytes`이면 `false`와 `default`를 반환한다.
- payload version이 다르면 `false`와 `default`를 반환한다.
- 모든 검증과 field read가 성공한 뒤 생성한 record를 `out value`에 넣는다.

C++ Decode의 failure atomicity는 기존 객체를 유지하지만, C#은 `out`
parameter API이므로 실패 시 `default`가 명시적인 계약이다.

## xUnit 검증 범위

`PstkPacketTests.cs`는 다음 네 동작만 검증한다. Generated source의 문자열
구조와 결정성은 기존 C++ generator test가 소유하므로 xUnit에서 중복하지
않는다.

1. `RoundTripsSignedBoundaries`
   - signed 최소값, `-1`, `0`, signed 최대값과 unsigned 최대값을 포함한다.
   - 8개 primitive가 C# encode/decode에서 손실되지 않는지 확인한다.
2. `MatchesGoldenWireFormat`
   - C# encode 결과가 위 32-byte golden과 같은지 확인한다.
   - 같은 golden을 C# decode했을 때 원래 field values가 나오는지 확인한다.
3. `RejectsInvalidOutputSizesWithoutMutation`
   - 짧거나 긴 output에서 `TryEncode`가 `false`를 반환하는지 확인한다.
   - sentinel로 채운 output bytes가 변경되지 않는지 확인한다.
4. `RejectsInvalidInputSizeAndVersion`
   - 짧거나 긴 input과 잘못된 payload version을 거부하는지 확인한다.
   - 모든 실패에서 `out value == default`인지 확인한다.

C# client surface에 common diagnostic 계약을 노출하지 않으므로 xUnit에
diagnostic test를 추가하지 않는다.

## 구현 순서

### I3-S1 — C++/C# generated test source를 같은 build step에서 생성

- **Delivered outcome:** Debug/Release build tree에 동일 descriptor 기반의
  `AllTypes.generated.h`와 `AllTypes.generated.cs`가 함께 존재한다.
- **Dependency:** 없음.
- **Relevant seam:** `TkPacketGenerateTestHeader.cpp`, packet test
  `CMakeLists.txt`, `CMakePresets.json`.
- **변경:** 생성 실행 파일과 source/target 이름을 `header`에서 `sources`로
  일반화하고 두 output path를 받는다. C++/C# generator를 모두 link하며
  custom command `OUTPUT`에 두 file을 선언한다. Release preset 세 개를
  추가한다.
- **Invariant:** CMake/CTest는 `dotnet`을 탐색하거나 실행하지 않는다. Source
  tree에 generated file을 쓰지 않는다. 별도 conformance target을 만들지
  않는다.
- **Acceptance:** `pstk_packet_generated_codec_tests` build가 선택한 preset의
  generated directory에 두 file을 만들고 기존 C++ test target이 계속
  compile된다.
- **Verification:** Debug와 Release에서 해당 target을 build하고 두 output의
  존재를 확인한다.

### I3-S2 — `net8.0` xUnit project가 canonical C# source를 compile

- **Delivered outcome:** `PstkPacketTests.csproj`가 build tree의 generated C#
  source와 source tree의 canonical support source를 함께 compile한다.
- **Dependency:** I3-S1.
- **Relevant seam:** `dotnet/Directory.Build.props`,
  `dotnet/tests/packet/PstkPacketTests.csproj`.
- **변경:** Debug/Release generated path, `net8.0`, xUnit v3/VSTest package와
  두 `Compile Include`를 추가한다. Generated source가 없으면 원인과 선행
  CMake build를 설명하는 오류를 제공한다.
- **Invariant:** `.NET` project가 CMake를 호출하지 않는다. Support와
  generated source를 test directory로 복사하지 않는다. SDK exact version을
  고정하지 않는다.
- **Acceptance:** 선행 CMake build 뒤 Debug와 Release `dotnet build`가 각각
  성공하며, generated source를 제거하거나 잘못된 path를 주면 명확하게
  실패한다.
- **Verification:** `dotnet build` Debug/Release와 missing-source failure를
  확인한다.

### I3-S3 — Golden wire와 C# failure 계약 실행 검증

- **Delivered outcome:** C++과 C# codec이 같은 golden bytes를 양방향으로
  처리하고 C# boundary/failure 계약을 지킨다.
- **Dependency:** I3-S2.
- **Relevant seam:** `TkPacketGeneratedCodecTests.cpp`,
  `PstkPacketTests.cs`.
- **변경:** 기존 C++ golden test가 in-code golden bytes decode까지
  확인하도록 보강한다. xUnit에 확정한 네 테스트를 추가한다.
- **Invariant:** 별도 JSON fixture/parser와 diagnostic test를 추가하지
  않는다. 기존 C++/C# wire values와 offsets를 변경하지 않는다.
- **Acceptance:** 양쪽 encode가 같은 32 bytes를 만들고 양쪽 decode가 같은
  values를 복원한다. Integer boundary와 모든 C# failure case가 통과한다.
- **Verification:** focused CTest와 Debug/Release `dotnet test`를 실행한다.

## 재현 명령

Debug:

```sh
cmake --preset dev
cmake --build --preset build-dev --target pstk_packet_generated_codec_tests
ctest --preset test-dev -R '^pstk\.packet\.generated-codec\.'

dotnet build dotnet/tests/packet/PstkPacketTests.csproj -c Debug
dotnet test dotnet/tests/packet/PstkPacketTests.csproj -c Debug --no-build
```

Release:

```sh
cmake --preset release
cmake --build --preset build-release --target pstk_packet_generated_codec_tests
ctest --preset test-release -R '^pstk\.packet\.generated-codec\.'

dotnet build dotnet/tests/packet/PstkPacketTests.csproj -c Release
dotnet test dotnet/tests/packet/PstkPacketTests.csproj -c Release --no-build
```

Issue 완료 전에는 전체 native regression도 한 번 확인한다.

```sh
cmake --build --preset build-dev
ctest --preset test-dev
git diff --check
```

## 현재 상태

- I3-S1, I3-S2, I3-S3 구현을 완료했다.
- Debug/Release focused CTest는 generated codec test 5개가 각각 통과했다.
- Debug/Release xUnit/VSTest는 packet codec test 4개가 각각 통과했다.
- 전체 Debug native build와 CTest 33개가 통과했으며 `git diff --check`도
  이상이 없다.
- 남은 blocker와 미해결 design decision은 없다.
- SDK 배포와 consumer build integration은 이 Issue의 제외 범위대로 Issue #1
  Phase 3에서 이어간다.
