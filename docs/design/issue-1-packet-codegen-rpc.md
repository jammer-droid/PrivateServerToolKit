# GitHub Issue #1: 스키마 기반 C++/C# 패킷 코드 생성기

- Issue: [jammer-droid/PrivateServerToolKit#1](https://github.com/jammer-droid/PrivateServerToolKit/issues/1)
- Issue state: Open
- Last verified: 2026-08-31

## 문서 역할

GitHub Issue #1은 schema를 source of truth로 삼아 C++/C# fixed-layout packet code를 생성하고, 미리 빌드한 C++ CLI와 언어별 support를 전달하는 상위 범위와 완료 조건을 소유한다. 실제 consumer의 build 연결과 통신 적용은 해당 consumer 작업에서 결정한다.

이 문서는 Issue #1의 세부 계약, 하위 Phase, 구현 순서와 검증 기준을 관리한다. 기존 `Phase 0: Common Contract`은 독립 프로젝트 Phase가 아니라 Phase 1 Packet Compiler가 의존하는 Issue #1의 선행 slice로 통합한다.

기존 `Phase 4 — Async unary RPC 계층`은 network와 application service 사이의 command/event/unary lifecycle, middleware와 adapter까지 포함하는 [Issue #2 — Typed Service Host와 middleware pipeline](https://github.com/jammer-droid/PrivateServerToolKit/issues/2)으로 분리·확장했다. Issue #1은 해당 runtime을 구현하지 않고 packet/schema compiler, CLI와 support 배포까지만 소유한다.

## Issue 경계

이 issue에서 생성하는 것:

- 언어 독립 message/field descriptor와 schema parser
- C++/C# fixed-layout DTO와 codec source
- INI 설정으로 shared compiler를 호출하는 C++ CLI
- 미리 빌드한 CLI, shared library와 generated code에 필요한 언어별 support 배포 폴더

수기 코드와 기존 runtime 책임으로 유지하는 것:

- NetworkRuntime의 6-byte transport framing과 socket/IOCP buffer lifetime
- Direction 해석과 handler 등록을 포함한 transport routing, session/entity ownership과 gameplay semantic validation
- PrivateServer/Godot의 실제 적용, build/CI hook, generated source의 Git 포함 여부와 stale output 관리

Typed Service Host, middleware, command/event dispatch, async unary call lifecycle, schema direction metadata, 중앙 packet catalog와 자동 registration, Protobuf wire format, runtime reflection과 schema hot reload는 범위 밖이다. Service Host와 unary call은 Issue #2가 소유한다.

Generated C# source의 실제 `.NET` compile과 C++/C# golden byte conformance는 CMake 기반 compiler 구현과 다른 toolchain을 사용하므로 [Issue #3 — Generated C# codec .NET build와 C++/C# wire conformance](https://github.com/jammer-droid/PrivateServerToolKit/issues/3)로 분리했으며, 해당 issue는 2026-08-29 완료했다.

## 구현 순서

1. Phase 0 — Common contract 선행 slice
2. Phase 1 — Packet compiler core와 C++ 생성
3. Phase 2 — C# source generation
4. Phase 3 — C++ CLI와 배포

Phase 0 common contract, Phase 1 C++ compiler와 Phase 2 C# source generation은 구현과 검증을 완료했다. Phase 2는 `fddbc86`에서 언어 독립 compiler pipeline을 추출하고 `f3be9d3`에서 C# source generator와 공용 `TkPacketCompileInfo` public path를 연결했다. C# compile과 cross-language conformance는 Issue #3에서 `dotnet` toolchain으로 검증하고 CMake/CTest에 등록하지 않는다.

Issue #3은 Issue #1의 Phase 3을 대체하지 않는다. Issue #3에서 검증한 compiler/codec을 바탕으로 Phase 3은 CLI와 support 배포, INI 설정에 따른 생성 결과 확인을 다룬다.

Phase 3 grilling은 2026-08-31 완료했다. 기존 consumer build integration 계획을 C++ CLI 전달 범위로 변경했으며, 실제 PrivateServer/Godot 적용은 Issue #1의 완료 조건에서 제외한다. Schema direction metadata, 중앙 packet catalog와 자동 registration은 추가하지 않는다. 구현은 아직 시작하지 않았으며 아래 P3-S1부터 순서대로 진행한다.

## Phase 0 — Common contract 선행 slice

### 목적과 경계

Packet Tool을 포함한 여러 tool과 consumer가 같은 기초 타입과 C++ API 규칙을 사용하도록 common 계약을 고정한다. Common은 별도 runtime을 소유하지 않고 header-only `PSTK::Common` INTERFACE target으로 유지한다.

Common이 소유하는 것:

- C와 C++에서 공통으로 사용할 수 있는 self-contained POD header
- Byte range의 null/empty 구조를 검사하는 작은 header-only 함수
- Tool 경계에서 공유할 result와 diagnostic 어휘

Common이 소유하지 않는 것:

- Packet schema parser, IR, semantic validation과 code generator
- Packet별 payload 검증과 diagnostic ID catalog
- Diagnostic callback 호출 helper, logger, registry 또는 저장소
- Buffer와 diagnostic의 lifetime, allocation 또는 thread 생성
- Transport header 해석과 NetworkRuntime framing

### Byte view

- `TkByteView`: `const uint8_t* data`와 `size_t size`를 가지는 읽기 전용 non-owning POD
- `TkMutableByteView`: `uint8_t* data`와 `size_t size`를 가지는 쓰기 가능 non-owning POD
- `size == 0`은 `data`와 관계없이 유효한 empty view
- `size > 0`은 `data != nullptr`이어야 함
- `TkIsValidByteRange(const void*, size_t)`는 위 구조적 조건만 검사하는 header-only 함수
- View는 lifetime, 실제 메모리 용량 또는 packet별 크기를 검증하지 않음

상세: [ADR 0001](../adr/0001-use-byte-views-for-buffer-access.md)

### Result와 failure atomicity

- Public API와 내부 C++ 함수의 성공/실패는 공용 `TkResult`로 전파
- 모듈이나 계층별 result enum/class는 추가하지 않음
- Predicate, nullable lookup과 단순 값 계산은 각 의미에 맞는 타입을 사용
- 해석할 수 없는 schema와 encoded data는 `TK_ERROR_INVALID_DATA`
- 잘못된 호출 인자는 `TK_ERROR_INVALID_ARGUMENT`
- 출력 용량 부족은 `TK_ERROR_BUFFER_TOO_SMALL`
- 세부 원인은 `TkResult`를 세분화하지 않고 Diagnostic으로 전달
- `TkResult != TK_SUCCESS`이면 output object와 output buffer는 호출 전 상태를 유지
- 실패 시에도 필요 buffer 크기 등을 갱신하는 API는 해당 정보성 output 예외를 명시해야 함

상세: [ADR 0003](../adr/0003-use-tkresult-for-cpp-failures.md), [C++ result handling](../agents/cpp-result-style.md)

### Diagnostic

- `TkDiagnosticSeverity`: Info, Warning, Error 분류
- `TkDiagnosticLocation`: optional `sourceName`, 0-based UTF-8 `byteOffset`, 1-based `line`, 1-based byte 기준 `column`
- `TkDiagnostic`: severity, stable `id`, UTF-8 `message`, location을 가지는 borrowed POD
- `TkDiagnosticCallbackInfo`: callback function pointer와 `userData`를 가지는 POD
- `callback == nullptr`은 diagnostic을 버리는 유효한 disabled callback
- Callback은 tool API를 호출한 thread에서 API 반환 전에 동기적으로 실행
- Diagnostic과 문자열은 callback 동안만 유효하며 consumer가 필요한 정보를 복사
- 하나의 API 호출은 callback을 순차 실행하지만, 동일 callback info를 사용한 별도 API 호출들은 동시에 callback을 실행할 수 있음
- `id`는 non-empty ASCII `PSTK-<TOOL>-<NAME>` 형식의 안정적인 식별자
- Common은 diagnostic 타입과 disabled callback을 처리하는 최소 header-only emit helper만 제공하고, 구성, 저장, 필터링, logging, allocation과 thread를 소유하지 않음

상세: [ADR 0004](../adr/0004-use-common-diagnostic-callbacks.md)

### 완료 기준과 현재 상태

다음 조건이 모두 충족되면 Phase 0 구현이 완료된다.

1. `TkResult.h`가 확정된 공용 result 값과 `TK_ERROR_INVALID_DATA`를 정의한다.
2. `TkByteView.h`가 C-compatible byte view POD와 `TkIsValidByteRange`를 정의한다.
3. `TkDiagnostic.h`가 diagnostic, location, callback과 callback info POD를 정의한다.
4. Common header를 별도 binary 연결 없이 C와 C++ translation unit에서 각각 compile한다.
5. Test가 공용 result 값, byte range의 null/empty 규칙과 byte view의 복사·쓰기 계약을 검증한다.

Phase 0은 2026-08-25 완료했다. `TkDiagnostic.h`의 계약은 이후 additive follow-up으로 disabled callback 확인과 동기 호출만 수행하는 최소 header-only helper까지 확장했으며, C11/C++17 translation unit의 독립 compile로 호환성을 확인한다.

### 구현 가이드

Phase 0은 Packet Tool 기능을 추가하지 않고 common header와 계약 테스트만 구현한다. 작업 중 schema, parser, generator, packet diagnostic ID 또는 `MovementInput` codec을 리팩터링하지 않는다.

#### 1. `TkResult` 어휘 완성

`include/pstk/TkResult.h`에 `TK_ERROR_INVALID_DATA = -5`를 추가한다. 기존 숫자값은 변경하지 않는다.

확정된 값:

```text
TK_SUCCESS                    0
TK_ERROR_UNKNOWN             -1
TK_ERROR_INVALID_ARGUMENT    -2
TK_ERROR_BUFFER_TOO_SMALL    -3
TK_ERROR_OUT_OF_MEMORY       -4
TK_ERROR_INVALID_DATA        -5
```

Packet, parser 또는 generator 전용 result 값은 추가하지 않는다.

#### 2. Byte view header 완성

`include/pstk/TkByteView.h`를 C와 C++ 모두에서 include할 수 있는 self-contained header로 완성한다.

- C++ 전용 `<cstddef>` 대신 `<stddef.h>`, `<stdint.h>`와 `<stdbool.h>`를 사용한다.
- `size`에 `const`를 붙이지 않아 view가 복사·대입 가능한 POD가 되게 한다.
- `TkIsValidByteRange(const void* data, size_t size)`는 `bool`을 반환하는 `static inline` predicate로 정의한다.
- 구현은 `size == 0 || data != NULL`만 판단한다.
- View에 constructor, method, template, lifetime 관리 또는 packet 크기 검증을 추가하지 않는다.

#### 3. Diagnostic header 추가

`include/pstk/TkDiagnostic.h`에 공용 POD, callback type과 최소 emit helper를 정의한다. 모든 enum 값은 명시적으로 고정한다.

```text
TkDiagnosticSeverity
  TK_DIAGNOSTIC_INFO       0
  TK_DIAGNOSTIC_WARNING    1
  TK_DIAGNOSTIC_ERROR      2
  TK_DIAGNOSTIC_SEVERITY_MAX_ENUM  0x7FFFFFFF

TkDiagnosticLocation
  const char* sourceName
  size_t      byteOffset
  uint32_t    line
  uint32_t    column

TkDiagnostic
  TkDiagnosticSeverity severity
  const char*          id
  const char*          message
  TkDiagnosticLocation location

TkDiagnosticCallback
  void (*)(const TkDiagnostic* diagnostic, void* userData)

TkDiagnosticCallbackInfo
  TkDiagnosticCallback callback
  void*                userData

TkEmitDiagnostic
  disabled callback이면 반환하고, 그 외에는 non-null Diagnostic과 userData로 callback을 동기 호출
```

`TkEmitDiagnostic`은 C-compatible `static inline` helper이며 callback disabled 여부만 확인한다. Diagnostic 구성, emit 시점과 순서는 tool이 소유하고 Common에는 logger, registry, allocation 또는 thread 코드를 추가하지 않는다. Disabled callbacks는 `{ NULL, NULL }`로 표현할 수 있어야 한다.

#### 4. Common C/C++ 계약 테스트 추가

루트 CMake project를 `LANGUAGES C CXX`로 구성하고 `common/tests/`에 `pstk_common_contract_c`, `pstk_common_contract_cpp` 테스트 target을 둔다. Common은 header-only `INTERFACE` target이므로 언어 표준을 consumer에게 전파하지 않고, C11/C++17 요구사항은 각 테스트 target에 `PRIVATE`로 지정한다. Common 테스트는 `PSTK_BUILD_PACKET_TOOL=OFF`인 build에서도 구성·빌드·실행되어야 한다.

개발 의존성은 root `vcpkg.json`의 `tests` feature로 관리한다. C 계약 테스트는 CTest 실행 파일로 유지하고 C++ 계약 테스트는 GoogleTest와 `gtest_discover_tests()`를 사용한다. GoogleTest 탐색은 `common/tests/` 안에서만 수행하여 `BUILD_TESTING=OFF`인 Common consumer가 테스트 의존성을 요구하지 않게 한다.

테스트 구조:

- C와 C++17 translation unit에서 공용 result와 byte view header를 compile한다.
- C++ test에서 byte view struct의 standard-layout/trivially-copyable 조건을 `static_assert`한다.
- C test에서 view 복사·대입을 compile해 `size`가 const field로 회귀하지 않도록 한다.
- Result 숫자값을 검증한다.
- `TkDiagnostic.h`는 별도 C11/C++17 translation unit compile로 self-contained C 호환성을 확인한다.

Byte range 필수 case:

| `data` | `size` | 결과 |
|---|---:|---|
| null | 0 | valid |
| non-null | 0 | valid |
| null | 1 | invalid |
| non-null | 1 | valid |

테스트는 common이 소유하지 않는 lifetime, 실제 allocation 크기, diagnostic 문자열 encoding 또는 callback 동시성을 검증하려고 하지 않는다.

#### 5. Packet target 연결 확인

`pstk_packet`은 기존처럼 `PSTK::Common`을 public link한다. Phase 0에서 Packet public API를 확장하지 않고 기존 API version smoke test가 그대로 통과하는지만 확인한다.

### 검증 게이트

#### Gate A — Common-only

Packet Tool을 빌드하지 않고 common header와 테스트가 독립적으로 성립하는지 확인한다.

```sh
cmake -S . -B out/build/phase0-common-only \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_MANIFEST_FEATURES=tests \
  -DPSTK_BUILD_PACKET_TOOL=OFF \
  -DBUILD_TESTING=ON
cmake --build out/build/phase0-common-only
ctest --test-dir out/build/phase0-common-only --output-on-failure
```

2026-08-25 실행 결과는 Common C/C++ 계약 테스트 4개 중 4개 통과다.

#### Gate B — Full regression

```sh
cmake --preset dev
cmake --build --preset build-dev
ctest --preset test-dev
git diff --check
```

Gate B에서 기존 `pstk.packet.api.version`과 새 common C/C++ 테스트가 모두 통과해야 한다.

2026-08-25 실행 결과는 Common 계약 테스트와 `pstk.packet.api.version`을 포함한 5개 중 5개 통과이며 `git diff --check`도 통과했다.

### Phase 0 완료 판정

Phase 0은 다음 조건을 충족하여 완료됐다.

- 세 common header가 확정된 계약과 일치한다.
- C/C++ 양쪽의 self-contained header compile과 공용 result/byte view contract test가 통과했다.
- Common-only와 full build/test gate가 모두 통과한다.
- Common에 tool-specific helper, storage, allocation 또는 runtime dependency가 추가되지 않았다.
- 남은 작업이 Phase 1의 schema/parser/IR/generator design으로 한정된다.

## Phase 1 — Packet compiler core와 C++ 생성

### 상속한 계약

- Generated codec은 byte view 크기가 packet의 `PayloadBytes`와 정확히 같을 때만 Encode/Decode한다.
- `PayloadBytes`는 payload version과 선언 field를 포함하고 NetworkRuntime의 6-byte transport header는 제외한다.
- 첫 vertical slice인 `MovementInput`의 `PayloadBytes`는 14다.
- Decode는 임시 객체를 검증한 뒤 성공 시에만 output에 commit한다.
- Encode는 모든 입력과 크기를 검증한 뒤 output buffer를 기록한다.
- Compiler는 실패한 중간 IR이나 불완전한 생성물을 output으로 commit하지 않는다.

상세: [ADR 0002](../adr/0002-require-exact-fixed-layout-payload-size.md)

### 확정된 범위

Phase 1은 build-time JSON schema compiler와 C++17 생성기만 구현한다. Runtime JSON parsing, C# 생성, consumer build integration, Issue #2의 Service Host/unary call lifecycle과 gameplay semantic validation은 포함하지 않는다. `MovementInput`은 예시 전용 타입이 아니라 첫 end-to-end vertical slice다.

#### Schema 계약

Schema 파일 하나는 packet 하나만 정의한다. Compiler는 여러 schema 경로를 한 번에 입력받을 수 있다.

```json
{
  "schemaVersion": 1,
  "packet": {
    "name": "MovementInput",
    "id": 257,
    "payloadVersion": 1,
    "fields": [
      {"name": "controlledEntityGeneration", "type": "uint32"},
      {"name": "targetServerTick", "type": "uint32"},
      {"name": "moveX", "type": "int16"},
      {"name": "moveY", "type": "int16"}
    ]
  }
}
```

- `schemaVersion`은 compiler grammar version이며 필수 정수 `1`만 허용한다.
- `payloadVersion`은 wire payload version이며 `1..65535` 범위다. Compiler가 payload offset 0에 `uint16`으로 삽입하며 `fields`에는 포함하지 않는다.
- `packet.id`는 `uint16` 전체 범위 `0..65535`를 허용한다. 한 compile batch에서 packet ID와 packet name은 각각 유일해야 한다.
- `fields: []`인 packet을 허용하며 이 경우 `PayloadBytes`는 payload version만 포함한 2다.
- Phase 1 primitive type은 `int8`, `uint8`, `int16`, `uint16`, `int32`, `uint32`, `int64`, `uint64`다.
- Packet name은 PascalCase, field name은 lowerCamelCase로 작성하도록 안내하되 Phase 1에서 naming style 정규식 검사는 하지 않는다.
- 필수 name은 비어 있을 수 없고 같은 packet의 field name은 중복될 수 없다.
- 알 수 없는 JSON property, duplicate key와 알 수 없는 field type은 오류다.
- `direction`은 fixed-layout schema와 descriptor에 포함하지 않는다. Direction 해석과 handler binding은 generated code를 사용하는 consumer runtime이 소유한다.

#### Wire와 layout 계약

- 모든 정수 field는 선언 순서대로 배치하고 little-endian으로 Encode/Decode한다.
- Compiler layout pass가 payload version과 각 field의 offset, size, 최종 `PayloadBytes`를 계산해 검증된 언어 독립 IR에 저장한다.
- Generator는 offset, size 또는 `PayloadBytes`를 다시 계산하지 않고 IR 값을 사용한다.
- Compiler는 layout 산술 overflow를 거부하지만 현재 NetworkRuntime의 최대 payload 정책을 schema compiler에 하드코딩하지 않는다.
- Signed wire 값은 2의 보수다. Codec은 raw byte를 같은 폭의 unsigned 값으로 조립한 뒤 signed field만 명시적으로 signed 값으로 변환한다.
- Host byte order, signed object representation 또는 unaligned access에 의존하는 `memcpy`/reinterpret 방식은 사용하지 않는다.

#### 입력, parser와 diagnostic 계약

- Public consumer는 schema 경로만 전달한다. Packet Tool file adapter가 각 경로를 현재 working directory를 바꾸지 않고 한 번 읽어 source 이름과 owned JSON byte를 가진 `PacketSource`를 만든다.
- 공통 `PacketSchemaCompiler`가 `PacketSource`를 `PacketJsonParser`에 전달해 language-neutral `PacketSchema`로 변환하고, 각 schema를 만든 직후 batch name/ID 유일성을 검사한 뒤 `PacketDescriptorBuilder`를 호출한다.
- 입력 경로는 caller가 전달한 순서대로 처리하며 첫 read, JSON syntax 또는 schema semantic 오류에서 전체 compile을 중단한다. 모든 입력이 성공한 뒤에만 공통 schema compiler가 `PacketDescriptorSet`을 commit한다.
- JSON parser는 Packet Tool의 private vcpkg dependency인 nlohmann/json을 사용한다.
- Diagnostic은 source 경로와 `packet.fields[2].type` 같은 logical schema path를 message에 포함한다.
- Phase 1은 별도 line/column scanner를 구현하지 않는다. Source 파일은 알지만 정확한 위치를 모르면 `sourceName != nullptr`이고 `byteOffset`, `line`, `column`은 모두 0이다.
- 모든 Phase 1 failure diagnostic severity는 `TK_DIAGNOSTIC_ERROR`다.
- Diagnostic ID 문자열은 Packet Tool 내부 `inline constexpr const char*`로 정의하고 public symbol로 export하지 않는다. Consumer는 pointer가 아니라 문자열 내용을 비교한다.

Phase 1의 안정적인 diagnostic ID catalog:

```text
PSTK-PACKET-SOURCE-READ-FAILED
PSTK-PACKET-INVALID-JSON
PSTK-PACKET-INVALID-SCHEMA
PSTK-PACKET-UNSUPPORTED-SCHEMA-VERSION
PSTK-PACKET-DUPLICATE-KEY
PSTK-PACKET-DUPLICATE-PACKET-ID
PSTK-PACKET-DUPLICATE-PACKET-NAME
PSTK-PACKET-DUPLICATE-FIELD-NAME
PSTK-PACKET-UNKNOWN-FIELD-TYPE
PSTK-PACKET-LAYOUT-OVERFLOW
PSTK-PACKET-OUTPUT-WRITE-FAILED
PSTK-PACKET-INVALID-PAYLOAD-SIZE
PSTK-PACKET-UNSUPPORTED-PAYLOAD-VERSION
```

#### Public compiler API

Phase 1은 여러 입력을 한 번에 처리하는 C-compatible Packet DLL API 하나를 추가한다.

```c
typedef struct TkPacketCompileInfo
{
    const char *const *inputPaths;
    size_t inputPathCount;
    const char *outputDirectory;
    const char *namespaceName;
    TkDiagnosticCallbackInfo diagnosticCallback;
} TkPacketCompileInfo;

PSTK_PACKET_API TkResult TkPacketCompileCpp(
    const TkPacketCompileInfo *compileInfo);
```

- 모든 pointer와 문자열은 호출 동안만 borrowed다.
- Phase 1 구현 기준으로 `compileInfo`, `inputPaths`, 각 input path, `outputDirectory`, `namespaceName`은 non-null이어야 하고 문자열은 non-empty여야 하며 `inputPathCount > 0`이어야 한다. 이 호출 계약을 위반하면 diagnostic 없이 `TK_ERROR_INVALID_ARGUMENT`를 반환한다. P3-S1에서 `namespaceName`만 optional로 변경한다.
- C++ namespace는 schema가 아니라 compile info로 전달한다. 현재 구현은 필수이며, P3-S1에서 null/empty일 때 namespace wrapper를 생략하도록 변경한다. `namespace`는 C++ keyword이므로 field 이름은 `namespaceName`을 사용한다.
- Diagnostic callback은 값으로 전달하며 `callback == nullptr`이면 disabled다.
- 파일 open/read/write 실패에는 공용 `TK_ERROR_IO`를 사용하고 schema 또는 payload 해석 실패에는 `TK_ERROR_INVALID_DATA`를 사용한다.
- Packet 전용 result type이나 layer별 result type은 만들지 않는다.

`TK_ERROR_IO`, source는 있지만 내부 위치를 모르는 Diagnostic location 표현과 최소 emit helper는 Phase 1 구현 전에 common 계약에 additive follow-up으로 반영한다. 이는 Phase 0의 C-compatible header-only 경계를 변경하지 않는다.

#### Generated C++ 계약

- Schema마다 `<PacketName>.generated.h` 하나를 생성한다.
- 생성 header는 Packet Tool이 제공하는 public C++17 header-only `pstk/packet/TkPacketCodecSupport.h`를 include한다.
- Codec support는 byte view 검증, little-endian read/write, signed 변환, `TkResult` mapping과 diagnostic emit만 제공한다.
- Generated DTO는 상속, virtual dispatch와 reflection 없이 packet별 field mapping을 소유한다.
- Consumer에게 공개하는 metadata는 `PacketId`, `PayloadVersion`, `PayloadBytes`뿐이다. Field offset/size와 endian helper는 public 계약으로 노출하지 않는다.
- DTO는 `const` Encode member와 Decode member를 제공하고 optional `TkDiagnosticCallbackInfo diagnosticCallback = {}`를 받는다.
- Decode는 임시 객체에 성공적으로 해석한 뒤에만 `*this`에 commit한다.

크기와 결과 mapping:

| 조건 | 결과 |
|---|---|
| `data == nullptr && size > 0`인 view | `TK_ERROR_INVALID_ARGUMENT` |
| Encode output이 `PayloadBytes`보다 작음 | `TK_ERROR_BUFFER_TOO_SMALL` |
| Encode output이 `PayloadBytes`보다 큼 | `TK_ERROR_INVALID_ARGUMENT` |
| Decode payload 크기가 정확히 일치하지 않음 | `TK_ERROR_INVALID_DATA` |
| Decode payload version 불일치 | `TK_ERROR_INVALID_DATA` |

모든 실패는 output object와 output buffer를 호출 전 상태로 유지하고 diagnostic message에 expected/actual 값을 포함한다. Codec은 wire 구조, 크기와 version만 검증하며 gameplay 값의 유효성은 검사하지 않는다.

#### Deterministic output과 file commit

- UTF-8, LF와 generator가 직접 만드는 고정 formatting을 사용한다.
- 고정 banner는 `// Generated by PrivateServerToolKit. Do not edit.`다.
- Timestamp, 절대 source 경로와 machine 정보는 생성물에 기록하지 않는다.
- 외부 clang-format 실행에 의존하지 않는다.
- Batch 전체를 parse, validate하고 모든 header를 메모리에 생성한 뒤 final file write를 시작한다.
- 기존 file과 새 최종 내용을 byte 단위로 직접 비교하고 같으면 다시 쓰지 않는다. Hash comment나 hash 비교는 사용하지 않는다.
- 내용이 다르면 같은 directory의 temporary file에 완전한 내용을 쓴 뒤 rename한다.
- 개별 header가 부분 내용으로 남지 않는 file 단위 atomicity를 보장하되 여러 header rename 전체가 하나의 filesystem transaction이라고 보장하지 않는다.

### 구현 slice와 순서

각 slice는 새로운 public layer나 전용 result type을 뜻하지 않는다. Phase 1을 독립적으로 compile, test, review하고 commit할 수 있는 순서로 나눈 것이다. 뒤 slice는 앞 slice의 검증된 산출물만 사용한다.

Phase 1의 compiler pipeline은 `schema path → PacketSource → PacketJsonParser → PacketSchema → batch validation → PacketDescriptorBuilder → PacketDescriptorSet` 순서의 공통 schema compiler와, 이를 소비하는 언어별 generator로 구성한다. 현재는 C++ generator orchestration만 연결하고 C# generator는 후속 Phase에서 같은 `PacketDescriptorSet`을 사용한다.

#### Slice 1 — 공용 계약 보완

- `TkResult`에 기존 숫자값을 바꾸지 않고 `TK_ERROR_IO = -6`을 추가한다.
- `sourceName != nullptr`이고 나머지 위치값이 모두 0인 Diagnostic location 의미를 common 계약 문서와 테스트에 반영한다.
- Common은 C-compatible header-only 상태를 유지하고 disabled callback 확인과 동기 호출만 수행하는 최소 emit helper를 제공한다. File I/O 또는 Packet 동작은 추가하지 않는다.

검증 게이트: C11/C++17 common contract compile과 기존 Common/Packet regression test가 통과한다.

#### Slice 2 — Schema 입력과 parsing

- `TkPacketCompileInfo` 공용 parameter block과 `TkPacketCompileCpp` public API를 추가한다. 함수는 언어별로 분리하지만 공통 compile parameter block을 공유하고 language enum/option bag은 쓰지 않는다.
- 공통 `PacketSchemaCompiler`의 file adapter가 `PacketSource`를 만들고, `PacketJsonParser`가 nlohmann/json을 사용해 `PacketFieldSchema`를 포함한 `PacketSchema`로 변환한다. `PacketSchema`는 후속 descriptor와 diagnostic을 위해 `sourceName`을 유지한다.
- Required/unknown property, duplicate key, version, name, ID, field type과 batch duplicate를 정해진 순서로 fail-fast 검증한다.
- 확정된 Packet diagnostic ID와 source/logical path message를 사용한다.

검증 게이트: valid schema와 read/syntax/schema 오류 fixture가 정확한 `TkResult`와 첫 diagnostic을 반환하며 output directory에 파일을 만들지 않는다.

#### Slice 3 — Descriptor와 layout

- 공통 `PacketDescriptorBuilder`가 검증된 `PacketSchema`를 language-neutral `PacketDescriptor` / `FieldDescriptor` IR로 변환하고, `PacketSchemaCompiler`가 입력 순서대로 `PacketDescriptorSet`에 누적한다.
- Payload version offset, field offset/size와 `PayloadBytes`를 한 번 계산해 IR에 저장한다.
- C++ 및 후속 C# generator는 JSON parser나 schema 검증을 직접 호출하지 않고 동일한 `PacketDescriptorSet`을 입력으로 사용한다.
- 빈 field 목록, 8개 integer type과 layout overflow를 검증한다.

검증 게이트: 모든 primitive type, 빈 packet과 `MovementInput`의 offset/size/`PayloadBytes`가 예상값과 일치한다.

#### Slice 4 — C++17 code generation

- `TkPacketCodecSupport.h`의 공용 header-only primitive codec을 구현한다.
- IR만 소비하는 deterministic C++ generator로 DTO, metadata와 member Encode/Decode를 생성한다.
- Signed 최소값, `-1`, `0`, 최대값을 포함해 unsigned 조립과 2의 보수 변환을 검증한다.

검증 게이트: 생성 header가 C++17 translation unit으로 compile되고 Encode/Decode round trip과 failure atomicity test가 통과한다.

#### Slice 5 — 생성 파일 commit

- Batch 전체 header를 메모리에 생성한다.
- 기존 파일과 byte 비교 후 동일하면 write를 생략한다.
- 변경 파일은 temporary sibling file과 rename으로 commit한다.
- 생성 또는 write 실패 시 불완전한 final file을 남기지 않는다.

검증 게이트: 동일 입력의 결과가 byte-identical하고 두 번째 실행이 기존 file을 다시 쓰지 않으며 write failure가 기존 final file을 보존한다.

#### Slice 6 — `MovementInput` end-to-end 검증 (merged)

전용 `MovementInput.generated.h` snapshot, 14-byte payload fixture와 기존 수기 codec 직접 비교는 추가하지 않는다. 해당 위험은 이미 다음 검증 seam에서 나눠 관찰할 수 있어 전용 fixture의 유지 비용을 추가하지 않기로 했다.

- `BuildsMovementInputLayout`은 field offset과 `PayloadBytes == 14`를 검증한다.
- Public compiler test는 schema에서 generated header가 생성되는지 검증한다.
- Generated codec test는 little-endian, signed boundary, exact-size와 failure atomicity를 검증한다.
- File commit test는 byte-identical output의 write 생략과 write failure 시 기존 final 보존을 검증한다.

이 slice의 완료 근거는 새로운 통합 fixture가 아니라 위 기존 검증의 조합으로 Phase 1 계약을 커버하는지 확인한 것이다.

### 현재 상태와 다음 세션 진입점

Phase 1 design grilling은 2026-08-26 완료했고 Phase 1 구현은 2026-08-29 완료 처리했다. Slice 1부터 Slice 5까지의 공용 계약, schema compiler, descriptor/layout, C++17 code generation과 생성 파일 commit을 구현했다. Slice 6은 전용 fixture를 추가하지 않고 기존 schema/layout, public compiler, generated codec와 file-commit 검증에 병합한 축소 기준으로 완료했다.

GitHub Issue #1의 Phase 1 diagnostic 문구도 이 문서의 계약과 동일하게 source 경로와 logical schema path를 제공하되 정확한 line/column은 계산하지 않는 범위로 갱신했다.

## Phase 2 — C# source generation

### 목적과 consumer 경계

Phase 2는 Phase 1의 `PacketDescriptorSet`을 재사용해 C# fixed-layout DTO와 codec source를 생성한다. JSON parser, schema validation과 layout을 C# generator에서 다시 구현하지 않는다.

Compiler API consumer와 generated-code consumer는 다른 계약을 사용한다. 언어별 의존성의 상위 기준은 [generated-code consumer ADR](../adr/0005-generated-code-consumer-boundary.md)을 따른다.

- `TkPacketCompileCpp`/`TkPacketCompileCSharp`를 호출하는 compiler consumer는 공용 `TkPacketCompileInfo` parameter block과 ToolKit의 C ABI, `TkResult`와 Diagnostic을 사용한다. 함수는 언어별로 분리하지만 공통 compile parameter block을 공유하고 language enum/option bag은 쓰지 않는다.
- Generated C# source를 사용하는 client는 ToolKit shared library, `TkResult` 또는 `TkDiagnostic`에 의존하지 않는다.
- 언어 간 공통 계약은 packet ID, payload version, payload size, field layout과 wire bytes다. API 표현은 각 언어에 맞게 다를 수 있다.
- C++은 ToolKit의 주 대상이므로 기존 generated header의 `TkResult`, Diagnostic과 `TkPacketCodecSupport.h` 의존을 유지한다.

Generated C# source의 최소 runtime target은 `net8.0`이다. CLI와 언어별 support 전달은 Phase 3에서 다루며, 실제 consumer build integration은 해당 consumer 작업에 남긴다.

### 언어 독립 generation seam

Phase 1의 `CompileCpp` 구현에서 schema compile, 언어별 source generation과 file commit을 분리한다.

```text
language-specific public compile API
  -> PacketCompiler
       -> PacketSchemaCompiler
       -> PacketDescriptorSet
       -> PacketCodeGenerator::Generate(PacketDescriptor)
       -> GeneratedFile
       -> GeneratedFileCommitter
```

- `PacketCompiler`, `PacketCodeGenerator`와 `GeneratedFile`은 Packet Tool 내부 C++ 타입으로 유지하고 DLL ABI에 export하지 않는다.
- Root `common`은 기존 C-compatible header-only 계약만 소유하고 Packet generation 오케스트레이션을 소유하지 않는다.
- 공통 `PacketCompiler`는 schema compile부터 생성 파일 commit까지 순서와 failure atomicity를 소유한다.
- 언어별 generator는 `PacketDescriptor` 하나를 `GeneratedFile` 하나로 변환한다. Packet 개수에는 별도 상한을 두지 않는다.
- Batch의 모든 source를 메모리에서 완성한 뒤 file commit을 시작한다.
- 기존 byte comparison, unchanged-file write 생략, temporary sibling file과 replace 계약을 언어에 관계없이 재사용한다.

Public API 함수는 언어별로 분리하지만 공통 `TkPacketCompileInfo` parameter block을 공유하며, 범용 language enum과 option bag은 사용하지 않는다.

```c
typedef struct TkPacketCompileInfo
{
    const char *const *inputPaths;
    size_t inputPathCount;
    const char *outputDirectory;
    const char *namespaceName;
    TkDiagnosticCallbackInfo diagnosticCallback;
} TkPacketCompileInfo;

PSTK_PACKET_API TkResult TkPacketCompileCSharp(
    const TkPacketCompileInfo *compileInfo);
```

Pointer, string, callback, invalid argument, diagnostic과 file I/O 계약은 `TkPacketCompileCpp`와 동일하다. 이 `TkResult`는 compiler operation의 결과이며 generated C# codec에 노출되지 않는다.

### Generated C# 계약

- Schema마다 `<PacketName>.generated.cs` 하나를 생성한다.
- C# namespace는 `TkPacketCompileInfo::namespaceName`으로 전달하고 schema에 저장하지 않는다. Phase 2 구현은 필수이며, P3-S1에서 null/empty일 때 namespace wrapper를 생략하도록 변경한다.
- DTO는 `public readonly record struct`로 생성한다.
- Generated C#의 public property는 Schema의 field name을 변환하지 않고 그대로 사용한다. 언어별 naming convention을 위한 case 변환이나 naming style 정규식 검사는 추가하지 않는다.
- `PacketId`, `PayloadVersion`과 `PayloadBytes`를 공개 constant로 생성한다.
- DTO 내부에 static `TryEncode(value, Span<byte> output)`과 `TryDecode(ReadOnlySpan<byte> input, out value)`를 생성한다.
- Codec은 `bool`만 반환하고 ToolKit의 `TkResult`, Diagnostic callback과 shared library를 요구하지 않는다.
- `TryEncode`는 크기 또는 인자 검증에 실패하면 output buffer를 변경하지 않는다.
- `TryDecode`는 크기가 정확하지 않거나 payload version이 다르면 `false`와 `default` output을 반환한다.
- 16/32/64-bit signed·unsigned integer는 `System.Buffers.Binary.BinaryPrimitives`의 little-endian operation을 사용하고 8-bit integer는 직접 indexing한다.
- Gameplay semantic validation은 generated codec에 추가하지 않는다.

`TkPacketCodecSupport.cs`는 packet마다 생성하지 않고 `tools/packet/support/csharp/`의 고정 source로 유지한다. Phase 3 배포 폴더에 이 source를 함께 제공하며, consumer가 generated C# source와 함께 자신의 .NET 프로젝트에서 컴파일한다.

### 검증 경계

Phase 2는 C++로 작성된 generator test에서 C# source 생성 계약만 검증한다.

- Output 파일 이름과 존재
- 동일 descriptor의 byte-identical source
- UTF-8, LF와 generated banner
- namespace, DTO, metadata, 8개 integer type mapping과 static `TryEncode`/`TryDecode` 구조

전체 generated source snapshot은 추가하지 않고 위 핵심 구조만 간단히 확인한다. C# compile, Encode/Decode 실행, signed boundary와 golden byte conformance는 Phase 2 완료 조건에 포함하지 않는다.

CMake/CTest는 `dotnet`을 탐색하거나 `.csproj`를 build/test하지 않는다. Issue #3은 별도 `.csproj`와 언어 전용 `dotnet build`/`dotnet test` 흐름을 소유한다.

### 구현 slice와 순서

#### P2-S1 — 언어 독립 generation pipeline 추출

- Outcome: 기존 C++ 동작을 변경하지 않고 `PacketCompiler`, `PacketCodeGenerator`, `GeneratedFile`과 언어 독립 file committer를 Packet Tool 내부에 추출한다.
- Dependency: Phase 1 완료.
- Seam: `PacketDescriptorSet` 이후의 language-specific source generation.
- Invariant: C++ public API, generated header 파일명·내용, failure atomicity와 commit 정책을 유지하고 root `common`을 확장하지 않는다.
- Acceptance: 기존 C++ compile path가 공통 pipeline을 사용하며 C++ compiler/generator/file-commit 검증이 기존과 동일하게 통과한다.
- Verification: 새 세부 테스트를 추가하지 않고 기존 Packet C++ 테스트를 regression gate로 사용한다.

#### P2-S2 — C# source generator와 public compile path

- Outcome: `PacketDescriptor`를 `<PacketName>.generated.cs`로 변환하는 C# generator, 고정 codec support source, `TkPacketCompileInfo`와 `TkPacketCompileCSharp`를 연결한다.
- Dependency: P2-S1 완료.
- Seam: 언어별 public compile API와 `PacketCodeGenerator` adapter.
- Invariant: Generated C#은 ToolKit result/diagnostic/runtime을 요구하지 않고 descriptor의 offset, size와 metadata를 재계산하지 않는다.
- Acceptance: 유효한 schema batch에서 packet별 deterministic `.generated.cs`가 생성되고 공개 API 실패가 기존 `TkResult`/Diagnostic/file commit 계약을 따른다.
- Verification: C++ generator/public compiler 테스트가 파일 생성, deterministic source와 핵심 C# 구조만 검증하며 `.NET` compile과 실행은 하지 않는다.

### 현재 상태와 다음 세션 진입점

Phase 2 design grilling과 P2-S1/P2-S2 구현은 2026-08-29 완료했다. C++/C# public compiler와 generator 관련 focused CTest 17개가 통과했으며, `.NET` compile과 runtime wire conformance는 Phase 2 완료 근거에 포함하지 않고 Issue #3으로 넘겼다. 이후 Issue #3도 완료했으며, 현재 다음 작업은 Phase 3의 P3-S1이다.

### 분리된 후속 Issue #3

[Issue #3 — Generated C# codec .NET build와 C++/C# wire conformance](https://github.com/jammer-droid/PrivateServerToolKit/issues/3)가 소유하는 범위:

- Generated C# source와 `TkPacketCodecSupport.cs`를 포함하는 별도 `.csproj`
- `dotnet build` 및 `dotnet test`
- 언어 독립 golden byte vector
- C++ Encode/C# Decode와 C# Encode/C++ Decode의 wire parity
- Signed/unsigned integer boundary와 C# `TryEncode`/`TryDecode` 실행 계약

Issue #3은 CMake/CTest에 `.NET` 실행을 등록하지 않고 자신의 `.NET` build/test 명령을 소유한다. 해당 issue는 2026-08-29 완료했으며 검증 근거는 [Issue #3 디자인 문서](issue-3-generated-csharp-conformance.md)에 기록했다. Issue #1의 남은 Phase 3은 이 결과를 재사용해 CLI와 support 배포를 진행한다.

### 후속 Service Host

Generated packet을 typed service 호출로 연결하는 application-facing I/O, middleware pipeline, command/event dispatch와 async unary lifecycle은 Issue #1의 Phase 4에서 제거하고 [Issue #2](https://github.com/jammer-droid/PrivateServerToolKit/issues/2)로 이관했다. Issue #2는 Issue #1의 C++/C# codec과 CLI/support 배포를 기반으로 별도 목표, 책임 경계, 설계 refinement와 완료 조건을 소유한다.

## Phase 3 — C++ CLI와 배포

### 목적과 책임 경계

패킷 생성은 게임 runtime이 아니라 개발·빌드 과정의 전처리 작업이다. Consumer가 ToolKit 소스를 직접 빌드하거나 compiler API 호출 프로그램을 따로 작성하지 않아도 미리 빌드한 CLI로 packet source를 생성할 수 있게 한다.

```text
INI 설정
  -> C++ CLI: 설정 해석과 입력 경로 수집
  -> TkPacketCompileInfo
  -> TkPacketCompileCpp / TkPacketCompileCSharp
  -> 기존 shared compiler: schema -> descriptor -> source -> file commit
```

- 기존 `pstk_packet` shared library를 generator engine으로 유지한다. CLI에 schema parser, layout, generator 또는 file committer를 복제하지 않는다.
- CLI는 INI, 경로 수집, compile info 구성, 언어별 API 선택과 결과/diagnostic 출력을 담당하는 얇은 실행 파일이다. .NET CLI나 새 범용 tool framework는 추가하지 않는다.
- INI parser는 vcpkg의 [SimpleIni](https://github.com/brofield/simpleini)를 사용한다. CLI의 private dependency이며 Common과 shared compiler에는 INI 처리를 추가하지 않는다.
- Compiler API consumer와 generated-code consumer의 의존성은 [ADR 0005](../adr/0005-generated-code-consumer-boundary.md)를 유지한다. Native shared library는 생성 시 CLI가 사용하며, generated packet을 쓰는 게임 runtime에 링크를 강제하지 않는다.
- Generated C++은 Common/codec support header를 include하고, generated C#은 고정 C# support source와 함께 컴파일한다. C# consumer에 `TkResult`, Diagnostic 또는 native DLL binding을 추가하지 않는다.

### 실행과 INI 계약

설정 파일 하나를 명시적으로 전달한다. 초기 버전은 설정 자동 탐색이나 CLI option override를 추가하지 않는다.

```sh
pstk-packet packet.ini
```

```ini
[packet]
language=cpp
input=./schemas
output=./generated/cpp
namespace=
```

- `language`: 필수, `cpp` 또는 `csharp`. 한 번의 실행은 한 언어만 생성한다.
- `input`: 필수, 단일 `.json` 파일 또는 schema directory.
- `output`: 필수, 생성 파일을 기록할 directory.
- `namespace`: 선택. 생략하거나 빈 값이면 generated DTO를 global namespace에 둔다. 값이 있으면 선택한 언어의 namespace로 그대로 전달한다.
- C++/C#을 모두 생성하려면 각각의 설정으로 실행한다. Public ABI에 language enum이나 새 compile info type을 추가하지 않는다.

### 경로와 입력 수집

- 상대 `input`/`output`은 실행 working directory가 아니라 **INI 파일이 있는 directory**를 기준으로 해석한다. 절대 경로는 그대로 사용한다.
- `.json` 파일을 지정하면 해당 파일 하나만 처리한다.
- Directory를 지정하면 하위 directory까지 재귀 탐색하여 `.json` 파일을 수집한다.
- 수집한 경로를 정렬해 순서를 고정하고 기존 public compile API에 한 batch로 전달한다. Shared compiler는 directory를 탐색하지 않고 명시적인 파일 목록을 받는다.
- Schema 검증, batch name/ID 중복 확인, 첫 오류 중단, deterministic output과 file commit은 기존 shared compiler 계약을 재사용한다.
- 출력은 기존 packet별 `<PacketName>.generated.h` 또는 `<PacketName>.generated.cs`다. CLI가 별도 codec 형식을 만들지 않는다.

### Optional namespace 변경 계약

- `TkPacketCompileInfo::namespaceName`은 `nullptr` 또는 빈 문자열이면 namespace 생략을 뜻하도록 변경한다. Struct layout과 언어별 public function signature는 유지한다.
- C++/C# generator 모두 namespace 선언과 대응하는 wrapper를 생략하고 global namespace에 DTO를 생성한다. C++ anonymous namespace로 대체하지 않는다.
- Non-empty namespace를 전달하는 기존 동작과 나머지 필수 인자 검증은 유지한다.
- 이 항목은 현재 구현과 달라지는 새 shared-library 동작이다. P3-S1에서 compiler/generator 검증을 보완하고 CLI 검증과 구분한다.

### 배포와 consumer 책임

플랫폼별로 미리 빌드한 다음 형태의 폴더를 전달한다. 아래는 배포 layout 계약이며 아직 생성된 산출물이 아니다.

```text
pstk-packet/
  bin/
    pstk-packet 실행 파일
    pstk_packet shared library
  include/
    pstk/...                     # C++ Common/codec support public headers
  support/
    csharp/
      TkPacketCodecSupport.cs
```

- 초기 배포·실행 확인 대상은 macOS arm64와 Windows x64다. 각 플랫폼에서 native build와 실행을 확인하고, macOS 결과로 Windows 검증을 대신하지 않는다.
- 배포 폴더의 CLI가 함께 제공한 shared library를 찾고 실행할 수 있어야 한다. Consumer가 ToolKit source/build tree를 가져오도록 요구하지 않는다.
- Linux와 다른 architecture는 필요 시 후속으로 다룬다. 이 Phase에서 자동 Release 업로드나 별도 installer까지 확장하지 않는다.
- 이 저장소의 빌드·실행 확인용 생성물은 기존처럼 `out/build` 아래에 두며 Git에서 제외한다.
- 외부 consumer는 자신의 출력 경로, generated source의 Git 포함 여부, 실행 시점과 build/CI hook을 결정한다. 매 빌드마다 생성하도록 강제하지 않는다.
- 기존 final과 byte-identical하면 write를 생략하는 정책은 그대로 재사용한다. 삭제되거나 이름이 바뀐 schema의 오래된 생성물을 자동 삭제하는 기능은 추가하지 않는다.
- PrivateServer/Godot의 실제 적용과 통신 확인은 consumer 작업이며 Issue #1의 완료 조건이 아니다.

### 검증 경계

CLI는 **INI 설정이 실제 생성에 반영되는지**만 직접 실행해 확인한다. 별도 CLI 단위 테스트 파일, 테스트 target이나 전용 테스트 harness는 만들지 않으며 이미 검증된 codec/generator 내용을 CLI에서 재검증하지 않는다.

- `cpp`/`csharp` 선택에 맞는 파일이 지정한 output에 생성되는지 확인한다.
- 단일 파일과 재귀 directory 입력이 각각 반영되는지 확인한다.
- INI와 다른 working directory에서 실행해도 상대 input/output 기준이 유지되는지 확인한다.
- Namespace 지정/생략이 생성 결과에 반영되는지 확인한다.
- 배포 폴더의 CLI로 위 설정 적용을 확인하고 사용 명령과 결과를 기록한다. 새로운 namespace 생략 동작 자체의 검증은 P3-S1의 library/generator 범위에서 담당한다.

### 구현 slice와 순서

순서는 **P3-S1 → P3-S2 → P3-S3**이다. 기존 Phase 번호는 유지하고 consumer integration 작업을 아래 CLI 범위로 대체한다.

#### P3-S1 — C++/C# namespace 선택 지원

- Outcome: Public compile API와 두 generator가 namespace 없는 source 생성을 지원한다.
- Dependency: Phase 2와 Issue #3 완료.
- Seam: `TkPacketCompileInfo::namespaceName`, `tools/packet/src/generator/cpp/`, `tools/packet/src/generator/csharp/`와 기존 Packet 테스트.
- Invariant: ABI layout, wire layout, non-empty namespace 생성 내용과 file commit 정책을 유지한다.
- Acceptance: null/empty namespace가 성공하고 두 언어 모두 namespace wrapper 없이 DTO를 생성한다. 기존 namespace 지정 경로도 유지된다.
- Verification: 기존 compiler/generator 검증에 namespace 생략 case만 보완한다. 생성 source가 해당 언어에서 컴파일되는지 확인하되 .NET은 기존 독립 toolchain을 사용하며 CMake/CTest에 연결하지 않는다.

#### P3-S2 — INI 기반 C++ CLI

- Outcome: `pstk-packet <config.ini>`가 설정과 입력 경로를 compile info로 변환하고 shared compiler를 호출한다.
- Dependency: P3-S1 완료.
- Seam: Packet CLI 실행 파일과 CMake 연결, root `vcpkg.json`의 SimpleIni dependency, 기존 언어별 public compile API.
- Invariant: INI/경로 탐색은 CLI에만 두고 shared compiler와 Common의 책임을 확장하지 않는다. 정렬한 입력 전체를 한 batch로 전달한다.
- Acceptance: 언어, 단일/재귀 입력, INI 기준 상대 경로, 출력과 optional namespace가 생성 결과에 반영되고 compiler 결과/diagnostic이 사용자에게 전달된다.
- Verification: `out/build` 아래의 설정과 schema로 CLI를 직접 실행해 설정 적용을 확인한다. 새 CLI 테스트 suite나 codec 재검증은 추가하지 않는다.

#### P3-S3 — 배포 폴더와 사용 가이드

- Outcome: Prebuilt CLI/shared library와 언어별 support를 배포 layout으로 모으고 INI 예제 및 실행 방법을 문서화한다.
- Dependency: P3-S2 완료.
- Seam: CMake의 배포 산출물 구성, Common/Packet public support, C# support와 README/사용 문서.
- Invariant: 실제 게임에 generator shared library를 요구하지 않으며 consumer의 build/CI/Git 정책을 대신 결정하지 않는다.
- Acceptance: macOS arm64와 Windows x64에서 배포 폴더의 CLI가 shared library를 로드하고 INI에 맞는 결과를 생성한다. 필요한 C++ header와 C# support가 함께 제공된다.
- Verification: 각 대상 플랫폼의 배포 폴더에서 설정 적용 실행을 확인하고 결과를 기록한다. PrivateServer/Godot 빌드·통신 검증이나 별도 CLI 테스트 harness는 포함하지 않는다.

### 현재 상태와 다음 세션 진입점

- Design: 2026-08-31 grilling 완료. 기존 tracker/design의 consumer integration 범위를 이 Phase의 CLI 계약으로 변경한다.
- Implementation: **0/3 완료**. 현재 코드는 namespace를 필수로 검사하며 CLI와 배포 규칙은 아직 없다.
- Next: **P3-S1**의 namespace 인자 검증과 C++/C# wrapper 생성을 수정하는 가이드부터 진행한다.
- 검증: 이번 문서 반영은 구현·빌드·실행 결과가 아니다. 특히 Windows x64 배포 검증을 완료한 것으로 간주하지 않는다.
- 보류: Consumer별 적용/build hook/stale 관리와 추가 플랫폼. CLI 파일 배치와 CMake 배포 명령의 세부 형태는 각 slice 구현 가이드에서 최소한으로 정한다. 현재 계약을 막는 미해결 결정은 없다.

## Issue 완료 기준

Issue 전체 완료 기준은 GitHub Issue #1을 source of truth로 삼으며, 2026-08-31 확정한 범위는 다음과 같다.

- C++/C# 생성과 Issue #3의 golden byte conformance가 완료된다.
- Prebuilt C++ CLI가 INI 설정을 읽어 언어, 입력, 출력과 optional namespace에 맞는 결과를 생성한다.
- macOS arm64/Windows x64 배포 폴더에 CLI, shared library와 필요한 언어별 support가 포함되고 각 플랫폼에서 설정 적용 실행을 확인한다.
- Public API, schema/generated-code 계약, 배포 구성과 INI 사용 예제가 문서화된다.
- 기존 transport/runtime ownership을 침범하지 않는다. 두 consumer의 실제 build 연결이나 request/event 교환은 완료 조건에 포함하지 않는다.
