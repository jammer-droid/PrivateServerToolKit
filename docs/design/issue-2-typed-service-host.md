# GitHub Issue #2: Typed Service Host와 middleware pipeline 구현

- Issue: [jammer-droid/PrivateServerToolKit#2](https://github.com/jammer-droid/PrivateServerToolKit/issues/2)
- Issue state: Open
- Last verified: 2026-08-28
- Design readiness: Refinement required before implementation

## 문서 역할

GitHub Issue #2는 Typed Service Host의 목표, 범위, 상태와 상위 완료 조건을 소유한다. 이 문서는 Issue #1의 기존 `Phase 4 — Async unary RPC 계층`에서 분리·확장한 설계 방향, 책임 경계와 구현 전 미해결 결정을 관리한다.

Issue #1은 C++/C# fixed-layout packet compiler, codec와 consumer build integration을 소유한다. Issue #2는 그 생성물을 사용해 NetworkRuntime과 application service 사이의 application-facing I/O, middleware, typed dispatch와 async unary lifecycle을 소유한다.

## 목표와 가치

Service 개발자가 공통 service 계약과 handwritten handler에 집중하고 packet switch, Decode/Encode, executor handoff, request correlation, timeout/disconnect와 공통 검증을 service마다 반복 구현하지 않게 한다.

```text
NetworkRuntime
  -> transport adapter
  -> Service Host
       -> mandatory protocol validation
       -> middleware pipeline
       -> generated dispatcher
       -> executor adapter
  -> handwritten service handler
  -> domain / World logic
```

단순 RPC stub generator가 아니라 비동기 request가 수신된 뒤 response 또는 cancellation으로 종료될 때까지의 lifecycle을 작은 interface 뒤에 숨기는 것을 핵심 가치로 삼는다.

## 확정된 설계 방향

### Service Host

- `ServiceHost`는 ingress pipeline과 typed service invocation lifecycle의 상위 개념이다.
- One-way command, one-way event와 async unary call을 서로 다른 invocation kind로 지원한다.
- Unary call은 전체 module 이름이 아니라 Service Host의 하위 capability다.
- Handler는 raw payload, packet switch, `callId`와 transport buffer lifetime을 알지 않는다.
- Core 이름과 계약에는 `PrivateServer`, `WorldServer` 또는 특정 transport type을 넣지 않는다.

예상 vocabulary는 다음과 같으며 실제 public API 이름은 refinement에서 확정한다.

```text
ServiceHost
ServicePipeline
ServiceContext
ServiceMiddleware
IServiceTransport
IServiceExecutor
CommandDispatcher
UnaryCallRuntime
```

### Invocation kind

- Command: caller가 service handler로 보내는 one-way 입력
- Event: publisher가 subscriber로 보내는 one-way 알림 또는 replication message
- Unary call: 한 request에 한 response가 연결되는 async 호출

공통 service/wire 계약은 호출 시작 방향이나 언어에 종속되지 않는다. 첫 production generation은 C# client와 C++ service host 방향으로 제한할 수 있지만, 역방향 generator는 실제 요구가 생길 때 추가한다.

### 검증과 middleware

검증 책임은 세 계층으로 구분한다.

1. Service Host core는 packet/method 존재, envelope, exact payload size, payload version과 Decode 같은 protocol invariant를 항상 검증한다.
2. Ordered middleware는 session 상태, 인증, rate limit, deadline, diagnostic와 metric 같은 공통 정책을 처리하고 handler 전에 short-circuit할 수 있다.
3. Handwritten service handler는 session/entity ownership, target tick과 game state 같은 domain semantic validation을 소유한다.

Middleware는 단순 pre-hook이 아니라 async completion까지 관찰할 수 있어야 한다. 등록 순서, `next` 단일 호출, short-circuit, 역순 completion, timeout/disconnect 뒤 late completion 무시와 borrowed data 비보관 조건을 계약으로 고정한다.

### Port와 adapter

- `IServiceTransport`는 Service Host를 기존 NetworkRuntime 또는 다른 transport에 연결한다.
- `IServiceExecutor`는 handler를 session actor, World ingress queue 또는 worker처럼 consumer가 선택한 실행 영역에 전달한다.
- PrivateServer adapter는 NetworkRuntime과 World execution seam을 연결한다.
- In-memory transport, deterministic executor와 controllable clock은 같은 port를 사용하는 test adapter다.

Production과 test adapter라는 실제 variation을 기준으로 seam을 정당화하고, test만을 위해 internal type을 public surface로 노출하지 않는다.

## 책임 경계

### Service Host가 소유

- application-facing message validation과 typed dispatch
- middleware ordering, short-circuit와 completion observation
- safe connection identity를 포함한 request context
- unary `callId`, pending registry, deadline, disconnect와 at-most-once completion
- response Encode와 transport adapter 호출
- 공통 diagnostic/metric hook

### NetworkRuntime이 계속 소유

- socket, IOCP/epoll과 OS I/O
- TCP framing
- recv/send buffer lifetime
- physical connection과 transport shutdown

### Consumer service가 계속 소유

- session/entity/World ownership
- tick admission과 command ordering
- gameplay semantic validation
- World state mutation과 business/gameplay logic
- service별 executor와 middleware policy 선택

## 확장성 원칙

- Service Host core는 World Server뿐 아니라 Login, Lobby, Chat, Matchmaking 또는 다른 게임 서버에서 adapter만 교체해 사용할 수 있어야 한다.
- Middleware는 인증/인가 제품이나 tracing backend 자체를 구현하지 않고 해당 정책을 연결하는 extension point를 제공한다.
- `MovementInput` 같은 one-way input을 unary call로 강제 변환하지 않는다.
- C++ client/C# server까지 네 방향 generator를 선행 구현하지 않는다.
- Streaming, retry, load balancing와 service discovery 요구가 생기면 범용 RPC framework 도입 여부를 다시 평가한다.

## 첫 vertical slice 원칙

단순 echo는 code generation만 증명하므로 첫 완료 근거로 사용하지 않는다. 첫 unary vertical slice는 지연된 executor 또는 repository 작업을 포함하고 다음 lifecycle을 드러내야 한다.

- 정상 request/response
- handler 실행 중 disconnect
- timeout 뒤 late completion
- duplicate completion 시도
- connection identity 재사용 방지
- middleware ordering, short-circuit와 async completion 관찰
- queue full 또는 admission 실패

같은 Service Host에서 기존 `MovementInput` 계열의 one-way command도 처리해 command와 unary가 동일 adapter/pipeline 기반을 공유하되 lifecycle은 구분되는지 확인한다. 구체적인 unary method는 PrivateServer의 실제 use case를 기준으로 refinement에서 선택한다.

## 범위 밖

- socket/IOCP/epoll과 transport framing 재구현
- World, session 또는 entity owner 대체
- gameplay semantic validation 자동 생성
- 범용 dependency injection/container와 전체 application framework
- HTTP/2 또는 gRPC wire compatibility
- streaming RPC
- 자동 retry, load balancing와 service discovery
- 인증/인가 제품 자체, distributed tracing backend와 persistence

## 구현 전 미해결 결정

다음 결정이 순서대로 해결되고 complete dependency-ordered stable-ID slice plan이 작성되기 전에는 구현 준비가 끝난 것으로 보지 않는다.

1. Service schema와 command/event/unary 표현
2. Packet ID, direction과 transport-to-service binding 소유자
3. Wire envelope, `callId` namespace와 양방향 동시 호출 규칙
4. Stable connection identity와 disconnect/quiescence 계약
5. C++17 async completion 및 responder ownership model
6. Middleware interface, ordering, short-circuit와 completion contract
7. `TkResult`, remote status와 domain outcome mapping
8. Executor handoff, queue full과 backpressure policy
9. C# client/C++ service generation surface
10. 첫 production vertical slice와 consumer migration 경계

## Readiness와 검증 방향

현재는 목표, ownership과 extension seam을 확정한 design direction 단계다. 세부 API, threading/lifetime contract와 stable implementation slice는 아직 확정하지 않았다.

구현 준비가 완료되면 각 slice는 다음 evidence 중 실제 위험에 맞는 항목을 가져야 한다.

- Public C++/C# generated contract compile
- Production NetworkRuntime adapter와 in-memory adapter의 동일 port conformance
- Middleware ordering, short-circuit와 async completion state test
- Timeout, disconnect, late/duplicate completion의 deterministic lifecycle test
- One-way command와 unary request/response의 public Service Host smoke path
- PrivateServer consumer에서 network parsing/correlation code가 World service handler로 누출되지 않는 integration evidence

