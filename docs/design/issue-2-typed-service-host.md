# GitHub Issue #2: Typed Service Host와 middleware pipeline 구현

- Issue: [jammer-droid/PrivateServerToolKit#2](https://github.com/jammer-droid/PrivateServerToolKit/issues/2)
- Issue state: Open
- Last verified: 2026-09-01 (GitHub Issue 본문 갱신 후 재조회)
- Local scope agreement: 2026-09-01 (등록 사용성 합의 포함)
- Tracker sync: Synced — 2026-09-01 합의된 축소 범위·상위 완료 조건·남은 상세 설계를 GitHub Issue 본문에 반영했다.
- Design readiness: 목적지·범위와 등록 사용성 합의 완료; 구체 API와 구현 slice는 미확정

## 문서 역할

GitHub Issue #2는 목표, 범위, 상태와 상위 완료 조건의 tracker이며, 이 문서는 세부 설계와 로컬에서 합의한 범위 조정을 기록한다. Issue #1의 packet compiler가 생성하는 DTO와 codec을 사용하되 compiler 자체의 책임은 가져오지 않는다.

초기 GitHub 본문은 범용 async unary lifecycle, `callId`와 pending registry, timeout, 역순 middleware completion, C# client stub 및 PrivateServer production 경로를 필수로 요구했다. 2026-09-01 사용자의 tracker 반영 요청에 따라 아래의 더 작은 범위와 상위 완료 조건으로 동기화했다. 범위·등록 사용성 합의와 tracker 동기화는 구현 준비 완료나 구현 결과를 의미하지 않는다.

## 목표와 가치

Service Host의 주목적은 **NetworkRuntime에서 받은 패킷의 공통 처리와 서비스 로직을 분리하고, 서비스 선언·등록의 반복 코드를 줄이는 것**이다. 개발자는 타입이 있는 요청을 받는 handler와 게임 규칙을 작성하며, Host는 packet lookup, generated Decode/Encode, 입력 middleware와 안전한 실행 전달을 담당한다.

현재 설계의 초점은 패킷 검증과 서비스 핸들러 선언·등록 자동화다. Adapter와 executor는 이 연결을 위한 최소 계약으로 다루며, World 내부의 큐 소비·스케줄링·종료 정책을 함께 설계하거나 해당 정책의 확정을 등록 설계의 선행 조건으로 삼지 않는다.

이번 포함 범위는 다음과 같다.

- 특정 PrivateServer 구현에 종속되지 않는 Service Host core
- 패킷의 기초 검증과 입력 middleware, typed handler 연결
- 명시적 서비스·executor 등록 API와 연결 선언을 줄이는 매크로
- 분리된 Host Input Adapter와 Host Output Adapter
- 요청 데이터와 handler 호출 작업을 consumer 실행 영역에 전달하는 executor 계약
- 등록·연결 경로에 실제로 필요한 최소 공용 실행 도구

관계도는 데이터 흐름이며, 구체적인 class·DLL 배치나 스레드 구성을 확정하지 않는다.

```text
                   +----------------------+
                   | NetworkRuntime       |
                   +----------------------+
                      |                ^
             received packets      send request
                      v                |
           HostInputAdapter    HostOutputAdapter
                      |                ^
                      v                |
                   +----------------------+
                   | Service Host         |
                   | Core / Input policy  |
                   | Binding / Encode     |
                   +----------------------+
                      |                ^
                 execution job     reply request
                      v                |
                World executor         |
                      |                |
                      v                |
                   +----------------------+
                   | WorldRuntime         |
                   | Service logic        |
                   +----------------------+
```

WorldRuntime은 향후 타깃이며, 실제 WorldRuntime 구현을 먼저 완성해야 Host를 설계하거나 검증할 수 있는 것은 아니다. 아래 역할명과 `World executor`는 구체적인 public API 이름이 아닌 설계 용어다.

## 확정된 설계 방향

### 독립적인 입력·출력 adapter

Host core와 public 계약에는 `NrGateway`, `NrSessionSendChannel`, PrivateServer의 event 타입 또는 특정 World 구현을 넣지 않는다. Toolkit이 필요한 계약을 먼저 정하고 개별 runtime이 adapter로 연결한다. 기존 PrivateServer 코드는 연결 가능성의 참고이지 Host 설계의 필수 전제가 아니다.

| 역할 | 방향 | 책임 |
| --- | --- | --- |
| Host Input Adapter (`HostInputAdapter`) | NetworkRuntime → Host | Framing이 끝난 패킷과 필요한 연결 이벤트를 Host의 독립적인 입력 계약으로 전달한다. |
| Host Output Adapter (`HostOutputAdapter`) | Host → NetworkRuntime | Host가 Encode한 데이터를 대상 연결의 송신 기능으로 연결하고 수락·거절을 전달한다. |
| Service Executor | Host → consumer 실행 영역 | 요청 데이터와 handler 호출 작업을 World 큐·worker 등 지정된 실행 영역에 맡긴다. |

입력 계약에는 연결·패킷 식별과 데이터의 유효 수명, 출력 계약에는 송신 대상과 데이터 소유권·수락 결과가 필요하다. 구체적인 타입과 API/ABI는 아직 미정이다. Adapter가 특정 runtime의 표현을 변환하더라도 generated Decode와 패킷별 형식 검증은 Host의 책임으로 유지한다.

입력·출력 adapter는 같은 transport를 사용할 수 있다. 둘을 나누는 것이 별도 스레드·큐·프로세스를 각각 만든다는 뜻은 아니다. 수신을 callback으로 받는지 queue에서 꺼내는지, 송신에 어떤 내부 worker를 사용하는지는 구체 구현 뒤에 둔다.

### 검증과 입력 middleware

검증 책임은 세 계층으로 구분한다.

1. Service Host core는 등록된 packet의 존재와 generated codec의 exact payload size, payload version, Decode 같은 protocol invariant를 항상 검증한다. 생성된 codec은 신뢰할 수 있어도 외부에서 받은 bytes가 그 계약을 지킨다고 가정하지 않는다.
2. 입력 middleware는 진입점에서 서비스를 등록할 때 목록으로 지정한다. 해당 서비스의 모든 binding에 같은 목록을 적용하며, 각 요청은 목록 순서대로 검사를 통과하거나 handler 전달 전에 거절된다. 인증 상태 확인이나 요청 빈도 제한은 사용 예이지 해당 정책 제품 전체를 구현한다는 뜻은 아니다.
3. Handwritten service handler는 session/entity ownership, target tick과 game state 같은 domain semantic validation을 소유한다.

서비스 단위 공통 적용은 middleware 목록의 공통 적용이다. 요청 하나가 모든 handler를 실행하는 것이 아니라, 해당 패킷에 연결된 handler 하나만 선택된다. 첫 버전에서 handler별 middleware 추가·제외·덮어쓰기는 지원하지 않으며, middleware 목록이 없어도 Core의 기본 검증은 유지한다.

**후처리 middleware는 이번 범위에서 제외한다.** World 완료 후 역순으로 middleware를 실행하거나, 완료 결과·응답을 공통 후처리 체인에 통과시키지 않는다. 미래 사용을 위한 빈 completion hook도 선행 추가하지 않는다.

Host가 소유한 데이터의 정리, 전달 실패와 안전한 송신에 필요한 기본 수명 조건은 여전히 필요하다. 구체 API를 만들 때 이를 명시하되, World가 수락한 작업을 실행·폐기하는 정책이나 전체 서버의 종료 전략을 Host의 책임으로 가져오지 않는다.

### 서비스 등록 편의

- 서비스 함수는 일반 C++ handler로 작성하고, **서비스 정의 옆에 패킷 타입과 특정 handler의 연결 선언을 모은다.** 진입점에서는 handler를 하나씩 등록하지 않고 서비스 단위로 등록한다.
- 연결 선언에는 패킷 타입과 handler를 둘 다 명시한다. Handler 인자에서 패킷 타입을 자동 추론하는 기능은 첫 범위에서 제외한다. 함수 이름은 패킷 이름과 같을 필요가 없으며 선언된 연결과 타입 호환성으로 판단한다.
- 진입점의 등록은 서비스 객체, executor와 입력 middleware 목록을 연결한다. 실제 요청 데이터는 등록 시가 아니라 패킷 수신 시 들어온다.
- Handler 자체가 서비스 로직을 실행하는 callback이다. 별도의 World callback 계층을 반드시 하나 더 두지 않는다.
- 매크로는 기본 등록 API를 바탕으로 패킷 타입과 handler의 연결 선언을 줄인다. 매크로의 최종 이름·문법과 구현 방식은 미정이다.
- Packet Tool이 생성한 `PacketId`, `PayloadBytes`, `Encode/Decode`를 재사용한다. 서비스 작성자가 동일한 번호·크기·codec 연결을 중복 기입하는 방향은 피한다.
- 명시된 요청 타입을 handler가 받을 수 있는지는 컴파일 시 검증한다. C++17 template/type traits는 이 타입 검증에 활용하며, 지원할 handler signature의 정확한 형태는 구체 API에서 정한다.
- 서비스 객체 생성·수명은 애플리케이션이 명시한다. 애트리뷰트를 프로젝트 전체에서 탐색해 자동 발견·등록하는 기능, 범용 DI container는 후속 검토로 둔다.

등록 사용성의 개념 예시는 다음과 같다. 이는 실제 API나 매크로 문법이 아니다.

```text
[TimeService 정의 옆의 연결 선언]
WorldTimeSyncRequest -> TimeService::OnTimeSync

[서버 진입점의 서비스 단위 등록]
TimeService 인스턴스 + World executor + 입력 middleware 목록
```

**같은 Host 안에서는 하나의 `PacketId`를 하나의 handler에만 연결한다.** 다른 서비스나 다른 패킷 타입 이름으로 선언했더라도 generated `PacketId`가 중복되면 해당 서비스 등록을 `TkResult` 실패로 처리한다. 기존 연결을 덮어쓰거나 여러 handler를 동시에 연결하지 않는다. 실패한 서비스 등록 시도의 binding은 부분 반영하지 않으며, 이전까지 성공한 등록 상태를 보존한다. 서로 다른 Host 인스턴스에서 같은 패킷을 사용하는 것은 허용한다.

여기까지가 등록 사용성에 대한 합의다. 실제 등록 구조의 필드, 서비스 객체의 보관 방식과 public API/ABI는 아직 확정하지 않았다.

### 실행 전달과 소유권

Host는 검증된 요청 데이터와 서비스 객체·handler·호출 문맥을 연결한 **실행 작업**을 executor에 넘긴다. 실행 작업은 논리적 단위이며, 구체적인 타입·저장 방식은 아직 미정이다. Executor에 함수 주소만 보내거나, 등록한 순간 handler를 실행하는 것이 아니다. 실제 실행 시점과 순서, World의 tick·double buffer·ECS 규칙은 consumer가 소유한다.

- 요청 데이터는 Host에서 Decode·검증한 뒤 **독립된 값으로 복사해 실행 작업이 소유**하는 방향으로 합의했다. 그 작업 자체는 move로 전달할 수 있다. 서비스 객체 전체를 복사하는 것이 아니며, 포인터·view만 복사해 원본 수신 버퍼를 계속 참조하는 방식은 독립 사본에 해당하지 않는다.
- 원본 수신 버퍼를 World 실행 완료까지 유지하지 않는다. Host에서 읽기가 끝나면 adapter/runtime의 수명 계약에 따라 반환할 수 있다. `std::move`나 view 자체가 수명을 연장하거나 zero-copy를 보장하는 것은 아니다.
- Executor가 작업을 수락하면 World 측이 작업과 요청 사본의 실행·보관·정리를 책임진다. 접수를 거절하면 소유권은 Host에 남고, Host는 자동 재시도 없이 실패를 드러낸 뒤 정리한다. 이 합의가 클라이언트에 자동 오류 패킷을 보내는 정책까지 정한 것은 아니다.
- 제출 호출 안에서 handler를 직접 실행하지 않고 World 측이 작업을 소비해 호출한다. 별도 전용 스레드를 강제하지 않으며, 다른 worker가 빠르게 소비해 제출 함수 반환 전에 실행되는 것까지 금지하는 계약은 아니다.
- Host의 공유 가변 상태를 논리적 소유 영역에서 직렬로 변경한다는 방향은 유지한다. 소유 단위와 스케줄러 구성은 구현 구체화 시 다룰 항목이며, 현재 등록 사용성 논의에서 정하지 않는다.
- 작업 접수와 실제 handler 실행 완료는 다르다. 작업을 넘긴 뒤 World 완료를 기다리며 Host의 요청 처리를 막는 구조를 전제로 하지 않는다.
- 최소 실행 전달 계약은 #2에서 다루되 World 전용 scheduler 전체를 선행 구현하지 않는다. Executor는 반드시 자체 thread pool을 가지는 것도 아니다.

이 전달 경계의 방향을 World 내부 동작의 상세 설계로 확대하지 않는다. **World가 수락한 작업을 모두 실행한 뒤 종료할지, 폐기하고 종료할지는 World/executor와 애플리케이션의 정책이며 아직 확정하지 않았다.** 앞서 논의한 drain 후 종료 제안은 Host의 필수 계약이나 검증 gate로 채택하지 않는다.

다만 작업이 참조하는 서비스 객체나 Host의 응답 기능을 사용 중에 파괴해서는 안 된다는 기본 수명 조건은 필요하다. 애플리케이션이 각 계층의 종료와 객체 수명을 조율하며, 이를 위해 Host가 World 작업의 완료를 추적하거나 drain 정책을 강제해야 하는 것은 아니다. Host 자신의 접수·진행 중 호출·참조 안전성에 필요한 최소 계약은 구체 API를 정할 때 명시한다.

### 응답 지원과 RPC의 구분

전용 요청·응답 패킷과 handler로 TimeSync 같은 request/response 서비스를 작성하는 것은 가능해야 한다. 서비스가 응답 데이터를 만들면 Host가 Encode하고 Host Output Adapter로 송신을 요청한다. 구체적인 응답 제출 API는 아직 미정이며, 송신 접수 성공이 실제 송신 또는 상대의 수신 완료를 뜻하지 않는다.

이 지원을 범용 RPC 자동 관리와 구분한다. `callId` 자동 부여·응답 매칭, pending-call registry, timeout·취소·late/duplicate completion 관리, client stub은 이번 필수 범위로 삼지 않는다. 필요한 요청 번호나 응답 대응 규칙을 개별 서비스/호출 측에서 정하는 방식은 허용한다. One-way 입력·알림도 요청마다 응답을 요구하는 형태로 강제하지 않는다.

### 별도 공용 실행 계층과 레퍼런스

- 기존 header-only Common은 기초 타입·byte view·result·diagnostic 계약으로 유지한다. 실행 도구를 이 계층에 섞거나 generated codec consumer에 runtime binary 의존성을 강제하지 않는다.
- NetworkRuntime·Service Host·향후 WorldRuntime이 공유할 실행 도구는 별도의 **공용 실행 계층(가칭)** 으로 구분한다. 실제 module 이름과 배포 단위는 미정이다.
- 첫 Host 경로에 필요한 도구만 먼저 제공한다. Job queue, 직렬 실행, MPSC, thread pool, spin lock, move-only 지원 타입과 메모리 풀을 모두 선행 구현하는 범위는 아니다.
- Akka의 mailbox/job queue와 실행 예약 구조를 레퍼런스로 삼는다. 예약 상태를 mailbox 내부에 캡슐화하고 drain 후 예약 해제·재확인을 연결하는 방향을 참고하되, 큐의 게시·empty 관찰·예약 실패·종료 계약까지 함께 검증해야 한다. 구체 자료구조·메모리 순서·풀링 채택은 미확정이다.
- Nakama의 match/tick 구조는 향후 WorldRuntime이 서비스 입력을 소비하는 방식을 설계할 때 참고한다. 이번 Host에 match/room 정책을 넣는 근거로 삼지 않는다.

## 책임 경계

### Service Host가 소유

- 공통 패킷 검증·Decode와 typed handler binding
- 입력 middleware의 순서와 거절 처리
- 독립적인 연결 식별·데이터 수명·실행 전달 계약
- 서비스 등록 편의와 응답 Encode·Host Output Adapter 호출
- 전달 실패와 handler 오류가 묵살되지 않도록 하는 기본 오류 처리

### NetworkRuntime이 소유

- socket, IOCP/epoll과 OS I/O
- TCP framing
- recv/send buffer lifetime
- physical connection과 transport shutdown

### Consumer service와 World가 소유

- session/entity/World ownership
- tick admission과 command ordering
- gameplay semantic validation
- World state mutation과 business/gameplay logic
- service별 executor와 middleware policy 선택
- 수락한 작업과 요청 사본의 실행·보관·정리 및 World 내부 종료 정책

애플리케이션 진입점은 서비스와 각 계층을 조립하고 객체 수명을 조율한다. World 작업의 drain 또는 폐기 정책을 Service Host가 대신 선택하지 않는다.

## 제외하거나 후속으로 둔 범위

- 범용 async unary RPC 자동 호출 관리와 client stub
- World 완료 후 역순 middleware, 공통 응답 변환 및 선행 completion hook
- 애트리뷰트 기반 프로젝트 전체 서비스 자동 탐색·등록
- Handler 인자에서의 패킷 타입 자동 추론, handler별 middleware 추가·제외·덮어쓰기
- 하나의 Host에서 같은 패킷을 여러 handler에 배포하는 구독 모델
- NetworkRuntime 이관·전체 구현과 WorldRuntime의 tick·ECS·게임 처리 파이프라인 구현
- World의 상세 큐 소비·스케줄링·drain/폐기·전체 서버 종료 정책
- socket/IOCP/epoll과 transport framing 재구현
- World, session 또는 entity owner 대체
- gameplay semantic validation 자동 생성
- 범용 dependency injection/container와 전체 application framework
- HTTP/2 또는 gRPC wire compatibility
- streaming RPC
- 자동 retry, load balancing와 service discovery
- 인증/인가 제품 자체, distributed tracing backend와 persistence

Toolkit이 향후 NetworkRuntime과 WorldRuntime도 제공한다는 목표는 유지한다. 이는 [런타임 확장 브레인스토밍](brainstorming/toolkit-server-runtime-direction.md)의 후속 범위이며, #2에서 두 runtime 전체를 구현한다는 뜻은 아니다.

## 다음 구체화와 보류 항목

등록 사용성의 주요 선택은 완료했다. Wayfinder 경로 판정에서는 현재 축소 범위에 별도 map을 만들지 않고 상세 설계 grilling으로 전환하기로 했다. 기존 map을 완료한 것이 아니며, 반드시 선행할 별도 조사나 prototype은 현재 식별되지 않았다. 추가 논의를 World 실행 정책으로 넓히지 않고, 다음 순서로 구현 준비를 구체화한다. 이는 아직 dependency-ordered implementation slice plan이 아니다.

1. **Host 제공 경계:** consumer가 컴파일하는 C++ 등록 도우미와 런타임 구현의 제공·public API/ABI 경계를 정한다. 기존 규약이 Host를 반드시 DLL로 제공하도록 강제하는 것은 아니며, 특정 facade/core 분리나 배포 단위를 확정한 상태도 아니다.
2. **TimeSync 기준의 구체 API 사용 예:** 서비스 함수와 연결 매크로, 진입점 등록, 입력 middleware signature 및 최소 input/output/executor 연결을 한 예시로 표현한다. 필요한 요청 문맥·응답 제출·오류 전달과 호출 가능한 범위의 수명 조건을 정하되, World 내부의 실행·종료 방식은 정하지 않는다. 매크로 이름과 지원 signature도 이 단계에서 구체화한다.
3. **검증 기준과 구현 slice:** 아래 검증 방향을 관찰 가능한 기준으로 다듬고 stable ID, 의존 관계, 첫 slice와 검증 방법을 갖춘 계획을 같은 문서에 작성한다. 공용 도구는 이 경로에 실제 필요한 만큼만 정한다. 이는 상세 설계의 마무리이며, readiness gate 충족 후 구현 요청에 따라 slice별 구현·검증으로 전환한다.

WorldRuntime 전체 구현, 구체 scheduler·pooling·종료 정책과 실제 runtime 통합 범위는 후속이다. 이들의 미확정을 현재 등록 사용성의 blocker로 취급하지 않는다. 다만 구현할 Host API 자체의 데이터·callback 수명과 실패 처리를 생략해도 된다는 의미는 아니다.

`TkResult`, byte view, diagnostic와 generated-code consumer 경계는 기존 [ADR](../adr/)을 따른다. 이 문서는 별도의 module result나 특정 runtime ABI를 새로 확정하지 않는다.

## Readiness와 검증 방향

목적지·범위 정의와 등록 사용성의 grilling은 완료했다. 구체 API와 해당 API를 안전하게 사용할 최소 계약, complete dependency-ordered stable-ID implementation slice는 아직 없다. 이 문서의 합의를 #2 전체의 구현 준비 완료나 구현 결과로 읽지 않는다.

첫 설계·검증 기준은 **TimeSync 요청·응답**이다. 기존 Packet Tool 테스트의 [WorldTimeSyncRequest](../../tools/packet/cli/tests/schemas/WorldTimeSyncRequest.json)와 [WorldTimeSyncResponse](../../tools/packet/cli/tests/schemas/server/WorldTimeSyncResponse.json) schema를 기준으로 삼는다. 메모리 기반 입력·출력 adapter와 실행 시점을 제어하는 테스트 executor로 아래 흐름을 확인하는 방향이며, 실제 NetworkRuntime·WorldRuntime을 먼저 구현할 필요는 없다.

```text
테스트 입력 -> Host Decode -> 서비스 공통 입력 middleware
-> executor에 작업 보관 -> 테스트에서 작업 소비 -> OnTimeSync
-> 응답 생성 -> Host Encode -> 테스트 출력
```

후속 검증은 합의된 계약에 맞춰 다음 위험을 관찰할 수 있어야 한다.

- 잘못된 패킷이 core에서 거절되고 handler에 도달하지 않는지
- 패킷·handler 타입 불일치가 컴파일 시 검출되는지
- 같은 Host의 중복 `PacketId` 등록이 실패하고 해당 서비스의 부분 등록 없이 이전 상태가 보존되는지
- 서비스 단위 middleware의 순서·거절이 각 binding에 적용되고, 패킷마다 연결된 handler 하나만 선택되는지
- 제어 가능한 executor로 전달과 실제 실행을 분리해 확인할 수 있는지
- 입력·출력 adapter를 메모리 기반 구현으로 바꿔 특정 PrivateServer 없이 검증할 수 있는지
- 테스트 adapter 경계에서 요청 사본의 독립성과 전달 거절 시 Host의 정리·자동 재시도 없음이 확인되는지
- 구체화한 Host API의 호출·응답 경로에서 데이터와 참조 대상의 수명이 안전한지
- 필요한 응답이 Encode되어 올바른 출력 대상에 전달되고 송신 실패가 드러나는지
- 서비스 코드가 raw payload 처리나 특정 NetworkRuntime API를 다시 구현하지 않는지

테스트 코드의 형태와 실제 runtime 통합은 아직 선택하지 않았다. 초기 문서의 timeout·late/duplicate completion·client stub 검증이나 World의 drain 후 종료를 현재 #2의 필수 gate로 유지하지 않는다.
