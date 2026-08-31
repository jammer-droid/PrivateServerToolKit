# PrivateServerToolKit

PrivateServer가 사용하는 플랫폼 독립 native tool과 공용 데이터 계약을 제공한다.

## Language

**Common Layer**:
여러 tool이 공유하는 기초 타입과 작은 연산을 제공하는 계층이다. Tool별 동작을 모으지 않으며 가능한 경우 별도 binary 의존성이 없는 가벼운 계약으로 유지한다.
_Avoid_: Misc utilities, tool implementation

**공용 실행 계층**:
여러 runtime이 공유할 작업 전달과 실행 도구를 모으는 별도 계층의 가칭이다. 가벼운 데이터 계약을 제공하는 Common Layer와 구분하며 서비스 정책이나 게임 로직은 소유하지 않는다.
_Avoid_: Common Layer, game runtime

**Service Host**:
특정 NetworkRuntime 구현과 독립적으로 패킷의 공통 검증·입력 정책을 서비스 로직에서 분리하고, 등록된 handler를 지정된 실행 영역에 연결하는 계층이다. 응답의 공통 Encode와 출력 연결도 담당하되 게임 상태와 실행 시점은 consumer에 남긴다.
_Avoid_: RPC runtime, NetworkRuntime, WorldRuntime

**Service Handler**:
Host가 연결한 타입 있는 요청을 받아 서비스 로직과 도메인 검증을 수행하는 호출 대상이다. 등록된다는 것이 서비스 객체나 게임 상태의 소유권을 Host로 이전한다는 뜻은 아니다.
_Avoid_: Packet decoder, transport callback

**Host Input Adapter**:
NetworkRuntime의 패킷과 필요한 연결 이벤트를 Host의 독립적인 입력 계약에 연결하는 역할이다. 패킷별 Decode와 검증 자체는 Host에 남긴다.
_Avoid_: Protocol validator, World executor

**Host Output Adapter**:
Host가 Encode한 데이터를 특정 NetworkRuntime의 송신 기능에 연결하는 역할이다. 송신 요청의 수락과 상대의 수신 완료를 구분한다.
_Avoid_: Completion middleware, service handler

**Service Executor**:
Host가 넘긴 요청 데이터와 handler 호출 작업을 consumer가 정한 실행 영역에 맡기는 역할이다. World executor는 이 역할을 World 쪽 실행 규칙에 연결한 것을 가리키며, 작업 접수와 실제 실행 완료는 다르다.
_Avoid_: Thread pool, transport adapter

**WorldRuntime**:
게임 서비스 입력을 tick과 ECS 등의 실행 규칙에 따라 소비하는 재사용 실행 기반의 가칭이다. 이 기반을 게임별 서비스·component·system과 조립한 실행 프로그램은 World Server로 구분한다.
_Avoid_: Service Host, World Server application

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

**Diagnostic Callback Info**:
Callback signature와 user data를 묶어 Tool이 생성한 borrowed Diagnostic을 전달하기 위한 common 계층의 type이다. Common은 호출 구현을 제공하지 않으며 각 Tool이 Callback을 호출하고, Callback 이후 보관할 정보는 consumer가 복사한다.
_Avoid_: Tool logger, diagnostic owner

**Failure Atomicity**:
`TkResult`를 반환하는 함수가 실패했을 때 호출자 소유 output/in-out object, buffer와 갱신 대상 객체를 호출 전 상태로 보존하여 호출자에게 복구 작업을 요구하지 않는 공용 API 규칙이다. API가 필요 크기 같은 정보성 output을 실패에서도 제공하려면 해당 예외를 명시해야 한다.
_Avoid_: Partial output, undocumented failure mutation
