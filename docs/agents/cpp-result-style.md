# C++ result handling

## 공용 결과 타입

C++ 함수가 호출자에게 성공 또는 실패를 보고해야 하면 public API와 내부 함수 모두 `TkResult`를 반환한다. Packet, parser, generator 또는 다른 계층을 위한 별도 result enum이나 result class를 만들지 않는다.

```cpp
TkResult CompileSchema(/* ... */);
TkResult EncodePacket(/* ... */);
```

- 성공은 `TK_SUCCESS`로 반환한다.
- 하위 호출의 `TkResult`를 그대로 해석할 수 있으면 새 타입으로 변환하지 않고 전파한다.
- 외부 라이브러리의 오류는 PrivateServerToolKit 경계에서 가장 가까운 `TkResult`로 변환한다.
- 새로운 `TkResult` 값은 호출자의 대응이 기존 값과 실제로 달라지고 여러 모듈에서 이해할 수 있을 때만 추가한다.
- Schema, encoded payload 또는 다른 입력 형식을 해석할 수 없으면 `TK_ERROR_INVALID_DATA`를 반환하고 구체적인 원인은 Diagnostic으로 전달한다.
- Null pointer나 잘못된 output parameter는 `TK_ERROR_INVALID_ARGUMENT`, 필요한 출력 용량 부족은 `TK_ERROR_BUFFER_TOO_SMALL`로 구분한다.

## 실패 시 output 보존

`TkResult`를 반환하는 함수는 `TK_SUCCESS`가 아닌 경우 호출자가 제공한 output object와 output buffer를 변경하지 않는다. 중간 결과가 필요하면 임시 객체나 tool 내부 저장소에서 작업한 뒤 모든 검증이 성공했을 때만 output에 commit한다.

- Decode는 임시 객체에 값을 구성하고 검증한 뒤 성공 시에만 output object에 대입한다.
- Encode는 인자, payload 크기와 값을 먼저 검증하고 성공이 확정된 뒤 output buffer를 기록한다.
- Compile은 실패한 중간 IR이나 불완전한 생성물을 output으로 commit하지 않는다.
- 필요 buffer 크기와 같은 정보성 output을 실패에서도 갱신하는 API는 그 예외를 API 계약에 명시한다. 명시되지 않은 예외와 부분 쓰기는 허용하지 않는다.

## 다른 반환 타입을 쓰는 경우

`TkResult`는 모든 함수에 강제하는 반환 타입이 아니다. 함수의 주된 결과가 성공/실패가 아니면 그 의미에 맞는 타입을 사용한다.

```cpp
bool IsValidIdentifier(/* ... */);       // predicate
const Field* FindField(/* ... */);       // nullable lookup
size_t CalculatePayloadBytes(/* ... */); // value calculation
```

- `bool`은 참/거짓 자체가 결과인 predicate에 사용한다. 실패 원인을 감추기 위한 축약으로 사용하지 않는다.
- null pointer나 빈 값은 문서화된 정상 조회 결과일 때만 사용한다. 실패와 구분해야 하면 `TkResult`와 output parameter를 사용한다.
- 복구 가능한 실패가 없는 함수는 `void` 또는 계산 결과 타입을 사용할 수 있다.

## Diagnostic 분리

`TkResult`는 호출자의 제어 흐름을 위한 작은 분류다. Schema의 line/column, 안정적인 diagnostic code, 관련 입력 위치와 설명 같은 상세 정보는 diagnostic output 또는 context를 통해 별도로 전달한다.

상세 정보를 보존하기 위해 모듈별 result 타입을 만들거나 모든 세부 원인을 `TkResult` 값으로 추가하지 않는다. Tool별 diagnostic callback이나 포맷도 만들지 않으며 common 계층의 Diagnostic과 Diagnostic Sink 계약을 사용한다.

Common diagnostic의 필드, lifetime과 tool별 ID 규칙은 [`docs/adr/0004-use-common-diagnostic-sink.md`](../adr/0004-use-common-diagnostic-sink.md)를 따른다.
