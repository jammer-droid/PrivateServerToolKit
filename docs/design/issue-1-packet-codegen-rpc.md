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

현재 design은 Phase 0과 Phase 1 진입 계약까지 확정되었다. Phase 2~4는 Issue #1의 상위 범위를 따르며 해당 구현에 진입할 때 이 문서에 세부 design을 추가한다.

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
- `TkDiagnosticSink`: callback function pointer와 `userData`를 가지는 POD
- `callback == nullptr`은 diagnostic을 버리는 유효한 disabled sink
- Callback은 tool API를 호출한 thread에서 API 반환 전에 동기적으로 실행
- Diagnostic과 문자열은 callback 동안만 유효하며 consumer가 필요한 정보를 복사
- 하나의 API 호출은 callback을 순차 실행하지만, 동일 sink를 사용한 별도 API 호출들은 동시에 callback을 실행할 수 있음
- `id`는 non-empty ASCII `PSTK-<TOOL>-<NAME>` 형식의 안정적인 식별자
- Common은 diagnostic 타입만 제공하고 호출, 저장, 필터링, logging, allocation과 thread를 소유하지 않음

상세: [ADR 0004](../adr/0004-use-common-diagnostic-sink.md)

### 완료 기준과 현재 상태

다음 조건이 모두 충족되면 Phase 0 구현이 완료된다.

1. `TkResult.h`가 확정된 공용 result 값과 `TK_ERROR_INVALID_DATA`를 정의한다.
2. `TkByteView.h`가 C-compatible byte view POD와 `TkIsValidByteRange`를 정의한다.
3. `TkDiagnostic.h`가 diagnostic, location, callback과 sink POD를 정의한다.
4. Common header를 별도 binary 연결 없이 C와 C++ translation unit에서 각각 compile한다.
5. Test가 byte range의 null/empty 규칙, disabled diagnostic sink 표현과 공용 enum/POD의 기본 계약을 검증한다.

공용 계약의 design은 확정되었다. Header 구현과 compile/test 검증은 아직 남아 있으며 이 조건을 충족하기 전에 Phase 0를 완료로 판정하지 않는다.

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
