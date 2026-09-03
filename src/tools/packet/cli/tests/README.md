# Packet CLI smoke

이 smoke는 이미 빌드한 `pstk-packet` CLI가 INI 설정을 실제 compiler 호출로
전달하는지 확인한다. 단일 schema와 재귀 directory 입력, INI 파일 기준 상대
경로, C++/C# 선택과 optional namespace만 검증한다. Fixture는 packet layout만
담으며 수기 World semantic validation, gameplay integration, golden/codec 검사는
범위 밖이다.

## Schema provenance

세 fixture는 `PrivateServer`의 commit
[`21fe5ee4fb73cb9e064739d1f47b823f62d2451e`](https://github.com/jammer-droid/PrivateServer/tree/21fe5ee4fb73cb9e064739d1f47b823f62d2451e)
기준으로 작성했다. `schemaVersion`과 `payloadVersion`은 모두 `1`이며,
payload version은 compiler가 앞에 배치하는 implicit `uint16`이라 `fields`에
중복하지 않는다. Packet ID는 World transport의 `packetType`이고 payload에 ID
bytes를 넣지 않는다.

| Packet | Direction / ID | Fields in order | PayloadBytes |
| --- | --- | --- | ---: |
| `MovementInput` | C2S / 257 (`0x0101`) | `controlledEntityGeneration:uint32`, `targetServerTick:uint32`, `moveX:int16`, `moveY:int16` | 14 |
| `WorldTimeSyncRequest` | C2S / 258 (`0x0102`) | `probeSequence:uint32` | 6 |
| `WorldTimeSyncResponse` | S2C / 391 (`0x0187`) | `probeSequence:uint32`, `serverTick:uint32` | 10 |

근거 source는 [`WorldPacketTypes.h`](https://github.com/jammer-droid/PrivateServer/blob/21fe5ee4fb73cb9e064739d1f47b823f62d2451e/src/PrivateServer.WorldServer/WorldPacketTypes.h),
[`MovementInput.h`](https://github.com/jammer-droid/PrivateServer/blob/21fe5ee4fb73cb9e064739d1f47b823f62d2451e/src/PrivateServer.WorldServer/MovementInput.h),
[`WorldTimeSyncRequest.h`](https://github.com/jammer-droid/PrivateServer/blob/21fe5ee4fb73cb9e064739d1f47b823f62d2451e/src/PrivateServer.WorldServer/WorldTimeSyncRequest.h),
[`WorldTimeSyncResponse.h`](https://github.com/jammer-droid/PrivateServer/blob/21fe5ee4fb73cb9e064739d1f47b823f62d2451e/src/PrivateServer.WorldServer/WorldTimeSyncResponse.h),
[`WorldProtocolValues.h`](https://github.com/jammer-droid/PrivateServer/blob/21fe5ee4fb73cb9e064739d1f47b823f62d2451e/src/PrivateServer.WorldServer/WorldProtocolValues.h)다.
`MovementInput`은 지원하는 v1 fixture이며, active v2 control은
`ControlStateCommand`가 사용할 수 있다. 이 fixture는 `MovementInput`을 유일한
기본 movement 경로라고 주장하지 않는다.

## 실행

CMake 3.24 이상과 CLI 및 함께 로드 가능한 shared library를 먼저 빌드한다.
CTest나 `.NET` 자동 integration에는 등록하지 않는다. 저장소 root에서 다음처럼
실행하며 두 경로는 caller 기준 root-relative이다.

```sh
cmake --preset dev
cmake --build --preset build-dev --target pstk_packet_cli
cmake -DPSTK_PACKET_CLI=out/build/dev/tools/packet/cli/pstk-packet \
  -P src/tools/packet/cli/tests/smoke.cmake
```

Windows에서는 실제 configuration directory와 `.exe`를 포함한 경로를 전달한다.

```powershell
cmake --preset dev
cmake --build --preset build-dev --target pstk_packet_cli
cmake -DPSTK_PACKET_CLI=out/build/dev/tools/packet/cli/Debug/pstk-packet.exe `
  -P src/tools/packet/cli/tests/smoke.cmake
```

각 실행은 기존 결과를 지우지 않고
`out/build/packet-cli-smoke/run-<random>/` 아래에 schema 복사본, case별 INI,
빈 working directory와 generated output을 남긴다. 네 case는 다음을 의미한다.

| Case | Input / namespace | Expected output |
| --- | --- | --- |
| `cpp-single` | `MovementInput.json` / `CliSmoke` | `MovementInput.generated.h` 1개 |
| `csharp-single` | `MovementInput.json` / `CliSmoke` | `MovementInput.generated.cs` 1개 |
| `cpp-directory` | schema tree / global namespace | 세 packet의 `.generated.h` 3개, flat layout |
| `csharp-directory` | schema tree / global namespace | 세 packet의 `.generated.cs` 3개, flat layout |

실패 시 case 이름, CLI stdout/stderr와 보존된 artifact 경로를 출력하며, 성공 시에도
run 경로를 출력한다.
