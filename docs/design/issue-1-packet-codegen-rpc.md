# GitHub Issue #1: 스키마 기반 C++/C# 패킷 코드 생성기와 unary RPC 계층 구현

- Issue: [jammer-droid/PrivateServerToolKit#1](https://github.com/jammer-droid/PrivateServerToolKit/issues/1)
- Issue state: Open
- Last verified: 2026-08-25

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

Phase 0 common contract 선행 slice는 구현과 검증을 완료했다. 다음 작업은 Phase 1 진입 전 남은 schema/parser/IR/generator 계약을 확정하는 것이다. Phase 2~4는 Issue #1의 상위 범위를 따르며 해당 구현에 진입할 때 이 문서에 세부 design을 추가한다.

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
- `TkDiagnosticCallbacks`: callback function pointer와 `userData`를 가지는 POD
- `callback == nullptr`은 diagnostic을 버리는 유효한 disabled callbacks
- Callback은 tool API를 호출한 thread에서 API 반환 전에 동기적으로 실행
- Diagnostic과 문자열은 callback 동안만 유효하며 consumer가 필요한 정보를 복사
- 하나의 API 호출은 callback을 순차 실행하지만, 동일 callbacks를 사용한 별도 API 호출들은 동시에 callback을 실행할 수 있음
- `id`는 non-empty ASCII `PSTK-<TOOL>-<NAME>` 형식의 안정적인 식별자
- Common은 diagnostic 타입만 제공하고 호출, 저장, 필터링, logging, allocation과 thread를 소유하지 않음

상세: [ADR 0004](../adr/0004-use-common-diagnostic-callbacks.md)

### 완료 기준과 현재 상태

다음 조건이 모두 충족되면 Phase 0 구현이 완료된다.

1. `TkResult.h`가 확정된 공용 result 값과 `TK_ERROR_INVALID_DATA`를 정의한다.
2. `TkByteView.h`가 C-compatible byte view POD와 `TkIsValidByteRange`를 정의한다.
3. `TkDiagnostic.h`가 diagnostic, location, callback과 callbacks POD를 정의한다.
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

TkDiagnosticCallbacks
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

### 작업 범위

1. Schema 형식과 version 규칙을 정의한다.
2. 위치를 보존하는 parser와 diagnostic 생성을 구현한다.
3. 언어 독립 `PacketDescriptor` / `FieldDescriptor` IR을 구현한다.
4. Duplicate packet ID/field, unknown type과 overflow semantic validation을 구현한다.
5. C++17 fixed-layout DTO와 Encode/Decode를 생성한다.
6. 동일한 입력이 byte-identical source를 생성하는지 검증한다.
7. `MovementInput` golden vector로 기존 수기 codec과 byte parity를 검증한다.

### Phase 1 진입 전 남은 design

- Schema 문법, primitive type과 schema version 규칙
- Parser input/output API와 source ownership
- Descriptor의 필수 field와 layout 계산 책임
- Semantic validation 순서와 Packet diagnostic ID catalog
- Generator output artifact·file 구조와 deterministic formatting 규칙
- Generated C++ public contract과 golden/conformance test 구조

이 항목들은 grilling을 통해 한 번에 하나씩 확정하고, 별도 Phase 문서를 만들지 않고 이 Issue #1 design 문서에 계속 반영한다.

## Issue 완료 기준

Issue 전체 완료 기준은 GitHub Issue #1을 source of truth로 삼는다. 이 문서에서는 각 Phase에 진입할 때 확정한 세부 검증 계약을 추가한다.
