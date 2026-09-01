# GitHub Issue #2: Typed Service Host와 middleware pipeline 구현

- Issue: [jammer-droid/PrivateServerToolKit#2](https://github.com/jammer-droid/PrivateServerToolKit/issues/2)
- Issue state: Open
- Last verified: 2026-09-01 (GitHub Issue 본문 갱신 후 재조회)
- Local design agreement: 2026-09-01 (`$grilling` 완료 — ABI, execution 도구, job pipeline과 stable slice 포함)
- Tracker sync: Synced — 2026-09-01 GitHub Issue 본문에 template-first facade, design readiness와 `E1`~`H4` 구현 순서를 반영했다.
- Design readiness: Ready for implementation — 로컬 상세 계약과 dependency-ordered `E1`~`H4` slice를 확정했다. 구현 또는 검증 완료를 뜻하지 않는다.

## 문서 역할

GitHub Issue #2는 목표, 범위, 상태와 상위 완료 조건의 tracker이며, 이 문서는 세부 설계와 로컬에서 합의한 범위 조정을 기록한다. Issue #1의 packet compiler가 생성하는 DTO와 codec을 사용하되 compiler 자체의 책임은 가져오지 않는다.

초기 GitHub 본문은 범용 async unary lifecycle, `callId`와 pending registry, timeout, 역순 middleware completion, C# client stub 및 PrivateServer production 경로를 필수로 요구했다. 2026-09-01 사용자의 tracker 반영 요청에 따라 더 작은 범위와 상위 완료 조건으로 한 차례 동기화했다. 이후 이 문서에서 shared-library ABI, 공용 execution 도구, opaque Service Job과 typed facade를 구체화했고 GitHub Issue 본문에도 design readiness와 stable implementation order를 반영했다.

## 목표와 가치

Service Host의 주목적은 **NetworkRuntime에서 받은 패킷의 공통 처리와 서비스 로직을 분리하고, 서비스 선언·등록의 반복 코드를 줄이는 것**이다. 개발자는 타입이 있는 요청을 받는 handler와 게임 규칙을 작성하며, Host는 packet lookup, generated Decode/Encode, 입력 middleware와 안전한 실행 전달을 담당한다.

현재 설계의 초점은 패킷 검증과 서비스 핸들러 선언·등록 자동화다. Adapter와 executor는 이 연결을 위한 최소 계약으로 다루며, World 내부의 큐 소비·스케줄링·종료 정책을 함께 설계하거나 해당 정책의 확정을 등록 설계의 선행 조건으로 삼지 않는다.

이번 포함 범위는 다음과 같다.

- 특정 PrivateServer 구현에 종속되지 않는 Service Host core
- 패킷의 기초 검증과 입력 middleware, typed handler 연결
- 명시적 서비스·executor 등록 API와 template/type-traits 기반 typed facade
- 분리된 Host Input Adapter와 Host Output Adapter
- 요청 데이터와 handler 호출 작업을 consumer 실행 영역에 전달하는 executor 계약
- 등록·연결 경로에 실제로 필요한 최소 공용 실행 도구

관계도는 확정한 job의 변환과 실행 소유 관계다. Service Host public spelling과 공용 execution module의 핵심 queue·scheduling 계약은 아래에서 구체화한다.

```text
NetworkRuntime producers
    | owning Ingress Job
    v
Host Input Adapter
    | hash(Host Connection Key)
    +----> ingress lane 0 --+
    +----> ingress lane 1 --+  lane별 단일 drain owner
    +----> ingress lane N --+  lane 사이는 병렬 실행
                            |
                            v
                    Service Host Core
                    lookup / Decode / middleware
                            |
                            | owning Service Job
                            v
                    service별 World executor
                            |
                            v
                    Service handler / game logic
                            |
                            | response
                            v
                    Host Encode -> Output Adapter
                            |
                            v
                    NetworkRuntime send admission
```

전체 조립은 job queue 기반이지만 Service Host Core 자체가 ingress queue, worker pool이나 World queue를 소유하는 모델은 아니다. WorldRuntime은 향후 타깃이며 실제 WorldRuntime 구현을 먼저 완성해야 Host를 설계하거나 검증할 수 있는 것은 아니다. `Ingress Job`은 adapter 구현 용어이고, `TkServiceJob`, `TkHostConnectionKey`와 executor callback은 Host public 계약에 포함한다.

## 확정된 설계 방향

### 제공 형태와 public binary 경계

- Service Host는 일반 C++17 library로 제공하며 compiled runtime의 기본 배포 형태는 Windows DLL과 Linux/macOS shared library다.
- Module source는 `runtime/service_host/`, CMake target은 `pstk_service_host`, consumer alias는 `PSTK::ServiceHost`를 사용한다. Public C ABI header는 `pstk/service_host/TkServiceHost.h`, C++17 facade는 `pstk/service_host/TkServiceBinding.hpp`에 둔다.
- 모든 ToolKit shared-library public binary seam은 [ADR 0006](../adr/0006-fix-shared-library-public-boundary-to-c-abi.md)에 따라 `extern "C"` 함수, fixed-width scalar, C-compatible POD, function pointer와 opaque handle로 고정한다. STL, exception, RTTI, template, reference와 compiler-dependent C++ class layout을 export하지 않는다.
- Template/type-traits 검증과 move-only RAII 편의는 `pstk::service` namespace의 consumer-compiled C++ facade가 담당한다. Host의 registry, pipeline state와 핵심 정책은 compiled runtime에 둔다. 첫 버전의 필수 사용 경로는 template-first 명시적 등록이며 전역 static initializer, attribute scanning과 project-wide discovery는 제공하지 않는다. Macro는 추가하더라도 이 facade를 감싸는 얇은 문법 편의로 제한하며 첫 완료 조건으로 삼지 않는다.
- C++ facade는 packet 타입과 handler의 호환성을 compile time에 검증하고, generated metadata·borrowed service instance·Decode/파괴·handler 호출 thunk를 C ABI binding descriptor로 만든다. Host DLL은 service class나 member-function pointer의 C++ 표현을 알지 않는다.
- Service Job은 DLL이 발급하고 같은 DLL API로 실행·해제하는 opaque single-owner handle로 경계를 통과한다. C++ facade는 이를 move-only RAII owner로 감싼다.

Service Host는 애플리케이션이 생성한 service instance를 주입받아 borrowed pointer로 사용하며 생성·복제·파괴하지 않는다. 애플리케이션은 해당 service에 연결된 모든 accepted Service Job이 실행되거나 폐기되어 handle이 해제될 때까지 instance와 consumer thunk code의 수명을 보장한다.

서비스 등록은 요청 처리 전 구성 단계에서만 허용하고, 구성이 확정된 뒤 binding registry는 immutable하다. 실행 중 register, unregister, replace와 hot reload는 첫 버전에서 지원하지 않는다.

### Public C ABI 조립 계약

아래 선언은 구현에서 유지할 public spelling과 lifetime을 보여준다. Export macro와 platform decoration은 생략했다.

```cpp
typedef struct TkServiceHost TkServiceHost;
typedef struct TkServiceJob TkServiceJob;

typedef struct TkHostConnectionKey
{
    uint64_t connectionId;
    uint64_t generation;
} TkHostConnectionKey;

typedef struct TkHostOutputInfo
{
    TkHostConnectionKey connectionKey;
    uint16_t packetId;
    TkByteView payload;
} TkHostOutputInfo;

typedef TkResult (*TkHostOutputCallback)(
    const TkHostOutputInfo* outputInfo,
    void* userData);

typedef struct TkHostOutputCallbackInfo
{
    TkHostOutputCallback callback;
    void* userData;
} TkHostOutputCallbackInfo;

typedef struct TkServiceHostCreateInfo
{
    TkHostOutputCallbackInfo outputAdapter;
} TkServiceHostCreateInfo;
```

`generation == 0`인 connection key는 invalid argument다. `connectionId == 0`은 generation이 유효하면 허용한다. Host는 두 field를 함께 비교·hash하고 특정 NetworkRuntime session pointer나 전역 connection 번호를 가정하지 않는다. Output callback은 Create에서 non-null을 검증하고 Host 전체에 하나를 주입한다. One-way 전용 애플리케이션도 명시적인 no-op callback을 등록한다.

Executor callback은 queue admission만 표현한다.

```cpp
typedef TkResult (*TkServiceJobSubmitCallback)(
    TkServiceJob* job,
    void* userData);

typedef struct TkServiceExecutorCallbackInfo
{
    TkServiceJobSubmitCallback callback;
    void* userData;
} TkServiceExecutorCallbackInfo;
```

Submit 성공은 Job 소유권을 consume하고, 실패는 caller인 Host의 소유권을 보존한다. 실패한 executor는 Job을 보관·실행·파괴하지 않는다. 성공 직후 다른 worker가 submit 반환 전에 Job을 실행할 수는 있지만 callback 자체가 handler를 직접 실행하는 계약은 아니다. Callback은 non-null이며 예외를 C ABI 밖으로 전파하지 않는다.

Middleware는 metadata-only read-only seam이다.

```cpp
typedef struct TkServiceMiddlewareCallInfo
{
    TkHostConnectionKey connectionKey;
    uint16_t packetId;
} TkServiceMiddlewareCallInfo;

typedef TkResult (*TkServiceMiddlewareCallback)(
    const TkServiceMiddlewareCallInfo* callInfo,
    TkDiagnosticCallbackInfo diagnostic,
    void* userData);

typedef struct TkServiceMiddlewareInfo
{
    TkServiceMiddlewareCallback callback;
    void* userData;
} TkServiceMiddlewareInfo;
```

`callInfo`와 Diagnostic은 callback 동안만 borrowed다. Middleware callback은 non-null, read-only·reentrant이며 여러 ingress lane에서 동시에 호출될 수 있다. `TK_SUCCESS`는 계속 진행, `TK_ERROR_REJECTED`는 정책 거절, 나머지는 middleware 실행 실패다.

Handler thunk와 binding descriptor는 실제 C++ 타입을 `void*` storage와 함수 포인터로 지운다.

```cpp
typedef struct TkServiceContext
{
    TkHostConnectionKey connectionKey;
} TkServiceContext;

typedef TkResult (*TkRequestDecodeCallback)(
    TkByteView payload,
    void* requestStorage,
    TkDiagnosticCallbackInfo diagnostic);

typedef void (*TkRequestDestroyCallback)(void* requestStorage);

typedef TkResult (*TkOneWayHandlerCallback)(
    void* serviceInstance,
    const TkServiceContext* context,
    const void* requestStorage);

typedef TkResult (*TkResponseHandlerCallback)(
    void* serviceInstance,
    const TkServiceContext* context,
    const void* requestStorage,
    void* responseStorage);

typedef TkResult (*TkResponseEncodeCallback)(
    const void* responseStorage,
    TkMutableByteView output,
    TkDiagnosticCallbackInfo diagnostic);

typedef void (*TkResponseDestroyCallback)(void* responseStorage);

typedef enum TkBindingType
{
    TK_BINDING_TYPE_ONE_WAY = 1,
    TK_BINDING_TYPE_REQUEST_RESPONSE = 2
} TkBindingType;

typedef struct TkServiceRequestBindingInfo
{
    uint16_t packetId;
    size_t payloadBytes;
    size_t requestSize;
    size_t requestAlignment;
    TkRequestDecodeCallback decodeRequest;
    TkRequestDestroyCallback destroyRequest;
} TkServiceRequestBindingInfo;

typedef struct TkOneWayBindingInfo
{
    TkOneWayHandlerCallback invokeHandler;
} TkOneWayBindingInfo;

typedef struct TkRequestResponseBindingInfo
{
    uint16_t packetId;
    size_t payloadBytes;
    size_t responseSize;
    size_t responseAlignment;
    TkResponseHandlerCallback invokeHandler;
    TkResponseEncodeCallback encodeResponse;
    TkResponseDestroyCallback destroyResponse;
} TkRequestResponseBindingInfo;

typedef union TkServiceBindingOperation
{
    TkOneWayBindingInfo oneWay;
    TkRequestResponseBindingInfo requestResponse;
} TkServiceBindingOperation;

typedef struct TkServiceBindingInfo
{
    TkBindingType type;
    TkServiceRequestBindingInfo request;
    TkServiceBindingOperation operation;
} TkServiceBindingInfo;
```

`TkBindingType`이 active union member를 지정하므로 one-way binding에 가짜 response callback이나 null field를 채우지 않는다. Request binding에는 generated `PacketId`와 `PayloadBytes`만 복사한다. Payload version은 Host가 별도 wire parser로 중복 구현하지 않고, Host pipeline이 반드시 호출하는 generated Decode thunk가 검증한다.

Decode thunk는 raw storage에 request를 생성하고 Decode까지 수행한다. 성공하면 살아 있는 request 객체를 남기고, 실패하면 생성한 객체를 스스로 파괴해 storage에 live object를 남기지 않는다. Response handler thunk도 같은 failure-atomic 계약으로 response를 생성·호출하고, 실패 시 스스로 파괴한다. Encode thunk는 Host가 제공한 exact-size mutable buffer만 채우고 allocation하거나 payload owner를 반환하지 않는다.

서비스 단위 등록 parameter block은 다음과 같다.

```cpp
typedef struct TkServiceRegistrationInfo
{
    void* serviceInstance;
    TkServiceExecutorCallbackInfo executor;
    const TkServiceMiddlewareInfo* middlewares;
    size_t middlewareCount;
    const TkServiceBindingInfo* bindings;
    size_t bindingCount;
} TkServiceRegistrationInfo;
```

`middlewares`와 `bindings` 배열은 `RegisterService` 호출 동안만 borrowed이고, 성공 시 Host가 descriptor와 callback 값을 내부 registry에 복사한다. `serviceInstance`와 callback의 `userData`가 가리키는 실제 객체는 외부 소유다. Host는 typed facade의 compile-time 검증을 신뢰 경계로 삼지 않고 C ABI descriptor 전체를 다시 검증한 뒤 한 번에 commit한다. 실패한 등록은 기존 registry와 output을 보존한다.

Module query, Host lifecycle과 Job lifecycle은 다음 public operation으로 제한한다.

```cpp
TkResult TkServiceHostGetApiVersion(uint32_t* outVersion);

TkResult TkServiceHostCreate(
    const TkServiceHostCreateInfo* createInfo,
    TkServiceHost** outHost);

TkResult TkServiceHostRegisterService(
    TkServiceHost* host,
    const TkServiceRegistrationInfo* registration,
    TkDiagnosticCallbackInfo diagnostic);

TkResult TkServiceHostFinalizeRegistration(
    TkServiceHost* host,
    TkDiagnosticCallbackInfo diagnostic);

TkResult TkServiceHostProcessPacket(
    TkServiceHost* host,
    TkHostConnectionKey connectionKey,
    uint16_t packetId,
    TkByteView payload,
    TkDiagnosticCallbackInfo diagnostic);

void TkServiceHostDestroy(TkServiceHost* host);

TkResult TkServiceJobExecute(
    TkServiceJob* job,
    TkDiagnosticCallbackInfo diagnostic);

void TkServiceJobDestroy(TkServiceJob* job);
```

`GetApiVersion`의 첫 값은 `1`이며 Packet Tool과 같은 out-parameter 규칙을 사용한다. Host 상태는 `Create -> Configuring -> FinalizeRegistration -> Ready`로 전이한다. `RegisterService`는 Configuring에서만, `ProcessPacket`은 Ready에서만 허용하고 finalize 실패는 Configuring을 유지한다. Host가 thread나 queue를 소유하지 않으므로 `Start`와 `Stop`은 없다.

`TkServiceHostDestroy`는 내부 registry와 Host allocation만 즉시 정리하며 accepted Job, executor 또는 World 완료를 기다리거나 취소하지 않는다. Caller는 Destroy 전에 같은 Host에 대한 Register/Finalize/ProcessPacket 동시 호출을 모두 끝내야 한다. `TkServiceHostDestroy(nullptr)`는 no-op이고, 이중 Destroy와 파괴 후 Host 사용은 금지한다.

### 독립적인 입력·출력 adapter

Host core와 public 계약에는 `NrGateway`, `NrSessionSendChannel`, PrivateServer의 event 타입 또는 특정 World 구현을 넣지 않는다. Toolkit이 필요한 계약을 먼저 정하고 개별 runtime이 adapter로 연결한다. 기존 PrivateServer 코드는 연결 가능성의 참고이지 Host 설계의 필수 전제가 아니다.

| 역할 | 방향 | 책임 |
| --- | --- | --- |
| Host Input Adapter (`HostInputAdapter`) | NetworkRuntime → Host | Framing이 끝난 packet과 raw payload owner를 Ingress Job으로 만들고 connection-key 기반 ingress lane에 게시한다. Lane의 queue·worker·ordering 정책을 소유한다. |
| Host Output Adapter (`HostOutputAdapter`) | Host → NetworkRuntime | Host가 Encode한 borrowed bytes를 대상 연결의 송신 admission에 연결하고 수락·거절을 전달한다. |
| Service Executor | Host → consumer 실행 영역 | Opaque Service Job을 World queue·worker 등 지정된 실행 영역에 맡긴다. Service 등록 시 instance·middleware와 함께 주입한다. |

Input Adapter는 queue에 borrowed byte view만 남기지 않는다. 기존 NetworkRuntime의 move-only owning event를 보관하거나, 원본 storage의 lease를 소유하거나, 이동할 owner가 없으면 독립 payload storage를 만들어 Ingress Job의 수명을 보장해야 한다. Host lane worker는 Ingress Job이 살아 있는 동안 raw payload를 borrow하여 Core를 호출한다. Adapter가 특정 runtime의 표현을 변환하더라도 generated Decode와 패킷별 형식 검증은 Host의 책임으로 유지한다.

여러 NetworkRuntime producer는 connection key로 고정된 ingress lane에 게시한다. 각 lane은 MPSC mailbox와 한 명의 논리적 drain owner를 사용하고, 서로 다른 lane은 shared worker pool에서 병렬 실행할 수 있다. 같은 connection의 job은 같은 lane으로 보내 입력 순서를 유지한다. 물리 worker는 drain cycle마다 달라질 수 있으며 Service Host Core가 thread를 생성하거나 특정 worker를 고정하지 않는다. 공용 scheduler의 bounded queue·drain budget·lost-wakeup·shutdown 계약은 아래 execution module에서 정하고, lane 수·connection-key hash와 queue full에 대한 transport 정책은 개별 Input Adapter 설정으로 남긴다.

Output Adapter는 `JobExecute`를 수행한 World thread에서 동기 호출한다. Encoded payload view는 callback 동안만 유효하므로 adapter가 반환 이후 사용하려면 NetworkRuntime-owned send storage에 복사하거나 commit해야 한다. `TK_SUCCESS`는 send admission을 뜻할 뿐 실제 socket 완료나 상대 수신 완료가 아니며, 실패 시 Host가 자동 재시도하지 않는다. 여러 World worker의 동시 호출을 허용하므로 adapter가 사용하는 transport가 thread-safe하지 않으면 adapter가 직렬화한다.

### 검증과 입력 middleware

검증 책임은 세 계층으로 구분한다.

1. Service Host core는 등록된 packet의 존재와 generated codec의 exact payload size, payload version, Decode 같은 protocol invariant를 항상 검증한다. 생성된 codec은 신뢰할 수 있어도 외부에서 받은 bytes가 그 계약을 지킨다고 가정하지 않는다.
2. 입력 middleware는 진입점에서 서비스를 등록할 때 목록으로 지정한다. 해당 서비스의 모든 binding에 같은 목록을 적용하며, 각 요청은 목록 순서대로 검사를 통과하거나 handler 전달 전에 거절된다. Middleware는 Host Connection Key와 PacketId의 read-only metadata만 보고 raw/typed payload, World state, response와 Host/executor pointer에는 접근하지 않는다.
3. Handwritten service handler는 session/entity ownership, target tick과 game state 같은 domain semantic validation을 소유한다.

서비스 단위 공통 적용은 middleware 목록의 공통 적용이다. 요청 하나가 모든 handler를 실행하는 것이 아니라, 해당 패킷에 연결된 handler 하나만 선택된다. 첫 버전에서 handler별 middleware 추가·제외·덮어쓰기는 지원하지 않으며, middleware 목록이 없어도 Core의 기본 검증은 유지한다.

Middleware callback은 read-only·reentrant 계약이며 여러 ingress lane에서 동시에 호출될 수 있다. 참조하는 policy snapshot은 concurrent read와 lifetime을 보장해야 하고, 인증 cache나 rate-limit counter 같은 mutable state가 필요하면 별도로 주입한 thread-safe policy object가 소유한다.

Middleware는 별도 decision/result 타입을 만들지 않고 공용 `TkResult`를 반환한다. `TK_SUCCESS`이면 다음 middleware 또는 handler로 진행하고, 정책 거절에는 공용 값 `TK_ERROR_REJECTED`를 추가해 사용하기로 했다. 그 밖의 실패는 middleware 실행 실패로 취급한다. 구체 원인은 공용 Diagnostic으로 전달하며 Host가 diagnostic ID를 parsing해 제어 흐름을 정하지 않는다. `TK_ERROR_REJECTED`의 Common 구현과 consumer 대응은 후속 implementation slice에 포함해야 한다.

**후처리 middleware는 이번 범위에서 제외한다.** World 완료 후 역순으로 middleware를 실행하거나, 완료 결과·응답을 공통 후처리 체인에 통과시키지 않는다. 미래 사용을 위한 빈 completion hook도 선행 추가하지 않는다.

Host가 소유한 데이터의 정리, 전달 실패와 안전한 송신에 필요한 기본 수명 조건은 아래 API와 Job 계약으로 명시한다. World가 수락한 작업을 실행·폐기하는 정책이나 전체 서버의 종료 전략은 Host의 책임으로 가져오지 않는다.

### Host processing 완료 의미와 실패 표면

`TkServiceHostProcessPacket`은 packet lookup → exact payload size 확인 → Job allocation → generated Decode → 등록 순서의 middleware → executor submit을 동기적으로 수행한다. 입력 payload는 호출 동안만 borrowed다. `TK_SUCCESS`는 검증된 Service Job을 executor가 인수했다는 의미이고 handler 실행·완료를 뜻하지 않는다. Submit 성공 뒤 다른 worker가 Job을 즉시 실행해도 Host는 그 결과를 기다리거나 ProcessPacket 결과에 합치지 않는다.

| 상황 | `TkResult`와 처리 |
| --- | --- |
| null Host, invalid byte view, `generation == 0` connection key | `TK_ERROR_INVALID_ARGUMENT` |
| Configuring Host에서 ProcessPacket, Ready Host에서 Register 등 잘못된 lifecycle | `TK_ERROR_INVALID_STATE` |
| 등록되지 않은 PacketId | `TK_ERROR_INVALID_DATA` + Host Diagnostic |
| payload 크기 불일치 | `TK_ERROR_INVALID_DATA` + Host Diagnostic |
| payload version 또는 Decode 실패 | generated codec의 `TK_ERROR_INVALID_DATA`와 Diagnostic 전파 |
| middleware 정책 거절 | `TK_ERROR_REJECTED` 전파 |
| Job allocation 실패 | `TK_ERROR_OUT_OF_MEMORY` |
| executor queue full | `TK_ERROR_CAPACITY_EXCEEDED` 전파 |
| executor stopping | `TK_ERROR_INVALID_STATE` 전파 |
| 그 밖의 executor·handler·Encode·Output Adapter 실패 | 첫 실패의 `TkResult` 그대로 전파 |

등록되지 않은 PacketId를 registry 조회용 `NOT_FOUND`로 분리하지 않는다. ProcessPacket은 외부 bytes를 해석하는 operation이므로 wire 형식 오류와 같은 `TK_ERROR_INVALID_DATA`로 분류하고 stable Diagnostic ID로 원인을 구분한다. Common `TkResult`에는 E1에서 기존 numeric value를 보존한 채 `TK_ERROR_INVALID_STATE`, `TK_ERROR_CAPACITY_EXCEEDED`, `TK_ERROR_REJECTED`를 append한다.

`RegisterService`는 service instance, executor callback, binding count·array, middleware pointer/count, 모든 callback, known `TkBindingType`, size/alignment와 allocation overflow를 먼저 검증한다. Size는 0보다 크고 alignment는 non-zero power-of-two여야 한다. 같은 registration 내부와 기존 registry의 PacketId 중복을 모두 검사하고 필요한 memory 확보까지 성공한 뒤 descriptor 전체를 commit한다. 실패는 해당 service의 부분 binding을 남기지 않으며 이전 registry를 보존한다.

### 서비스 등록 편의

- 서비스 함수는 일반 C++ handler로 작성하고, **서비스 정의 옆에 패킷 타입과 특정 handler의 연결 선언을 모은다.** 진입점에서는 handler를 하나씩 등록하지 않고 서비스 단위로 등록한다.
- 연결 선언에는 패킷 타입과 handler를 둘 다 명시한다. Handler 인자에서 패킷 타입을 자동 추론하는 기능은 첫 범위에서 제외한다. 함수 이름은 패킷 이름과 같을 필요가 없으며 선언된 연결과 타입 호환성으로 판단한다.
- 진입점의 등록은 서비스 객체, executor와 입력 middleware 목록을 연결한다. 실제 요청 데이터는 등록 시가 아니라 패킷 수신 시 들어온다.
- Handler 자체가 서비스 로직을 실행하는 callback이다. 별도의 World callback 계층을 반드시 하나 더 두지 않는다.
- 첫 버전은 template-first 명시적 등록을 사용한다. Macro가 필요해져도 같은 template API를 감싸는 얇은 편의로만 추가하며 전역 자동 등록이나 discovery를 만들지 않는다.
- Packet Tool이 생성한 `PacketId`, `PayloadBytes`, `Encode/Decode`를 재사용한다. 서비스 작성자가 동일한 번호·크기·codec 연결을 중복 기입하는 방향은 피한다.
- 명시된 요청 타입을 handler가 받을 수 있는지는 C++17 template/type traits로 컴파일 시 검증한다. 첫 버전은 아래의 `noexcept` one-way와 request/response signature만 지원한다.
- 서비스 객체는 애플리케이션이 생성하여 등록 시 주입하고 모든 accepted Service Job이 해제될 때까지 수명을 보장한다. Host는 borrowed instance pointer만 보관한다. 애트리뷰트를 프로젝트 전체에서 탐색해 자동 발견·등록하는 기능, 범용 DI container와 shared ownership은 후속 검토로 둔다.

Typed facade의 사용 형태는 다음과 같다. 최종 함수 spelling은 H4에서 이 형태를 유지하며 구현 코드 스타일에 맞춰 확정한다.

```cpp
constexpr auto timeServiceBindings = pstk::service::MakeBindings(
    pstk::service::BindRequestResponse<
        TimeService,
        WorldTimeSyncRequest,
        WorldTimeSyncResponse,
        &TimeService::OnTimeSync>(),

    pstk::service::BindOneWay<
        TimeService,
        B,
        &TimeService::OnB>());

pstk::service::RegisterService(
    host,
    timeService,
    worldExecutor,
    middlewares,
    timeServiceBindings,
    diagnostic);
```

**같은 Host 안에서는 하나의 `PacketId`를 하나의 handler에만 연결한다.** 다른 서비스나 다른 패킷 타입 이름으로 선언했더라도 generated `PacketId`가 중복되면 해당 서비스 등록을 `TkResult` 실패로 처리한다. 기존 연결을 덮어쓰거나 여러 handler를 동시에 연결하지 않는다. 실패한 서비스 등록 시도의 binding은 부분 반영하지 않으며, 이전까지 성공한 등록 상태를 보존한다. 서로 다른 Host 인스턴스에서 같은 패킷을 사용하는 것은 허용한다.

Facade는 packet·handler 타입 호환성과 `noexcept` signature를 검증하고 임시 C ABI descriptor 배열을 구성한 뒤 `TkServiceHostRegisterService`를 호출한다. Host가 배열 내용을 복사하므로 facade의 임시 배열 수명을 등록 이후까지 늘리지 않는다.

### 실행 전달과 소유권

입력과 World 실행 사이에는 수명이 다른 두 작업이 존재한다.

| 작업 | 소유 데이터 | 종료 시점 |
| --- | --- | --- |
| Ingress Job | 아직 Decode하지 않은 packet metadata와 raw payload owner 또는 lease | Host Core가 검증·Decode·World 제출을 마치거나 중간 실패로 중단한 뒤 |
| Service Job | Host Connection Key, decoded typed request, borrowed service instance, handler/codec thunk, Output Adapter callback 값과 response scratch | World가 handler를 실행하거나 job을 폐기하고 opaque handle을 해제한 뒤 |

Input Adapter worker는 queue에서 Ingress Job을 drain하고 그 owner가 살아 있는 동안 payload view를 빌려 Host Core를 호출한다. Host는 lookup과 기본 검증 뒤 **Service Job의 request 저장소에 직접 Decode**한다. `원본 → Host 임시 DTO → World DTO` 형태의 중간 사본은 만들지 않으며, raw payload를 World 실행 완료까지 유지하지도 않는다. Decode나 middleware가 실패하면 Service Job을 제출하지 않고, Host가 생성 중인 job/request 저장소를 파괴한 뒤 Ingress Job을 정리한다.

검증과 middleware를 통과하면 Host는 C ABI의 opaque single-owner Service Job handle을 service에 등록된 executor callback으로 제출한다.

- Executor가 수락하면 World 측이 job handle과 decoded request의 실행·보관·정리를 책임진다. Host는 더 이상 handle을 사용하지 않는다.
- Executor가 거절하면 handle 소유권은 Host에 남고 자동 재시도 없이 실패를 드러낸 뒤 같은 DLL API로 정리한다. 성공은 consume, 실패는 caller 소유 상태 보존이라는 conditional ownership transfer를 public callback 계약으로 고정한다.
- Executor submit callback은 queue admission만 수행하고 그 자리에서 handler를 직접 호출하지 않는다. 다른 World worker가 빠르게 소비해 submit 반환 전에 job을 실행하는 것은 금지하지 않는다.
- World는 선택한 시점에 DLL의 JobExecute operation으로 job을 실행한다. Handler는 실행 중에만 decoded request를 `const&`로 빌리고 job 이후에 보관하지 않는다.
- 작업 접수와 handler 실행 완료는 다르며 Host는 World 완료를 기다리거나 추적하지 않는다.

Service Job은 생성 시 실행에 필요한 pointer·function pointer·connection key와 Output Adapter callback info를 값으로 snapshot하고 Host registry나 Host object를 다시 참조하지 않는다. Request 데이터도 Host 임시 DTO를 거치지 않고 Job storage에 직접 Decode하므로 Host에서 World로 전달할 때 typed request 복사는 없다. C ABI에서 raw handle을 전달하지만 submit 성공 이후의 single-owner 의미는 move semantics와 같다.

Job은 한 allocation 안에 metadata, aligned request storage, request/response binding이면 aligned response storage와 encoded response payload buffer를 배치한다. Response 객체는 `JobExecute` 안에서만 생성하고 성공·실패 경로 모두 반환 전에 파괴한다. One-way Job은 response storage와 encoded buffer를 포함하지 않는다. Request 객체와 전체 allocation은 `TkServiceJobDestroy`가 정리한다.

`TkServiceJobExecute`와 `TkServiceJobDestroy`는 분리한다. 정상 경로는 Execute 한 번 뒤 Destroy 한 번, discard 경로는 Execute 없이 Destroy 한 번이다. Execute는 Job ownership을 consume하거나 메모리를 해제하지 않는다. 첫 Execute는 handler 결과와 관계없이 내부 상태를 Executed로 전이하고, 두 번째 Execute는 handler를 다시 호출하지 않고 `TK_ERROR_INVALID_STATE`를 반환한다. Job은 single-owner이므로 상태는 non-atomic이고 같은 handle에 대한 동시 Execute/Destroy, Destroy 이후 사용과 이중 Destroy는 지원하지 않는다.

Accepted Job은 self-contained이므로 Host object 파괴 후에도 실행·폐기할 수 있다. 다만 service instance, callback `userData`, consumer thunk code와 Host DLL은 모든 accepted Job이 해제될 때까지 유효해야 한다. 애플리케이션이 ingress와 Host 동시 호출을 중단한 뒤 Host를 파괴하고, executor의 drain 또는 폐기를 조율한 뒤 나머지 의존 객체를 파괴한다. **World가 수락한 작업을 실행할지 폐기할지, World queue와 전체 서버를 어떤 순서로 종료할지는 World/executor와 애플리케이션의 정책**이며 Host의 필수 drain 정책이나 검증 gate가 아니다.

### Handler와 요청 문맥

첫 버전은 one-way와 handler 반환 안에서 완료되는 request/response 두 형태만 지원한다.

```cpp
TkResult OnB(const TkServiceContext& context, const B& request) noexcept;

TkResult OnTimeSync(const TkServiceContext& context,
                    const WorldTimeSyncRequest& request,
                    WorldTimeSyncResponse* outResponse) noexcept;
```

- `TkServiceContext`는 특정 NetworkRuntime의 session 타입 대신 generation-safe `TkHostConnectionKey`만 제공한다. 별도 ServiceId는 두지 않고 service binding 조회에는 generated PacketId를 사용한다.
- Request와 context는 JobExecute 동안만 유효한 borrowed reference다.
- Response output은 non-null로 전달하며 handler가 실패하면 호출 전 상태를 보존한다.
- One-way 성공은 응답을 만들지 않는다. Response handler가 성공하면 generated codec으로 Encode하고 Output Adapter에 제출하며, 실패하면 응답을 Encode·전송하지 않는다.
- Future, coroutine, stored completion callback과 handler 반환 뒤의 지연 응답은 첫 버전에서 지원하지 않는다.

첫 버전의 typed facade는 handler가 위와 같은 `noexcept` signature인지 compile time에 검증하고, C ABI thunk도 예외를 경계 밖으로 전파하지 않는 호출 계약을 사용한다. Thunk는 `void*` service/request/response storage를 template parameter의 실제 타입으로 복원해 호출한다. `noexcept` handler가 예외를 던지는 것은 복구 가능한 `TkResult` 실패가 아니라 consumer의 계약 위반이다.

### Diagnostic 호출 수명

기존 공용 Diagnostic의 synchronous borrowed 계약을 유지한다. Ingress lane worker가 호출하는 `TkServiceHostProcessPacket`은 core validation·middleware·World submit 중 발생한 Diagnostic을 해당 호출의 `TkDiagnosticCallbackInfo`로 전달하고 callback info를 저장하지 않는다. World가 나중에 호출하는 `TkServiceJobExecute`도 handler·response Encode·Output Adapter 중 발생한 Diagnostic callback info를 별도로 받아 그 호출 안에서만 사용한다.

애플리케이션은 두 operation에 같은 logger를 가리키는 callback info를 전달할 수 있지만 Host와 Service Job은 callback, `userData` 또는 borrowed Diagnostic pointer를 호출 이후 보관하지 않는다. Diagnostic 활성 여부나 ID parsing으로 `TkResult` 제어 흐름을 바꾸지 않는다.

### 응답 지원과 RPC의 구분

전용 요청·응답 패킷과 handler로 TimeSync 같은 request/response 서비스를 작성할 수 있다. Response handler thunk가 성공하면 Host가 Job 내부의 exact-size buffer에 generated Encode thunk를 실행하고 `TkHostOutputCallback`으로 송신 admission을 요청한다. Encoded payload와 `TkHostOutputInfo`는 callback 동안만 borrowed이며, callback이 반환 이후 사용할 데이터는 adapter가 NetworkRuntime-owned send storage에 복사하거나 commit한다. `TK_SUCCESS`는 송신 접수일 뿐 실제 송신 또는 상대 수신 완료가 아니다.

`TkServiceJobExecute`는 one-way이면 handler 결과까지만, request/response이면 handler → Encode → Output Adapter의 첫 실패까지 실행한다. 첫 실패의 `TkResult`를 변환하거나 재시도하지 않고 그대로 반환하며 이후 단계는 실행하지 않는다. Handler와 Output Adapter 실패에는 Host stage Diagnostic을 emit하고, generated Encode의 구체 원인은 codec Diagnostic을 사용한다. 생성된 response 객체는 어느 결과에서도 Execute 반환 전에 파괴한다.

이 지원을 범용 RPC 자동 관리와 구분한다. `callId` 자동 부여·응답 매칭, pending-call registry, timeout·취소·late/duplicate completion 관리, client stub은 이번 필수 범위로 삼지 않는다. 필요한 요청 번호나 응답 대응 규칙을 개별 서비스/호출 측에서 정하는 방식은 허용한다. One-way 입력·알림도 요청마다 응답을 요구하는 형태로 강제하지 않는다.

### 별도 공용 실행 계층과 레퍼런스

- 기존 header-only Common은 기초 타입·byte view·result·diagnostic 계약으로 유지한다. 실행 도구를 이 계층에 섞거나 generated codec consumer에 runtime binary 의존성을 강제하지 않는다.
- NetworkRuntime·Host Input Adapter·향후 WorldRuntime이 공유할 queue와 scheduling 도구는 `execution/`의 internal STATIC target `pstk_execution`, namespace `pstk::execution`으로 구분한다. 외부 설치·public DLL ABI 대상이 아니며 각 runtime component가 내부 링크한다.
- Akka의 mailbox/job queue와 실행 예약 구조를 레퍼런스로 삼아 예약 상태를 mailbox 안에 캡슐화한다. Producer publish와 worker의 release 후 recheck가 서로 보완해 drain 종료 경쟁의 lost wakeup을 닫는다.
- Nakama의 match/tick 구조는 향후 WorldRuntime이 서비스 입력을 소비하는 방식을 설계할 때 참고한다. 이번 Host에 match/room 정책을 넣는 근거로 삼지 않는다.

공용 실행 도구를 Host Core 내부 구현으로 숨겨 고정하지 않는다. Input Adapter가 ingress lane과 worker scheduling을 조립하고, 서비스별 World executor는 별도로 주입된다.

#### `TkBoundedMpscQueue<T>`

- 고정 capacity의 bounded MPSC ring이며 capacity는 2 이상 power-of-two다. 입력값을 반올림하지 않고 잘못된 capacity는 factory에서 `TK_ERROR_INVALID_ARGUMENT`으로 거절한다.
- Slot storage와 per-slot sequence는 생성 시 한 번 할당하며 hot path에 allocation을 두지 않는다. 별도 memory pool은 첫 버전에 추가하지 않는다.
- Index는 monotonic position과 `position & (capacity - 1)`로 계산한다. Power-of-two 규칙을 execution module의 bounded queue에 공통 적용한다.
- `TryPush(T&&)`는 non-blocking이고 full이면 입력 object를 move하지 않아 caller 소유 상태를 보존한다.
- `TryPop(T& out) noexcept`는 성공할 때만 out에 move-assign하고 empty이면 out을 보존한다. `optional`은 사용하지 않는다.
- `T`는 `noexcept` move construction, move assignment와 destruction을 만족해야 하며 default construction과 copy는 요구하지 않는다.
- Queue object는 non-copyable·non-movable이고 static `TkResult` factory가 owning smart pointer를 만든다. `Close`, wait, wakeup은 queue 책임이 아니다.
- 파괴 전 producer/consumer quiescence는 외부 계약이다. Destructor는 남아 있는 element를 파괴한다.

#### `TkWorkItem`

```cpp
using TkWorkInvoke = void (*)(void*) noexcept;
using TkWorkDestroy = void (*)(void*) noexcept;
```

- `TkWorkItem`은 context와 항상 non-null인 invoke/destroy callback을 소유하는 move-only RAII value다.
- 정상 worker 경로는 invoke 정확히 한 번 뒤 destroy 정확히 한 번, discard 경로는 invoke 없이 destroy 정확히 한 번을 호출한다.
- 소멸이 필요 없는 context도 명시적인 no-op destroy callback을 전달하며 worker loop는 null branch를 두지 않는다.
- Move된 원본은 empty가 되고 새 owner만 lifecycle callback을 호출한다.

#### `TkWorkerPool`

- 고정 worker 수와 power-of-two capacity의 shared bounded ready ring을 생성 시 정한다. Ready ring은 여러 producer와 consumer가 쓰는 lock-based MPMC이며 mutex와 condition variable로 대기·wake를 관리한다.
- Static factory 성공 시 worker thread를 시작하고 곧바로 Running이다. 별도 `Start`는 없다.
- `TrySchedule(TkWorkItem&&)`는 짧은 queue mutex 구간만 사용하는 non-blocking admission이다. 성공은 item ownership consume, capacity 실패와 invalid state 실패는 caller 소유 보존이다.
- Worker는 condition variable에서 대기하고 schedule은 `notify_one`, stop은 `notify_all`을 사용한다. WorkItem invoke는 queue mutex 밖에서 실행한다.
- Stop mode는 `Drain`과 `Discard`를 명시한다. Stop은 모든 worker를 join하는 synchronous operation이며 같은 pool의 worker에서는 호출할 수 없다.
- Lifecycle owner는 하나이고 concurrent Stop 호출을 지원하지 않는다. 순차적인 repeated Stop은 success no-op이며 destructor fallback은 `Stop(Discard)`다.

#### `TkSerialMailbox<T>`와 `TkMailboxScheduler<T>`

- Mailbox는 bounded MPSC queue와 `Idle`, `Scheduled`, `Draining` CAS 상태만 가진다. 별도 gate·permit object를 외부에 노출하지 않는다.
- Producer는 message publish 후 `Idle -> Scheduled` CAS에 성공한 경우에만 mailbox drain WorkItem을 ready queue에 게시한다. 이미 Scheduled/Draining이면 현재 owner가 후속 message를 관찰하므로 추가 예약하지 않는다.
- Worker는 Scheduled mailbox를 Draining으로 전이해 `maxMessagesPerDrain` count budget만큼 소비한다. 이후 Idle로 release한 다음 queue를 다시 확인하고 남은 message가 있으면 스스로 재예약한다. Release 전 publish와 release 후 publish 모두 producer 또는 worker 중 하나가 예약을 책임져 orphan message를 만들지 않는다.
- Scheduler는 WorkerPool과 생성 시 등록한 모든 Mailbox를 소유한다. 실행 중 register/unregister, slot reuse와 restart는 지원하지 않는다.
- Producer는 scheduler identity와 slot을 가진 opaque value handle만 사용한다. Slot 재사용이 없으므로 generation은 두지 않으며 다른 scheduler의 handle은 invalid argument다.
- 한 Scheduler는 동일한 `T`, 동일한 power-of-two mailbox capacity와 명시적인 `maxMessagesPerDrain >= 1`을 사용한다. Mailbox별 registration은 borrowed context와 non-null `void (*)(void*, T&&) noexcept` invoke를 제공한다.
- Ready capacity는 등록 mailbox 수 이상으로 생성해 Running 상태에서 mailbox scheduling이 capacity 때문에 실패하지 않도록 한다. Scheduler가 허용한 registered mailbox만 scheduling할 수 있다.
- `Stop(Drain)`은 새 post를 닫고 in-flight post를 기다린 뒤 모든 mailbox가 Empty+Idle이 될 때까지 pool을 Running으로 유지하고 마지막에 pool을 Drain한다. `Stop(Discard)`는 ready work를 폐기·join한 뒤 mailbox와 queued message를 파괴한다.
- Stop 이후 handle은 invalid이고 Scheduler는 terminal state다. Destructor fallback은 `Stop(Discard)`다.

Connection key를 lane handle로 매핑하는 실제 hash, lane 수와 ingress full 시 connection/drop 정책은 특정 Host Input Adapter의 조립 정책으로 남긴다. #2는 E4에서 generic scheduler의 ordering·lost-wakeup·shutdown 계약을 검증하고, 실제 NetworkRuntime adapter를 완료 조건으로 요구하지 않는다.

## 책임 경계

### Service Host가 소유

- 공통 패킷 검증·Decode와 typed handler binding
- Immutable binding registry와 C ABI descriptor/thunk 연결
- 입력 middleware의 순서와 거절 처리
- Opaque Service Job의 생성·JobExecute·해제와 conditional ownership transfer
- 서비스 등록 편의와 응답 Encode·Host Output Adapter 호출
- 전달 실패와 handler 오류가 묵살되지 않도록 하는 기본 오류 처리

### NetworkRuntime이 소유

- socket, IOCP/epoll과 OS I/O
- TCP framing
- recv/send buffer lifetime
- physical connection과 transport shutdown

### Host Input Adapter가 소유

- NetworkRuntime owning event·lease 또는 독립 payload storage를 Ingress Job으로 보존
- Host Connection Key 생성과 같은 connection의 lane routing·ordering
- Ingress lane queue·drain worker와 게시 실패·backpressure·shutdown 정책
- Lane worker에서 Host processing operation의 `TkResult`와 Diagnostic 처리

### Host Output Adapter가 소유

- Host Connection Key를 transport의 generation-safe 송신 대상으로 연결
- Callback 동안 borrowed encoded bytes를 Runtime-owned send storage에 복사하거나 commit
- 동시 callback의 transport 직렬화와 send admission 실패 처리

### Consumer service와 World가 소유

- session/entity/World ownership
- tick admission과 command ordering
- gameplay semantic validation
- World state mutation과 business/gameplay logic
- service별 executor와 middleware policy 선택
- 수락한 Service Job과 decoded request의 실행·보관·정리 및 World 내부 종료 정책

애플리케이션 진입점은 서비스와 각 계층을 조립하고 객체 수명을 조율한다. World 작업의 drain 또는 폐기 정책을 Service Host가 대신 선택하지 않는다.

## 제외하거나 후속으로 둔 범위

- 범용 async unary RPC 자동 호출 관리와 client stub
- World 완료 후 역순 middleware, 공통 응답 변환 및 선행 completion hook
- 애트리뷰트 기반 프로젝트 전체 서비스 자동 탐색·등록
- 실행 중 service register·unregister·replace와 hot reload
- Handler 인자에서의 패킷 타입 자동 추론, handler별 middleware 추가·제외·덮어쓰기
- 하나의 Host에서 같은 패킷을 여러 handler에 배포하는 구독 모델
- Host Core가 직접 소유하는 ingress thread pool·고정 worker·전역 mailbox
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

## 결정 상태와 명시적 보류

**Grilling complete.** 목적지·범위, execution 도구, public ABI, descriptor/thunk, ownership, lifecycle, 실패 표면과 first vertical slice에 남은 material branch는 없다. 구현 중 현재 계약을 바꿀 필요가 생기면 코드를 임의 확장하지 않고 이 문서의 해당 stable slice를 먼저 갱신한다.

명시적으로 후속으로 보류한 항목은 다음과 같다.

- 실제 NetworkRuntime adapter의 lane 수, connection-key hash와 queue-full transport 정책
- 실제 WorldRuntime executor, tick/ECS admission과 전체 서버 shutdown 조립
- memory pool, 별도 spin lock과 범용 allocator 주입
- Template facade 위의 선택적 macro spelling
- async completion, timeout/cancel, pending-call registry와 client stub

GitHub Issue 본문의 readiness, template-first facade와 stable slice 문구는 2026-09-01 동기화했다. `TkResult`, byte view, Diagnostic, generated-code consumer 경계와 shared-library ABI는 기존 [ADR](../adr/)을 따른다.

## Dependency-ordered implementation slices

`E`는 공용 execution 계층, `H`는 Service Host 계층을 뜻한다. 사용자가 승인한 delivery 순서는 `E1 -> E2 -> E3 -> E4 -> H1 -> H2 -> H3 -> H4`다. Host Core가 `pstk_execution`에 직접 링크된다는 의미는 아니며, E4를 먼저 끝내 공용 scheduling 계약을 확정한 뒤 Host를 구현한다는 delivery gate다.

| ID | Delivered outcome | Dependency | Status |
| --- | --- | --- | --- |
| E1 | Common `TkResult` control-flow 값 확장 | 없음 | Pending |
| E2 | `pstk_execution`의 bounded MPSC queue와 move-only WorkItem | E1 | Pending |
| E3 | Bounded ready queue 기반 WorkerPool | E2 | Pending |
| E4 | CAS serial mailbox와 owning MailboxScheduler | E3 | Pending |
| H1 | Service Host shared target, public C ABI와 immutable registry | E4 delivery gate | Pending |
| H2 | ProcessPacket, middleware와 one-way Service Job pipeline | H1 | Pending |
| H3 | Request/response storage, Encode와 Output Adapter pipeline | H2 | Pending |
| H4 | C++ typed facade와 TimeSync end-to-end acceptance | H3 | Pending |

### E1 — Common result 확장

- **Outcome:** 기존 `TkResult`에 `TK_ERROR_INVALID_STATE = -7`, `TK_ERROR_CAPACITY_EXCEEDED = -8`, `TK_ERROR_REJECTED = -9`를 append한다.
- **Seam:** `common/include/pstk/TkResult.h`의 project-wide control-flow contract.
- **Invariant:** 기존 `0`, `-1`~`-6` numeric ABI를 변경하지 않고 모듈별 result를 추가하지 않는다.
- **Acceptance:** C/C++ Common contract가 모든 numeric value를 고정하고 기존 Packet Tool source가 새 값 추가로 영향을 받지 않는다.
- **Verification:** Common contract compile/test와 전체 configure/build/CTest.

### E2 — Bounded MPSC queue와 WorkItem

- **Outcome:** `execution/` internal STATIC target, `TkBoundedMpscQueue<T>`와 `TkWorkItem`을 제공한다.
- **Dependency:** E1.
- **Seam:** producer/consumer 사이의 move-only bounded admission과 arbitrary work lifecycle.
- **Invariant:** Full/empty 실패는 caller input/out을 보존하고, accepted item은 정확히 한 owner와 정확히 한 destroy path를 가진다. Queue hot path에는 allocation이 없다.
- **Acceptance:** invalid capacity, wrap-around, empty/full, multi-producer single-consumer, move-only non-default type과 queued destruction을 검증한다. WorkItem 정상 경로는 invoke→destroy, discard는 destroy-only이며 no-op destroy도 같은 경로를 사용한다.
- **Verification:** deterministic unit tests, bounded concurrent stress와 sanitizer를 사용할 수 있는 preset에서는 address/undefined behavior 검사.

### E3 — WorkerPool

- **Outcome:** fixed worker와 bounded MPMC ready ring을 소유하는 `TkWorkerPool`을 제공한다.
- **Dependency:** E2.
- **Seam:** `TrySchedule(TkWorkItem&&)` admission과 synchronous `Stop(Drain/Discard)`.
- **Invariant:** Schedule 성공만 ownership을 consume하고 invoke는 queue mutex 밖에서 실행한다. Concurrent Stop과 worker 내부 Stop은 지원하지 않는다.
- **Acceptance:** worker wakeup, 여러 producer/consumer 실행, capacity failure input 보존, Drain/Discard 차이, worker 내부 Stop 거절, sequential repeated Stop no-op와 destructor fallback을 검증한다.
- **Verification:** unit/concurrency tests에서 모든 accepted WorkItem의 invoke/destroy count와 join 완료를 확인한다.

### E4 — SerialMailbox와 MailboxScheduler

- **Outcome:** `TkSerialMailbox<T>`, opaque mailbox handle과 owning `TkMailboxScheduler<T>`를 제공한다.
- **Dependency:** E3.
- **Seam:** 여러 producer의 publish와 shared worker pool의 single-owner drain 연결.
- **Invariant:** 한 mailbox는 동시에 한 drain owner만 가지며 release-to-Idle 후 recheck가 lost wakeup을 닫는다. Running scheduler의 registered mailbox scheduling은 ready capacity 때문에 실패하지 않는다.
- **Acceptance:** publish/drain 종료 경쟁, 동일 mailbox 직렬성, 서로 다른 mailbox 병렬성, drain budget fairness, 잘못된 handle, post close, Drain/Discard shutdown과 queued message destruction을 검증한다.
- **Verification:** latch/barrier로 경합 시점을 통제한 unit tests와 반복 concurrent stress. 특정 NetworkRuntime adapter나 lane hash는 만들지 않는다.

### H1 — Public ABI와 immutable registry

- **Outcome:** `runtime/service_host/`의 `pstk_service_host` shared target, public C header, module version query와 Configuring/Ready Host registry를 제공한다.
- **Dependency:** E4 delivery gate. Host target에는 `pstk_execution` link dependency를 추가하지 않는다.
- **Seam:** `TkServiceHostCreate`, service-level descriptor 등록, `FinalizeRegistration`, `Destroy`.
- **Invariant:** 모든 C ABI descriptor를 방어적으로 검증·복사하고 failed registration/finalize는 기존 상태를 보존한다. 같은 Host의 PacketId는 하나의 binding만 가진다.
- **Acceptance:** API version, invalid Create/registration, empty binding, invalid callback/size/alignment/type, registration 내부·기존 registry 중복, atomic failure와 lifecycle state를 검증한다.
- **Verification:** shared-library smoke, public header compile, registry unit tests와 install/build-tree consumer 확인.

### H2 — One-way Service Job pipeline

- **Outcome:** `ProcessPacket`, metadata-only middleware, executor submit과 one-way `TkServiceJobExecute/Destroy`를 구현한다.
- **Dependency:** H1.
- **Seam:** borrowed raw payload에서 self-contained owning Job으로의 변환과 conditional ownership transfer.
- **Invariant:** Request는 Job storage에 직접 Decode하며 raw input lifetime을 연장하지 않는다. Submit 성공만 ownership을 consume하고 실패는 Host가 파괴한다. Execute와 Destroy는 분리되고 handler는 최대 한 번 호출된다.
- **Acceptance:** unknown PacketId, size/version/Decode 오류, middleware 순서·거절·동시 호출, executor success/failure, Host 파괴 후 accepted Job 실행·discard, 중복 Execute 거절과 Diagnostic 호출 수명을 검증한다.
- **Verification:** memory input/no-op output와 capturing executor를 사용한 deterministic unit tests. Handler는 Execute 전 호출되지 않아야 한다.

### H3 — Request/response와 Output Adapter

- **Outcome:** response storage·handler thunk·generated Encode와 synchronous Host Output Adapter 경로를 구현한다.
- **Dependency:** H2.
- **Seam:** World handler의 typed response를 exact-size encoded payload send admission으로 변환한다.
- **Invariant:** Job은 request/response/encoded buffer를 한 allocation에 소유하고 response live object는 Execute 반환 전에 항상 파괴한다. 첫 실패 뒤 후속 단계와 retry는 실행하지 않는다.
- **Acceptance:** handler·Encode·Output Adapter success/failure 각각의 반환과 Diagnostic, 올바른 connection key/response PacketId/bytes, callback borrowed lifetime과 one-way 무응답을 검증한다.
- **Verification:** memory Output Adapter와 generated request/response codec을 사용한 pipeline unit tests.

### H4 — Typed facade와 TimeSync vertical slice

- **Outcome:** `pstk/service_host/TkServiceBinding.hpp`의 template-first binding facade와 TimeSync end-to-end acceptance를 제공한다.
- **Dependency:** H3.
- **Seam:** typed C++ service declaration을 stable C ABI descriptor/thunk와 startup registration으로 변환한다.
- **Invariant:** Packet 타입과 handler를 모두 명시하고 지원 signature·`noexcept` 호환성을 compile time에 검증한다. Facade는 registry나 scheduling state를 소유하지 않는다.
- **Acceptance:** 기존 [WorldTimeSyncRequest](../../tools/packet/cli/tests/schemas/WorldTimeSyncRequest.json)와 [WorldTimeSyncResponse](../../tools/packet/cli/tests/schemas/server/WorldTimeSyncResponse.json)을 사용해 아래 흐름을 특정 PrivateServer 없이 완료한다.

```text
owned request bytes
-> TkServiceHostProcessPacket
-> exact size/version/Decode
-> service middleware
-> capturing executor가 TkServiceJob 인수
-> 테스트가 선택한 시점에 TkServiceJobExecute
-> TimeService::OnTimeSync
-> WorldTimeSyncResponse Encode
-> memory Output Adapter
-> response bytes Decode·비교
-> TkServiceJobDestroy
```

- **Verification:** supported/unsupported handler trait compile checks, one-way와 request/response facade unit tests, shared target을 통한 TimeSync integration test와 전체 CTest.

H4까지 통과하면 #2의 로컬 design completion criteria를 충족한다. 실제 NetworkRuntime·WorldRuntime 연결, timeout·late/duplicate completion, client stub과 World drain 정책은 이 완료 판정에 포함하지 않는다. 다음 구현 진입점은 `$lean-implementation` Guide mode의 E1이다.
