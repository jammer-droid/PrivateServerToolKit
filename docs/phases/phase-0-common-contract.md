# Phase 0: Common Contract

## 목적

Phase 0은 Packet Tool을 포함한 여러 tool과 consumer가 같은 기초 타입과 C++ API 규칙을 사용하도록 common 계약을 고정하는 단계다. Common은 별도 runtime을 소유하지 않고 header-only `PSTK::Common` INTERFACE target으로 유지한다.

이 문서는 Phase 1 Packet Compiler가 의존할 공용 계약의 범위와 완료 기준을 정의한다. 각 결정의 배경과 상세는 연결된 ADR을 따른다.

## 계약 범위

### Byte view

- `TkByteView`: `const uint8_t* data`와 `size_t size`를 가지는 읽기 전용 non-owning POD
- `TkMutableByteView`: `uint8_t* data`와 `size_t size`를 가지는 쓰기 가능 non-owning POD
- `size == 0`은 `data`와 관계없이 유효한 empty view
- `size > 0`은 `data != nullptr`이어야 함
- `TkIsValidByteRange(const void*, size_t)`는 위 구조적 조건만 검사하는 header-only 함수
- View는 lifetime, 실제 메모리 용량 또는 packet별 크기를 검증하지 않음

상세: [ADR 0001](../adr/0001-use-byte-views-for-buffer-access.md)

### Result

- Public API와 내부 C++ 함수의 성공/실패는 공용 `TkResult`로 전파
- 모듈이나 계층별 result enum/class는 추가하지 않음
- Predicate, nullable lookup과 단순 값 계산은 각 의미에 맞는 `bool`, pointer 또는 값 타입을 사용
- 해석할 수 없는 schema와 encoded data는 `TK_ERROR_INVALID_DATA`
- 잘못된 호출 인자는 `TK_ERROR_INVALID_ARGUMENT`
- 출력 용량 부족은 `TK_ERROR_BUFFER_TOO_SMALL`
- 세부 원인은 `TkResult`를 세분화하지 않고 Diagnostic으로 전달

상세: [ADR 0003](../adr/0003-use-tkresult-for-cpp-failures.md), [C++ result handling](../agents/cpp-result-style.md)

### Failure atomicity

- `TkResult != TK_SUCCESS`이면 output object와 output buffer는 호출 전 상태를 유지
- Decode는 임시 객체를 검증한 뒤 성공 시에만 output에 commit
- Encode는 모든 입력과 크기를 검증한 뒤 output buffer를 기록
- Compiler는 실패한 중간 IR이나 불완전한 생성물을 output으로 commit하지 않음
- 실패 시에도 필요 buffer 크기 등을 갱신하는 API는 해당 정보성 output 예외를 명시해야 함

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

### Fixed-layout packet boundary

- Generated codec은 byte view 크기가 packet의 `PayloadBytes`와 정확히 같을 때만 Encode/Decode
- `PayloadBytes`는 payload version과 선언 field를 포함하고 NetworkRuntime의 6-byte transport header는 제외
- Phase 1 vertical slice인 `MovementInput`의 `PayloadBytes`는 14

이 항목은 common byte view를 사용하는 Packet Tool 계약이며 common 타입 자체의 책임은 아니다.

상세: [ADR 0002](../adr/0002-require-exact-fixed-layout-payload-size.md)

## 책임 경계

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

## Phase 0 완료 기준

다음 조건이 모두 충족되면 Phase 0의 common 계약과 구현이 완료된다.

1. `TkResult.h`가 확정된 공용 result 값과 `TK_ERROR_INVALID_DATA`를 정의한다.
2. `TkByteView.h`가 C-compatible byte view POD와 `TkIsValidByteRange`를 정의한다.
3. `TkDiagnostic.h`가 diagnostic, location, callback과 sink POD를 정의한다.
4. Common header를 별도 binary 연결 없이 C와 C++ translation unit에서 각각 compile한다.
5. Test가 byte range의 null/empty 규칙, disabled diagnostic sink 표현과 공용 enum/POD의 기본 계약을 검증한다.
6. Phase 1의 첫 generated MovementInput codec 테스트가 실패 시 output 보존과 정확한 14-byte payload 규칙을 검증한다.

## 현재 상태

공용 계약의 설계와 문서화는 완료되었다. Header 구현과 compile/test 검증은 아직 남아 있으며, 이 구현 조건을 충족하기 전에 Phase 0 전체를 완료로 판정하지 않는다.

Schema 문법, primitive type, parser/IR API, code generation 출력 형식, Packet diagnostic ID catalog과 deterministic output은 Phase 1 Packet Tool 계약으로 다룬다.
