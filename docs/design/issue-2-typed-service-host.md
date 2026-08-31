# GitHub Issue #2: Typed Service Host와 middleware pipeline 구현

- Issue: [jammer-droid/PrivateServerToolKit#2](https://github.com/jammer-droid/PrivateServerToolKit/issues/2)
- Issue state: Open
- Last verified: 2026-08-31 (GitHub Issue 조회)
- Local scope agreement: 2026-08-31
- Tracker sync: Pending — 아래 로컬 합의는 GitHub Issue 본문에 아직 반영하지 않았다.
- Design readiness: 목적지·범위 합의 완료; 세부 계약과 구현 slice는 미확정

## 문서 역할

GitHub Issue #2는 목표, 범위, 상태와 상위 완료 조건의 tracker이며, 이 문서는 세부 설계와 로컬에서 합의한 범위 조정을 기록한다. Issue #1의 packet compiler가 생성하는 DTO와 codec을 사용하되 compiler 자체의 책임은 가져오지 않는다.

현재 GitHub 본문은 초기 설계의 범용 async unary lifecycle, `callId`와 pending registry, timeout, 역순 middleware completion, C# client stub 및 PrivateServer production 경로를 필수로 요구한다. 이번 합의는 아래의 더 작은 범위로 조정한 것이므로 **tracker와 로컬 문서가 아직 일치하지 않는다.** 이 문서 갱신은 Issue 본문·완료 조건을 변경하거나 구현을 승인한 것이 아니다. 별도의 tracker 반영 요청 전에는 해당 차이를 유지하고 드러낸다.

## 목표와 가치

Service Host의 주목적은 **NetworkRuntime에서 받은 패킷의 공통 처리와 서비스 로직을 분리하고, 서비스 선언·등록의 반복 코드를 줄이는 것**이다. 개발자는 타입이 있는 요청을 받는 handler와 게임 규칙을 작성하며, Host는 packet lookup, generated Decode/Encode, 입력 middleware와 안전한 실행 전달을 담당한다.

이번 포함 범위는 다음과 같다.

- 특정 PrivateServer 구현에 종속되지 않는 Service Host core
- 패킷의 기초 검증과 입력 middleware, typed handler 연결
- 명시적 서비스·executor 등록 API와 연결 선언을 줄이는 매크로
- 분리된 Host Input Adapter와 Host Output Adapter
- 요청 데이터와 handler 호출 작업을 consumer 실행 영역에 전달하는 executor 계약
- 첫 전달 경로에 실제로 필요한 최소 공용 실행 도구

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
2. 입력 middleware는 등록 순서에 따라 공통 정책을 검사하고, 통과시키거나 handler 전달 전에 거절할 수 있다. 인증 상태 확인이나 요청 빈도 제한은 사용 예이지 해당 정책 제품 전체를 구현한다는 뜻은 아니다.
3. Handwritten service handler는 session/entity ownership, target tick과 game state 같은 domain semantic validation을 소유한다.

**후처리 middleware는 이번 범위에서 제외한다.** World 완료 후 역순으로 middleware를 실행하거나, 완료 결과·응답을 공통 후처리 체인에 통과시키지 않는다. 미래 사용을 위한 빈 completion hook도 선행 추가하지 않는다.

메모리·작업 수명 정리, executor 거절과 종료, handler 오류 처리 및 안전한 송신은 여전히 필요하다. 이들은 작업 소유권과 runtime의 기본 계약이며, 후처리 middleware를 제외한다고 생략하지 않는다.

### 서비스 등록 편의

- 서비스 함수는 일반 C++ handler로 작성하고, 진입점에서 서비스 객체와 executor를 명시적으로 연결한다.
- 등록 정보는 패킷 타입, 서비스 객체와 handler, 실행 대상 및 적용할 입력 정책을 연결한다. 실제 요청 데이터는 등록 시가 아니라 패킷 수신 시 들어온다.
- Handler 자체가 서비스 로직을 실행하는 callback이다. 별도의 World callback 계층을 반드시 하나 더 두지 않는다.
- 매크로는 기본 등록 API를 바탕으로 패킷 타입과 handler의 연결 선언을 줄인다. 매크로의 최종 이름·문법과 구현 방식은 미정이다.
- Packet Tool이 생성한 `PacketId`, `PayloadBytes`, `Encode/Decode`를 재사용한다. 서비스 작성자가 동일한 번호·크기·codec 연결을 중복 기입하는 방향은 피한다.
- 서비스 객체 생성·수명은 애플리케이션이 명시한다. 애트리뷰트를 프로젝트 전체에서 탐색해 자동 발견·등록하는 기능, 범용 DI container는 후속 검토로 둔다.

이 목록은 등록이 연결할 역할을 설명한다. 실제 등록 구조의 필드, 서비스 객체의 보관 방식과 종료 시 수명 보장 절차는 후속 계약에서 정한다.

### 실행 전달과 소유권

Host는 검증된 요청 데이터와 서비스 객체·handler·호출 문맥을 연결한 **실행 작업**을 executor에 넘긴다. 실행 작업은 논리적 단위이며, 구체적인 타입·저장 방식은 아직 미정이다. Executor에 함수 주소만 보내거나, 등록한 순간 handler를 실행하는 것이 아니다. 실제 실행 시점과 순서, World의 tick·double buffer·ECS 규칙은 consumer가 소유한다.

- 경계 간 전달은 move 기반 소유권 이전을 기본 방향으로 삼는다. 서비스 객체 자체를 매번 이동하는 것은 아니며, callback이 참조하는 객체와 문맥은 작업이 끝날 때까지 유효해야 한다.
- `std::move` 또는 byte view만으로 수명이 연장되거나 zero-copy가 보장되지는 않는다. 비동기 전달에서는 owner를 함께 넘기거나 유효한 동안 consumer 소유 데이터로 변환해야 한다.
- Host의 공유 가변 상태는 하나의 논리적 소유 영역에서 직렬로 변경하는 방향을 유지한다. 이것은 서버 전체 요청을 하나의 OS thread나 단일 전역 큐에 묶는 결정이 아니며, 소유 단위와 스케줄링은 미정이다.
- 작업 접수와 실제 handler 실행 완료는 다르다. 작업을 넘긴 뒤 World 완료를 기다리며 Host의 요청 처리를 막는 구조를 전제로 하지 않는다.
- 최소 실행 전달 계약은 #2에서 다루되 World 전용 scheduler 전체를 선행 구현하지 않는다. Executor는 반드시 자체 thread pool을 가지는 것도 아니다.
- 작업 수락·거절 때의 소유권, 큐 포화, 연결 종료, shutdown 및 서비스 객체 파괴 순서는 구현 전 확정해야 한다.

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

## 제외하거나 후속으로 둔 범위

- 범용 async unary RPC 자동 호출 관리와 client stub
- World 완료 후 역순 middleware, 공통 응답 변환 및 선행 completion hook
- 애트리뷰트 기반 프로젝트 전체 서비스 자동 탐색·등록
- NetworkRuntime 이관·전체 구현과 WorldRuntime의 tick·ECS·게임 처리 파이프라인 구현
- socket/IOCP/epoll과 transport framing 재구현
- World, session 또는 entity owner 대체
- gameplay semantic validation 자동 생성
- 범용 dependency injection/container와 전체 application framework
- HTTP/2 또는 gRPC wire compatibility
- streaming RPC
- 자동 retry, load balancing와 service discovery
- 인증/인가 제품 자체, distributed tracing backend와 persistence

Toolkit이 향후 NetworkRuntime과 WorldRuntime도 제공한다는 목표는 유지한다. 이는 [런타임 확장 브레인스토밍](brainstorming/toolkit-server-runtime-direction.md)의 후속 범위이며, #2에서 두 runtime 전체를 구현한다는 뜻은 아니다.

## 구현 전 미해결 결정

아래는 다음 Wayfinder 단계에서 의존 관계를 탐색할 결정 항목이며 구현 순서나 확정된 slice plan이 아니다.

- Host 입력·출력의 public API/ABI, 연결 식별과 연결 이벤트의 표현
- Move 기반 데이터·실행 작업·서비스 객체 수명 및 adapter 간 소유권 계약
- Host 직렬 소유 단위, executor의 작업 수락·거절, 포화와 종료 처리
- Handler와 입력 middleware의 signature·실행 문맥·거절 및 오류 전달 규칙
- 명시적 등록 API와 매크로 문법, 타입 검증·중복 등록 처리 및 필요한 metadata
- C++17 template/type traits와 move-only 지원 도구의 실제 필요 범위
- 응답 제출·Encode·송신 실패 처리 및 기본 진단 방식
- 첫 경로에 필요한 공용 실행 도구, module·의존성·배포 경계
- 첫 검증용 서비스/패킷과 실제 consumer 통합 범위

`TkResult`, byte view, diagnostic와 generated-code consumer 경계는 기존 [ADR](../adr/)을 따른다. 이 문서는 별도의 module result나 특정 runtime ABI를 새로 확정하지 않는다.

## Readiness와 검증 방향

목적지·범위 정의용 grilling은 완료했다. 다음은 미해결 결정과 의존 관계를 탐색하는 단계이며, 세부 API·threading/lifetime contract와 complete dependency-ordered stable-ID implementation slice는 아직 없다. 이 문서의 범위 합의를 구현 준비 완료나 구현 결과로 읽지 않는다.

후속 검증은 합의된 계약에 맞춰 다음 위험을 관찰할 수 있어야 한다.

- 잘못된 패킷이 core에서 거절되고 handler에 도달하지 않는지
- 입력 middleware 순서와 거절, 등록된 타입·handler 연결이 올바른지
- 제어 가능한 executor로 전달과 실제 실행을 분리해 확인할 수 있는지
- 입력·출력 adapter를 메모리 기반 구현으로 바꿔 특정 PrivateServer 없이 검증할 수 있는지
- 전달 거절·큐 포화·연결 종료·shutdown에서 데이터와 서비스 객체 수명이 안전한지
- 필요한 응답이 Encode되어 올바른 출력 대상에 전달되고 송신 실패가 드러나는지
- 서비스 코드가 raw payload 처리나 특정 NetworkRuntime API를 다시 구현하지 않는지

구체적인 첫 서비스, 테스트 형태와 실제 runtime 통합은 아직 선택하지 않았다. 초기 문서의 timeout·late/duplicate completion·client stub 검증을 현재 #2의 필수 gate로 유지하지 않는다.
