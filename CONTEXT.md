# PrivateServerToolKit

PrivateServer가 사용하는 플랫폼 독립 native tool과 공용 데이터 계약을 제공한다.

## Language

**Common Layer**:
여러 tool이 공유하는 기초 타입과 작은 연산을 제공하는 계층이다. Tool별 동작을 모으지 않으며 가능한 경우 별도 binary 의존성이 없는 가벼운 계약으로 유지한다.
_Avoid_: Misc utilities, tool implementation

**Byte View**:
소유권을 갖지 않고 읽기 전용 byte 메모리의 시작 주소와 byte 수를 전달하는 공용 값이다. Byte 수가 0이면 시작 주소와 관계없이 empty view다.
_Avoid_: Span, buffer owner

**Mutable Byte View**:
소유권을 갖지 않고 쓰기 가능한 byte 메모리의 시작 주소와 byte 수를 전달하는 공용 값이다. Byte 수가 0이면 시작 주소와 관계없이 empty view다.
_Avoid_: Mutable Span, output buffer owner

**Fixed-layout Payload**:
Payload version과 선언된 field가 순서대로 차지하는 정확한 semantic payload byte 구간이다. NetworkRuntime transport header는 포함하지 않는다.
_Avoid_: Transport frame, minimum buffer size

**TkResult**:
C++ 호출자가 다음 제어 흐름을 결정할 수 있도록 성공, 상태 또는 실패 분류를 전달하는 프로젝트 공용 결과다. 상세 진단 정보는 포함하지 않는다.
_Avoid_: Module result, layer result

**Diagnostic**:
`TkResult`와 분리하여 오류나 경고의 안정적인 식별자, 심각도, 위치와 설명을 전달하는 tool 공용 정보다. 개별 tool은 공통 포맷을 다시 정의하지 않는다.
_Avoid_: Tool diagnostic format, detailed result

**Diagnostic Callbacks**:
Callback signature와 user data를 묶어 Tool이 생성한 borrowed Diagnostic을 전달하기 위한 common 계층의 type이다. Common은 호출 구현을 제공하지 않으며 각 Tool이 Callback을 호출하고, Callback 이후 보관할 정보는 consumer가 복사한다.
_Avoid_: Tool logger, diagnostic owner

**Failure Atomicity**:
`TkResult`를 반환하는 함수가 실패했을 때 호출자가 제공한 output object와 output buffer에 부분 결과를 남기지 않는 공용 API 규칙이다. API가 필요 크기 같은 정보성 output을 실패에서도 제공하려면 해당 예외를 명시해야 한다.
_Avoid_: Partial output, undocumented failure mutation
