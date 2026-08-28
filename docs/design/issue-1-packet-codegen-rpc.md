# GitHub Issue #1: 스키마 기반 C++/C# 패킷 코드 생성기와 unary RPC 계층 구현

- Issue: [jammer-droid/PrivateServerToolKit#1](https://github.com/jammer-droid/PrivateServerToolKit/issues/1)
- Issue state: Open
- Last verified: 2026-08-26

## 문서 역할

GitHub Issue #1은 schema를 source of truth로 삼아 C++/C# fixed-layout packet code를 생성하고, Packet MVP 이후 같은 descriptor를 unary RPC 계층으로 확장하는 상위 범위와 완료 조건을 소유한다.

이 문서는 Issue #1의 세부 계약, 하위 Phase, 구현 순서와 검증 기준을 관리한다. 기존 `Phase 0: Common Contract`은 독립 프로젝트 Phase가 아니라 Phase 1 Packet Compiler가 의존하는 Issue #1의 선행 slice로 통합한다.

## Issue 경계

이 issue에서 생성하는 것:

- 언어 독립 message/field descriptor와 schema parser
- C++/C# fixed-layout DTO, codec와 golden vector
- Packet ID, direction과 catalog
- 후속 unary RPC stub, dispatcher와 runtime

수기 코드와 기존 runtime 책임으로 유지하는 것:

- NetworkRuntime의 6-byte transport framing과 socket/IOCP buffer lifetime
- Transport routing, session/entity ownership과 gameplay semantic validation
- RPC business/gameplay 구현

Protobuf wire format, runtime reflection, schema hot reload, gRPC/HTTP2 호환과 streaming RPC는 범위 밖이다.

## 구현 순서

1. Phase 0 — Common contract 선행 slice
2. Phase 1 — Packet compiler core와 C++ 생성
3. Phase 2 — C# 생성과 cross-language conformance
4. Phase 3 — Packet catalog과 build integration
5. Phase 4 — Async unary RPC 계층

Phase 0 common contract 선행 slice는 구현과 검증을 완료했다. Phase 1은 schema/parser, 공통 schema compiler와 언어별 generator를 순서대로 구현한다. Phase 2~4는 Issue #1의 상위 범위를 따르며 해당 구현에 진입할 때 이 문서에 세부 design을 추가한다.

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
- Common은 diagnostic 타입만 제공하고 호출, 저장, 필터링, logging, allocation과 thread를 소유하지 않음

상세: [ADR 0004](../adr/0004-use-common-diagnostic-callbacks.md)

### 완료 기준과 현재 상태

다음 조건이 모두 충족되면 Phase 0 구현이 완료된다.

1. `TkResult.h`가 확정된 공용 result 값과 `TK_ERROR_INVALID_DATA`를 정의한다.
2. `TkByteView.h`가 C-compatible byte view POD와 `TkIsValidByteRange`를 정의한다.
3. `TkDiagnostic.h`가 diagnostic, location, callback과 callback info POD를 정의한다.
4. Common header를 별도 binary 연결 없이 C와 C++ translation unit에서 각각 compile한다.
5. Test가 공용 result 값, byte range의 null/empty 규칙과 byte view의 복사·쓰기 계약을 검증한다.

Phase 0은 2026-08-25 완료했다. `TkDiagnostic.h`는 동작을 소유하지 않는 type-only 계약이므로 별도 runtime test를 추가하지 않고 C11/C++17 translation unit의 독립 compile로 호환성을 확인했다.

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

#### 3. Diagnostic type header 추가

`include/pstk/TkDiagnostic.h`에 구현 없는 공용 POD와 callback type만 정의한다. 모든 enum 값은 명시적으로 고정한다.

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
```

Common에 emit 함수, null-check helper, logger, registry, allocation 또는 thread 코드를 추가하지 않는다. Disabled callbacks는 `{ NULL, NULL }`로 표현할 수 있어야 한다.

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

Phase 1은 build-time JSON schema compiler와 C++17 생성기만 구현한다. Runtime JSON parsing, C# 생성, packet catalog, direction, unary RPC와 gameplay semantic validation은 포함하지 않는다. `MovementInput`은 예시 전용 타입이 아니라 첫 end-to-end vertical slice다.

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
- `direction`은 semantic packet 분류이므로 Phase 1 schema와 descriptor에서 제외하고 Phase 3 catalog 설계에서 다룬다.

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
typedef struct TkPacketCppCompileInfo
{
    const char *const *inputPaths;
    size_t inputPathCount;
    const char *outputDirectory;
    const char *namespaceName;
    TkDiagnosticCallbackInfo diagnosticCallback;
} TkPacketCppCompileInfo;

PSTK_PACKET_API TkResult TkPacketCompileCpp(
    const TkPacketCppCompileInfo *compileInfo);
```

- 모든 pointer와 문자열은 호출 동안만 borrowed다.
- `compileInfo`, `inputPaths`, 각 input path, `outputDirectory`, `namespaceName`은 non-null이어야 하고 문자열은 non-empty여야 하며 `inputPathCount > 0`이어야 한다. 이 호출 계약을 위반하면 diagnostic 없이 `TK_ERROR_INVALID_ARGUMENT`를 반환한다.
- C++ namespace는 schema가 아니라 required compile info로 전달한다. `namespace`는 C++ keyword이므로 field 이름은 `namespaceName`을 사용한다.
- Diagnostic callback은 값으로 전달하며 `callback == nullptr`이면 disabled다.
- 파일 open/read/write 실패에는 공용 `TK_ERROR_IO`를 사용하고 schema 또는 payload 해석 실패에는 `TK_ERROR_INVALID_DATA`를 사용한다.
- Packet 전용 result type이나 layer별 result type은 만들지 않는다.

`TK_ERROR_IO`와 source는 있지만 내부 위치를 모르는 Diagnostic location 표현은 Phase 1 구현 전에 common 계약에 additive follow-up으로 반영한다. 이는 Phase 0의 header-only/type-only 경계를 변경하지 않는다.

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
- Common은 header-only/type-only 상태를 유지하고 emit, file I/O 또는 Packet 동작을 추가하지 않는다.

검증 게이트: C11/C++17 common contract compile과 기존 Common/Packet regression test가 통과한다.

#### Slice 2 — Schema 입력과 parsing

- `TkPacketCppCompileInfo`와 `TkPacketCompileCpp` public API를 추가한다.
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

#### Slice 6 — `MovementInput` end-to-end 검증

- `MovementInput.packet.json`, 예상 `MovementInput.generated.h`와 예상 14-byte payload를 golden fixture로 저장한다.
- Generated header snapshot과 payload bytes를 정확히 비교한다.
- Generated C++ codec을 기존 수기 `MovementInput` codec과 byte-for-byte 비교한다.
- Exact size, payload version mismatch, signed boundary와 Decode failure atomicity를 함께 검증한다.

검증 게이트: CMake build와 CTest 전체가 통과하고 golden header와 payload가 예상값과 일치한다.

### 현재 상태와 다음 세션 진입점

Phase 1 design grilling은 2026-08-26 완료했다. Material design branch나 blocker는 남아 있지 않다. Slice 1부터 Slice 4까지 공용 계약, schema compiler, descriptor/layout과 C++17 code generation을 구현했으며, 다음 작업은 **Slice 5 — 생성 파일 commit**이다.

GitHub Issue #1의 Phase 1 항목에는 아직 "위치를 포함한 diagnostic"이라고 적혀 있다. 이 문서에서 확정한 Phase 1 범위는 source 경로와 logical schema path만 제공하고 정확한 line/column 계산은 하지 않는 것이다. 원격 issue를 다음에 갱신할 때 이 차이를 함께 반영한다.

## Issue 완료 기준

Issue 전체 완료 기준은 GitHub Issue #1을 source of truth로 삼는다. 이 문서에서는 각 Phase에 진입할 때 확정한 세부 검증 계약을 추가한다.
