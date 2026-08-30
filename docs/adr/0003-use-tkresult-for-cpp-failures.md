# C++ 성공과 실패는 TkResult로 통일한다

PrivateServerToolKit의 public API와 내부 C++ 함수는 호출자에게 성공 또는 실패를 전파할 때 공용 `TkResult`를 사용한다. 계층마다 전용 result 타입을 만들지 않으며, predicate나 조회처럼 성공/실패가 주된 결과가 아닌 함수만 `bool`, pointer 또는 값 타입을 사용한다.

이 결정은 Vulkan API와 Khronos Vulkan Loader가 공유 `VkResult`로 전파 가능한 실패를 표현하고 predicate에는 `bool`을 사용하는 구분을 따른다. 모듈별 result 변환과 분기 증가를 피하고 consumer가 하나의 결과 어휘를 사용하게 하는 것이 목적이다.

`TkResult`는 상세 diagnostic 저장소가 아니다. 위치, 안정적인 diagnostic code, 관련 입력과 설명은 common 계층이 소유하는 Diagnostic과 Diagnostic Callbacks 계약으로 분리한다.

Schema, encoded payload 또는 다른 입력 형식을 해석할 수 없는 경우는 공용 `TK_ERROR_INVALID_DATA`로 분류한다. Packet별 invalid length나 unsupported version result를 추가하지 않고 구체적인 원인은 Diagnostic으로 전달하며, 잘못된 호출 인자는 `TK_ERROR_INVALID_ARGUMENT`, 출력 용량 부족은 `TK_ERROR_BUFFER_TOO_SMALL`을 사용한다.

`TkResult`를 반환하는 함수는 실패 시 호출자 소유 output/in-out object, buffer와 갱신 대상 객체를 호출 전 상태로 보존하는 failure atomicity를 기본 계약으로 사용한다. 전달 방식이 pointer, reference 또는 view인지는 이 보장을 바꾸지 않는다. Decode는 임시 객체를 거쳐 commit하고 Encode는 모든 검증 후 기록하며, 필요 크기와 같은 정보성 output을 실패에서도 갱신하는 예외는 각 API가 명시해야 한다. Diagnostic과 외부 부수효과의 구분은 [실패 시 output 보존 규칙](../agents/cpp-result-style.md#실패-시-output-보존)을 따른다.
