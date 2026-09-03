#include <array>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <pstk/service_host/TkServiceHost.h>

namespace
{

struct Request final
{
    uint32_t value = 0U;
};

struct alignas(64) AlignedRequest final
{
    uint32_t value = 0U;
};

static_assert(alignof(AlignedRequest) == 64U, "aligned request test requires 64-byte alignment");

struct PipelineState final
{
    std::atomic<int> decodeCalls{0};
    std::atomic<int> destroyCalls{0};
    std::atomic<int> submitCalls{0};
    std::atomic<int> handlerCalls{0};
    std::atomic<int> middlewareCalls{0};
    std::atomic<int> executorDestroyedJobs{0};
    std::atomic<int> processFailures{0};

    TkResult decodeResult = TK_SUCCESS;
    bool destroyOnDecodeFailure = false;
    bool emitDecodeDiagnostic = false;
    TkResult submitResult = TK_SUCCESS;
    TkResult handlerResult = TK_SUCCESS;

    std::atomic<const void *> decodeStorage{nullptr};
    std::atomic<void *> observedDecodeDiagnosticUserData{nullptr};
    const void *handlerStorage = nullptr;
    const void *alignedHandlerStorage = nullptr;
    TkHostConnectionKey handlerConnectionKey{};
    uint32_t handlerValue = 0U;
    bool alignedStorageCorrect = false;

    std::mutex mutex;
    std::vector<TkServiceJob *> acceptedJobs;
    std::vector<int> middlewareOrder;
    bool middlewareMetadataValid = true;

    std::condition_variable middlewareCondition;
    int middlewareEntered = 0;
    int middlewareTarget = 0;
    bool releaseMiddleware = false;
};

std::atomic<PipelineState *> currentDecodeState{nullptr};

TkResult NoOpOutput(const TkHostOutputInfo *, void *)
{
    return TK_SUCCESS;
}

void DestroyRequest(void *const requestStorage)
{
    PipelineState *const state = currentDecodeState.load();
    if (state != nullptr)
    {
        state->destroyCalls.fetch_add(1);
    }

    static_cast<Request *>(requestStorage)->~Request();
}

TkResult DecodeRequest(const TkByteView payload, void *const requestStorage, const TkDiagnosticCallbackInfo diagnostic)
{
    PipelineState *const state = currentDecodeState.load();
    if (state == nullptr)
    {
        return TK_ERROR_UNKNOWN;
    }

    state->decodeCalls.fetch_add(1);
    state->decodeStorage.store(requestStorage);
    state->observedDecodeDiagnosticUserData.store(diagnostic.userData);
    if (state->decodeResult != TK_SUCCESS)
    {
        if (state->emitDecodeDiagnostic)
        {
            const TkDiagnostic codecDiagnostic = {
                TK_DIAGNOSTIC_ERROR,
                "PSTK-TEST-DECODE-FAILED",
                "test decode failed",
                {nullptr, 0U, 0U, 0U},
            };
            TkEmitDiagnostic(diagnostic, &codecDiagnostic);
        }

        if (state->destroyOnDecodeFailure)
        {
            new (requestStorage) Request();
            DestroyRequest(requestStorage);
        }

        return state->decodeResult;
    }

    Request *const request = new (requestStorage) Request();
    std::memcpy(&request->value, payload.data, sizeof(request->value));
    return TK_SUCCESS;
}

void DestroyAlignedRequest(void *const requestStorage)
{
    PipelineState *const state = currentDecodeState.load();
    if (state != nullptr)
    {
        state->destroyCalls.fetch_add(1);
    }

    static_cast<AlignedRequest *>(requestStorage)->~AlignedRequest();
}

TkResult DecodeAlignedRequest(const TkByteView payload, void *const requestStorage, TkDiagnosticCallbackInfo)
{
    PipelineState *const state = currentDecodeState.load();
    if (state != nullptr)
    {
        state->decodeStorage.store(requestStorage);
    }

    AlignedRequest *const request = new (requestStorage) AlignedRequest();
    std::memcpy(&request->value, payload.data, sizeof(request->value));
    return TK_SUCCESS;
}

TkResult InvokeHandler(void *const serviceInstance, const TkServiceContext *const context,
                       const void *const requestStorage)
{
    PipelineState &state = *static_cast<PipelineState *>(serviceInstance);
    state.handlerCalls.fetch_add(1);
    state.handlerStorage = requestStorage;
    state.handlerConnectionKey = context->connectionKey;
    state.handlerValue = static_cast<const Request *>(requestStorage)->value;
    return state.handlerResult;
}

TkResult InvokeAlignedHandler(void *const serviceInstance, const TkServiceContext *, const void *const requestStorage)
{
    PipelineState &state = *static_cast<PipelineState *>(serviceInstance);
    state.alignedHandlerStorage = requestStorage;
    state.alignedStorageCorrect = reinterpret_cast<std::uintptr_t>(requestStorage) % alignof(AlignedRequest) == 0U;
    state.handlerCalls.fetch_add(1);
    return TK_SUCCESS;
}

TkResult CaptureExecutor(TkServiceJob *const job, void *const userData)
{
    PipelineState &state = *static_cast<PipelineState *>(userData);
    state.submitCalls.fetch_add(1);
    if (state.submitResult == TK_SUCCESS)
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.acceptedJobs.push_back(job);
    }

    return state.submitResult;
}

TkResult DestroyingExecutor(TkServiceJob *const job, void *const userData)
{
    PipelineState &state = *static_cast<PipelineState *>(userData);
    state.submitCalls.fetch_add(1);
    TkServiceJobDestroy(job);
    state.executorDestroyedJobs.fetch_add(1);
    return TK_SUCCESS;
}

TkResult ExecuteAndDestroyExecutor(TkServiceJob *const job, void *const userData)
{
    PipelineState &state = *static_cast<PipelineState *>(userData);
    state.submitCalls.fetch_add(1);
    const TkResult executeResult = TkServiceJobExecute(job, {nullptr, nullptr});
    TkServiceJobDestroy(job);
    state.executorDestroyedJobs.fetch_add(1);
    return executeResult;
}

TkResult RecordMiddleware(const TkServiceMiddlewareCallInfo *const callInfo, TkDiagnosticCallbackInfo,
                          void *const userData)
{
    PipelineState &state = *static_cast<PipelineState *>(userData);
    state.middlewareCalls.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.middlewareOrder.push_back(1);
        if (callInfo->packetId != 7U || callInfo->connectionKey.connectionId != 42U ||
            callInfo->connectionKey.generation != 3U)
        {
            state.middlewareMetadataValid = false;
        }
    }
    return TK_SUCCESS;
}

TkResult RejectMiddleware(const TkServiceMiddlewareCallInfo *, TkDiagnosticCallbackInfo, void *const userData)
{
    PipelineState &state = *static_cast<PipelineState *>(userData);
    state.middlewareCalls.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.middlewareOrder.push_back(2);
    }
    return TK_ERROR_REJECTED;
}

TkResult ConcurrentMiddleware(const TkServiceMiddlewareCallInfo *const callInfo, TkDiagnosticCallbackInfo,
                              void *const userData)
{
    PipelineState &state = *static_cast<PipelineState *>(userData);
    if (callInfo->packetId != 7U || callInfo->connectionKey.generation != 3U)
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.middlewareMetadataValid = false;
    }

    state.middlewareCalls.fetch_add(1);
    std::unique_lock<std::mutex> lock(state.mutex);
    ++state.middlewareEntered;
    if (state.middlewareEntered == state.middlewareTarget)
    {
        state.middlewareCondition.notify_all();
    }
    state.middlewareCondition.wait(lock, [&state]() { return state.releaseMiddleware; });
    return TK_SUCCESS;
}

TkServiceBindingInfo ValidBinding()
{
    TkServiceBindingInfo binding{};
    binding.type = TK_BINDING_TYPE_ONE_WAY;
    binding.request.packetId = 7U;
    binding.request.payloadBytes = sizeof(uint32_t);
    binding.request.requestSize = sizeof(Request);
    binding.request.requestAlignment = alignof(Request);
    binding.request.decodeRequest = DecodeRequest;
    binding.request.destroyRequest = DestroyRequest;
    binding.operation.oneWay.invokeHandler = InvokeHandler;
    return binding;
}

TkServiceBindingInfo ValidAlignedBinding()
{
    TkServiceBindingInfo binding = ValidBinding();
    binding.request.requestSize = sizeof(AlignedRequest);
    binding.request.requestAlignment = alignof(AlignedRequest);
    binding.request.decodeRequest = DecodeAlignedRequest;
    binding.request.destroyRequest = DestroyAlignedRequest;
    binding.operation.oneWay.invokeHandler = InvokeAlignedHandler;
    return binding;
}

TkServiceHost *CreateReadyHost(PipelineState *const state, TkServiceBindingInfo *const binding,
                               const TkServiceMiddlewareInfo *const middlewares = nullptr,
                               const std::size_t middlewareCount = 0U,
                               TkServiceJobSubmitCallback executor = CaptureExecutor)
{
    const TkServiceHostCreateInfo createInfo{{NoOpOutput, nullptr}};
    TkServiceHost *host = nullptr;
    if (TkServiceHostCreate(&createInfo, &host) != TK_SUCCESS)
    {
        return nullptr;
    }

    TkServiceRegistrationInfo registration{};
    registration.serviceInstance = state;
    registration.executor = {executor, state};
    registration.middlewares = middlewares;
    registration.middlewareCount = middlewareCount;
    registration.bindings = binding;
    registration.bindingCount = 1U;

    if (TkServiceHostRegisterService(host, &registration, {nullptr, nullptr}) != TK_SUCCESS ||
        TkServiceHostFinalizeRegistration(host, {nullptr, nullptr}) != TK_SUCCESS)
    {
        TkServiceHostDestroy(host);
        return nullptr;
    }

    return host;
}

TkResult ProcessPacket(TkServiceHost *const host, const uint8_t *const bytes, const std::size_t size,
                       TkDiagnosticCallbackInfo diagnostic = {nullptr, nullptr})
{
    return TkServiceHostProcessPacket(host, {42U, 3U}, 7U, {bytes, size}, diagnostic);
}

struct DiagnosticCapture final
{
    int count = 0;
    std::string id;
    std::string message;
};

void CaptureDiagnostic(const TkDiagnostic *const diagnostic, void *const userData)
{
    DiagnosticCapture &capture = *static_cast<DiagnosticCapture *>(userData);
    ++capture.count;
    capture.id = diagnostic->id;
    capture.message = diagnostic->message;
}

class ServiceHostPipelineTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        currentDecodeState.store(&state);
    }

    void TearDown() override
    {
        currentDecodeState.store(nullptr);
    }

    PipelineState state;
};

} // namespace

TEST_F(ServiceHostPipelineTest, ValidatesProcessInputsAndLifecycleBeforeLookup)
{
    TkServiceBindingInfo binding = ValidBinding();
    TkServiceHostCreateInfo createInfo{{NoOpOutput, nullptr}};
    TkServiceHost *host = nullptr;
    ASSERT_EQ(TkServiceHostCreate(&createInfo, &host), TK_SUCCESS);

    const std::array<uint8_t, sizeof(uint32_t)> payload{1U, 2U, 3U, 4U};
    EXPECT_EQ(TkServiceHostProcessPacket(host, {42U, 3U}, 7U, {nullptr, 1U}, {nullptr, nullptr}),
              TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(TkServiceHostProcessPacket(host, {42U, 0U}, 7U, {payload.data(), payload.size()}, {nullptr, nullptr}),
              TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(TkServiceHostProcessPacket(host, {42U, 3U}, 7U, {payload.data(), payload.size()}, {nullptr, nullptr}),
              TK_ERROR_INVALID_STATE);

    TkServiceRegistrationInfo registration{};
    registration.serviceInstance = &state;
    registration.executor = {CaptureExecutor, &state};
    registration.bindings = &binding;
    registration.bindingCount = 1U;
    ASSERT_EQ(TkServiceHostRegisterService(host, &registration, {nullptr, nullptr}), TK_SUCCESS);
    ASSERT_EQ(TkServiceHostFinalizeRegistration(host, {nullptr, nullptr}), TK_SUCCESS);

    EXPECT_EQ(TkServiceHostProcessPacket(host, {42U, 0U}, 7U, {nullptr, 1U}, {nullptr, nullptr}),
              TK_ERROR_INVALID_ARGUMENT);
    TkServiceHostDestroy(host);
}

TEST_F(ServiceHostPipelineTest, ReportsUnknownPacketAndExactPayloadSizeSynchronously)
{
    TkServiceBindingInfo binding = ValidBinding();
    TkServiceHost *const host = CreateReadyHost(&state, &binding);
    ASSERT_NE(host, nullptr);

    DiagnosticCapture capture;
    const TkDiagnosticCallbackInfo diagnostic{CaptureDiagnostic, &capture};
    const std::array<uint8_t, sizeof(uint32_t)> payload{1U, 2U, 3U, 4U};

    EXPECT_EQ(TkServiceHostProcessPacket(host, {42U, 3U}, 99U, {payload.data(), payload.size()}, diagnostic),
              TK_ERROR_INVALID_DATA);
    EXPECT_EQ(capture.count, 1);
    EXPECT_EQ(capture.id, "PSTK-SERVICE-HOST-UNKNOWN-PACKET");
    EXPECT_NE(capture.message.find("99"), std::string::npos);

    capture = {};
    EXPECT_EQ(ProcessPacket(host, payload.data(), payload.size() - 1U, diagnostic), TK_ERROR_INVALID_DATA);
    EXPECT_EQ(capture.count, 1);
    EXPECT_EQ(capture.id, "PSTK-SERVICE-HOST-INVALID-PAYLOAD-SIZE");
    EXPECT_NE(capture.message.find("4"), std::string::npos);

    TkServiceHostDestroy(host);
}

TEST_F(ServiceHostPipelineTest, DecodeFailureUsesCodecCleanupWithoutDoubleDestroy)
{
    state.decodeResult = TK_ERROR_INVALID_DATA;
    state.destroyOnDecodeFailure = true;
    state.emitDecodeDiagnostic = true;
    TkServiceBindingInfo binding = ValidBinding();
    TkServiceHost *const host = CreateReadyHost(&state, &binding);
    ASSERT_NE(host, nullptr);

    const std::array<uint8_t, sizeof(uint32_t)> payload{1U, 2U, 3U, 4U};
    DiagnosticCapture capture;
    const TkDiagnosticCallbackInfo diagnostic{CaptureDiagnostic, &capture};
    EXPECT_EQ(ProcessPacket(host, payload.data(), payload.size(), diagnostic), TK_ERROR_INVALID_DATA);
    EXPECT_EQ(state.decodeCalls.load(), 1);
    EXPECT_EQ(state.destroyCalls.load(), 1);
    EXPECT_EQ(state.submitCalls.load(), 0);
    EXPECT_EQ(state.observedDecodeDiagnosticUserData.load(), &capture);
    EXPECT_EQ(capture.count, 1);
    EXPECT_EQ(capture.id, "PSTK-TEST-DECODE-FAILED");
    EXPECT_EQ(capture.message, "test decode failed");

    TkServiceHostDestroy(host);
}

TEST_F(ServiceHostPipelineTest, RunsMiddlewareInOrderAndStopsBeforeExecutor)
{
    const TkServiceMiddlewareInfo middlewares[] = {
        {RecordMiddleware, &state},
        {RejectMiddleware, &state},
    };
    TkServiceBindingInfo binding = ValidBinding();
    TkServiceHost *const host = CreateReadyHost(&state, &binding, middlewares, 2U);
    ASSERT_NE(host, nullptr);

    const std::array<uint8_t, sizeof(uint32_t)> payload{1U, 2U, 3U, 4U};
    EXPECT_EQ(ProcessPacket(host, payload.data(), payload.size()), TK_ERROR_REJECTED);
    EXPECT_EQ(state.submitCalls.load(), 0);
    EXPECT_EQ(state.destroyCalls.load(), 1);
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        ASSERT_EQ(state.middlewareOrder, (std::vector<int>{1, 2}));
        EXPECT_TRUE(state.middlewareMetadataValid);
    }

    TkServiceHostDestroy(host);
}

TEST_F(ServiceHostPipelineTest, AllowsConcurrentMetadataOnlyMiddlewareCalls)
{
    constexpr int processCount = 8;
    state.middlewareTarget = processCount;
    const TkServiceMiddlewareInfo middleware{ConcurrentMiddleware, &state};
    TkServiceBindingInfo binding = ValidBinding();
    TkServiceHost *const host = CreateReadyHost(&state, &binding, &middleware, 1U, DestroyingExecutor);
    ASSERT_NE(host, nullptr);

    const std::array<uint8_t, sizeof(uint32_t)> payload{1U, 2U, 3U, 4U};
    PipelineState &pipelineState = state;
    std::vector<std::thread> workers;
    workers.reserve(processCount);
    for (int index = 0; index < processCount; ++index)
    {
        workers.emplace_back([host, &payload]() {
            PipelineState *const state = currentDecodeState.load();
            if (ProcessPacket(host, payload.data(), payload.size()) != TK_SUCCESS && state != nullptr)
            {
                state->processFailures.fetch_add(1);
            }
        });
    }

    {
        std::unique_lock<std::mutex> lock(state.mutex);
        state.middlewareCondition.wait(
            lock, [&pipelineState]() { return pipelineState.middlewareEntered == pipelineState.middlewareTarget; });
        state.releaseMiddleware = true;
    }
    state.middlewareCondition.notify_all();

    for (std::thread &worker : workers)
    {
        worker.join();
    }

    EXPECT_EQ(state.middlewareCalls.load(), processCount);
    EXPECT_EQ(state.submitCalls.load(), processCount);
    EXPECT_EQ(state.executorDestroyedJobs.load(), processCount);
    EXPECT_EQ(state.destroyCalls.load(), processCount);
    EXPECT_EQ(state.processFailures.load(), 0);
    EXPECT_TRUE(state.middlewareMetadataValid);
    TkServiceHostDestroy(host);
}

TEST_F(ServiceHostPipelineTest, ExecutorFailureRetainsNoJobAndHostCleansItUp)
{
    state.submitResult = TK_ERROR_CAPACITY_EXCEEDED;
    TkServiceBindingInfo binding = ValidBinding();
    TkServiceHost *const host = CreateReadyHost(&state, &binding);
    ASSERT_NE(host, nullptr);

    const std::array<uint8_t, sizeof(uint32_t)> payload{1U, 2U, 3U, 4U};
    DiagnosticCapture capture;
    const TkDiagnosticCallbackInfo diagnostic{CaptureDiagnostic, &capture};
    EXPECT_EQ(ProcessPacket(host, payload.data(), payload.size(), diagnostic), TK_ERROR_CAPACITY_EXCEEDED);
    EXPECT_EQ(state.submitCalls.load(), 1);
    EXPECT_EQ(state.destroyCalls.load(), 1);
    EXPECT_EQ(capture.count, 1);
    EXPECT_EQ(capture.id, "PSTK-SERVICE-HOST-EXECUTOR-SUBMIT-FAILED");
    EXPECT_NE(capture.message.find("-8"), std::string::npos);
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        EXPECT_TRUE(state.acceptedJobs.empty());
    }

    TkServiceHostDestroy(host);
}

TEST_F(ServiceHostPipelineTest, HandlerFailureEmitsOneDiagnosticAndMarksJobExecuted)
{
    state.handlerResult = TK_ERROR_REJECTED;
    TkServiceBindingInfo binding = ValidBinding();
    TkServiceHost *const host = CreateReadyHost(&state, &binding);
    ASSERT_NE(host, nullptr);

    const std::array<uint8_t, sizeof(uint32_t)> payload{1U, 2U, 3U, 4U};
    ASSERT_EQ(ProcessPacket(host, payload.data(), payload.size()), TK_SUCCESS);

    TkServiceJob *job = nullptr;
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        ASSERT_EQ(state.acceptedJobs.size(), 1U);
        job = state.acceptedJobs.front();
    }

    DiagnosticCapture capture;
    const TkDiagnosticCallbackInfo diagnostic{CaptureDiagnostic, &capture};
    EXPECT_EQ(TkServiceJobExecute(job, diagnostic), TK_ERROR_REJECTED);
    EXPECT_EQ(state.handlerCalls.load(), 1);
    EXPECT_EQ(capture.count, 1);
    EXPECT_EQ(capture.id, "PSTK-SERVICE-HOST-HANDLER-FAILED");
    EXPECT_NE(capture.message.find("-9"), std::string::npos);

    EXPECT_EQ(TkServiceJobExecute(job, diagnostic), TK_ERROR_INVALID_STATE);
    EXPECT_EQ(state.handlerCalls.load(), 1);
    EXPECT_EQ(capture.count, 1);

    TkServiceJobDestroy(job);
    EXPECT_EQ(state.destroyCalls.load(), 1);
    TkServiceHostDestroy(host);
}

TEST_F(ServiceHostPipelineTest, ExecutorMayExecuteAndDestroyBeforeSubmitReturns)
{
    TkServiceBindingInfo binding = ValidBinding();
    TkServiceHost *const host = CreateReadyHost(&state, &binding, nullptr, 0U, ExecuteAndDestroyExecutor);
    ASSERT_NE(host, nullptr);

    const std::array<uint8_t, sizeof(uint32_t)> payload{1U, 2U, 3U, 4U};
    EXPECT_EQ(ProcessPacket(host, payload.data(), payload.size()), TK_SUCCESS);
    EXPECT_EQ(state.submitCalls.load(), 1);
    EXPECT_EQ(state.handlerCalls.load(), 1);
    EXPECT_EQ(state.executorDestroyedJobs.load(), 1);
    EXPECT_EQ(state.destroyCalls.load(), 1);

    TkServiceHostDestroy(host);
}

TEST_F(ServiceHostPipelineTest, NullJobOperationsFollowNoOpAndInvalidArgumentContracts)
{
    EXPECT_EQ(TkServiceJobExecute(nullptr, {nullptr, nullptr}), TK_ERROR_INVALID_ARGUMENT);
    TkServiceJobDestroy(nullptr);
    SUCCEED();
}

TEST_F(ServiceHostPipelineTest, AcceptedJobExecutesOnlyWhenRequestedAndOnlyOnce)
{
    TkServiceBindingInfo binding = ValidBinding();
    TkServiceHost *const host = CreateReadyHost(&state, &binding);
    ASSERT_NE(host, nullptr);

    const void *rawAddress = nullptr;
    {
        std::array<uint8_t, sizeof(uint32_t)> payload{1U, 2U, 3U, 4U};
        rawAddress = payload.data();
        EXPECT_EQ(ProcessPacket(host, payload.data(), payload.size()), TK_SUCCESS);
    }

    TkServiceJob *job = nullptr;
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        ASSERT_EQ(state.acceptedJobs.size(), 1U);
        job = state.acceptedJobs.front();
    }
    ASSERT_NE(job, nullptr);
    EXPECT_EQ(state.handlerCalls.load(), 0);
    EXPECT_EQ(TkServiceJobExecute(job, {nullptr, nullptr}), TK_SUCCESS);
    EXPECT_EQ(state.handlerCalls.load(), 1);
    EXPECT_NE(state.handlerStorage, rawAddress);
    EXPECT_EQ(state.handlerStorage, state.decodeStorage.load());
    EXPECT_EQ(state.handlerValue, 0x04030201U);
    EXPECT_EQ(state.handlerConnectionKey.connectionId, 42U);
    EXPECT_EQ(state.handlerConnectionKey.generation, 3U);
    EXPECT_EQ(TkServiceJobExecute(job, {nullptr, nullptr}), TK_ERROR_INVALID_STATE);
    EXPECT_EQ(state.handlerCalls.load(), 1);

    TkServiceJobDestroy(job);
    EXPECT_EQ(state.destroyCalls.load(), 1);
    TkServiceHostDestroy(host);
}

TEST_F(ServiceHostPipelineTest, AcceptedJobsRemainUsableAfterHostDestruction)
{
    TkServiceBindingInfo binding = ValidBinding();
    TkServiceHost *const host = CreateReadyHost(&state, &binding);
    ASSERT_NE(host, nullptr);

    const std::array<uint8_t, sizeof(uint32_t)> payload{1U, 2U, 3U, 4U};
    ASSERT_EQ(ProcessPacket(host, payload.data(), payload.size()), TK_SUCCESS);
    ASSERT_EQ(ProcessPacket(host, payload.data(), payload.size()), TK_SUCCESS);

    TkServiceJob *executeJob = nullptr;
    TkServiceJob *discardJob = nullptr;
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        ASSERT_EQ(state.acceptedJobs.size(), 2U);
        executeJob = state.acceptedJobs[0];
        discardJob = state.acceptedJobs[1];
    }

    TkServiceHostDestroy(host);
    EXPECT_EQ(TkServiceJobExecute(executeJob, {nullptr, nullptr}), TK_SUCCESS);
    TkServiceJobDestroy(executeJob);
    TkServiceJobDestroy(discardJob);
    EXPECT_EQ(state.handlerCalls.load(), 1);
    EXPECT_EQ(state.destroyCalls.load(), 2);
}

TEST_F(ServiceHostPipelineTest, RequestStorageHonorsRegisteredOverAlignment)
{
    TkServiceBindingInfo binding = ValidAlignedBinding();
    TkServiceHost *const host = CreateReadyHost(&state, &binding);
    ASSERT_NE(host, nullptr);

    const void *rawAddress = nullptr;
    {
        std::array<uint8_t, sizeof(uint32_t)> rawPayload{1U, 2U, 3U, 4U};
        rawAddress = rawPayload.data();
        EXPECT_EQ(ProcessPacket(host, rawPayload.data(), rawPayload.size()), TK_SUCCESS);
    }

    TkServiceJob *job = nullptr;
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        ASSERT_EQ(state.acceptedJobs.size(), 1U);
        job = state.acceptedJobs.front();
    }
    ASSERT_NE(job, nullptr);
    EXPECT_EQ(TkServiceJobExecute(job, {nullptr, nullptr}), TK_SUCCESS);
    EXPECT_NE(state.alignedHandlerStorage, rawAddress);
    EXPECT_EQ(state.alignedHandlerStorage, state.decodeStorage.load());
    EXPECT_TRUE(state.alignedStorageCorrect);
    EXPECT_EQ(state.handlerCalls.load(), 1);

    TkServiceJobDestroy(job);
    EXPECT_EQ(state.destroyCalls.load(), 1);
    TkServiceHostDestroy(host);
}

TEST_F(ServiceHostPipelineTest, DiagnosticPointersAreUsedOnlyDuringSynchronousCall)
{
    TkServiceBindingInfo binding = ValidBinding();
    TkServiceHost *const host = CreateReadyHost(&state, &binding);
    ASSERT_NE(host, nullptr);

    DiagnosticCapture capture;
    const TkDiagnosticCallbackInfo diagnostic{CaptureDiagnostic, &capture};
    const std::array<uint8_t, sizeof(uint32_t)> payload{1U, 2U, 3U, 4U};
    EXPECT_EQ(TkServiceHostProcessPacket(host, {42U, 3U}, 99U, {payload.data(), payload.size()}, diagnostic),
              TK_ERROR_INVALID_DATA);
    const std::string idAfterCall = capture.id;
    const std::string messageAfterCall = capture.message;
    EXPECT_EQ(capture.count, 1);
    EXPECT_EQ(idAfterCall, "PSTK-SERVICE-HOST-UNKNOWN-PACKET");
    EXPECT_NE(messageAfterCall.find("99"), std::string::npos);

    TkServiceHostDestroy(host);
    EXPECT_EQ(capture.count, 1);
}
