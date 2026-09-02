#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <pstk/service_host/TkServiceHost.h>

namespace
{

struct Request final
{
    uint32_t value = 0U;
};

struct Response final
{
    uint32_t value = 0U;
};

struct alignas(128) AlignedResponse final
{
    uint32_t value = 0U;
};

static_assert(alignof(AlignedResponse) == 128U, "aligned response test requires 128-byte alignment");

struct DiagnosticCapture final
{
    int count = 0;
    std::string id;
    std::string message;
};

struct State final
{
    std::vector<int> order;
    int requestDestroyCalls = 0;
    int responseDestroyCalls = 0;
    int handlerCalls = 0;
    int encodeCalls = 0;
    int outputCalls = 0;
    TkResult handlerResult = TK_SUCCESS;
    TkResult encodeResult = TK_SUCCESS;
    TkResult outputResult = TK_SUCCESS;
    bool constructResponseOnHandlerFailure = true;
    bool responseIsLive = false;
    TkServiceJob *job = nullptr;
    const void *handlerRequestStorage = nullptr;
    void *handlerResponseStorage = nullptr;
    const void *encodeResponseStorage = nullptr;
    void *destroyResponseStorage = nullptr;
    bool responseStorageAligned = false;
    TkHostOutputInfo outputInfo{};
    std::vector<uint8_t> outputBytes;
};

State *currentState = nullptr;

void CaptureDiagnostic(const TkDiagnostic *const diagnostic, void *const userData)
{
    DiagnosticCapture &capture = *static_cast<DiagnosticCapture *>(userData);
    ++capture.count;
    capture.id = diagnostic->id;
    capture.message = diagnostic->message;
}

TkResult Output(const TkHostOutputInfo *const outputInfo, void *const userData)
{
    State &state = *static_cast<State *>(userData);
    ++state.outputCalls;
    state.order.push_back(4);
    state.outputInfo = *outputInfo;
    state.outputBytes.assign(outputInfo->payload.data, outputInfo->payload.data + outputInfo->payload.size);
    return state.outputResult;
}

TkResult Decode(const TkByteView payload, void *const requestStorage, TkDiagnosticCallbackInfo)
{
    Request *const request = new (requestStorage) Request();
    std::memcpy(&request->value, payload.data, sizeof(request->value));
    return TK_SUCCESS;
}

void DestroyResponse(void *responseStorage);

void DestroyRequest(void *const requestStorage)
{
    ++currentState->requestDestroyCalls;
    static_cast<Request *>(requestStorage)->~Request();
}

TkResult Handle(void *const serviceInstance, const TkServiceContext *const context, const void *const requestStorage,
                void *const responseStorage)
{
    State &state = *static_cast<State *>(serviceInstance);
    ++state.handlerCalls;
    state.order.push_back(1);
    state.handlerRequestStorage = requestStorage;
    state.handlerResponseStorage = responseStorage;
    state.responseStorageAligned = reinterpret_cast<std::uintptr_t>(responseStorage) % alignof(Response) == 0U;

    if (state.handlerResult != TK_SUCCESS)
    {
        if (state.constructResponseOnHandlerFailure)
        {
            new (responseStorage) Response();
            state.responseIsLive = true;
            DestroyResponse(responseStorage);
        }
        return state.handlerResult;
    }

    const Request &request = *static_cast<const Request *>(requestStorage);
    Response *const response = new (responseStorage) Response();
    response->value = request.value + 1U;
    state.responseIsLive = true;
    return TK_SUCCESS;
}

TkResult HandleAligned(void *const serviceInstance, const TkServiceContext *, const void *const requestStorage,
                       void *const responseStorage)
{
    State &state = *static_cast<State *>(serviceInstance);
    ++state.handlerCalls;
    state.order.push_back(1);
    state.handlerRequestStorage = requestStorage;
    state.handlerResponseStorage = responseStorage;
    state.responseStorageAligned = reinterpret_cast<std::uintptr_t>(responseStorage) % alignof(AlignedResponse) == 0U;
    AlignedResponse *const response = new (responseStorage) AlignedResponse();
    response->value = static_cast<const Request *>(requestStorage)->value + 1U;
    state.responseIsLive = true;
    return TK_SUCCESS;
}

TkResult Encode(const void *const responseStorage, const TkMutableByteView output,
                const TkDiagnosticCallbackInfo diagnostic)
{
    State &state = *currentState;
    ++state.encodeCalls;
    state.order.push_back(2);
    state.encodeResponseStorage = responseStorage;
    EXPECT_EQ(output.size, sizeof(uint32_t));
    if (state.encodeResult != TK_SUCCESS)
    {
        const TkDiagnostic codecDiagnostic = {
            TK_DIAGNOSTIC_ERROR,
            "PSTK-TEST-ENCODE-FAILED",
            "test encode failed",
            {nullptr, 0U, 0U, 0U},
        };
        TkEmitDiagnostic(diagnostic, &codecDiagnostic);
        return state.encodeResult;
    }

    const Response &response = *static_cast<const Response *>(responseStorage);
    std::memcpy(output.data, &response.value, sizeof(response.value));
    return TK_SUCCESS;
}

TkResult EncodeAligned(const void *const responseStorage, const TkMutableByteView output, TkDiagnosticCallbackInfo)
{
    State &state = *currentState;
    ++state.encodeCalls;
    state.order.push_back(2);
    state.encodeResponseStorage = responseStorage;
    EXPECT_EQ(output.size, sizeof(uint32_t));
    const AlignedResponse &response = *static_cast<const AlignedResponse *>(responseStorage);
    std::memcpy(output.data, &response.value, sizeof(response.value));
    return TK_SUCCESS;
}

void DestroyResponse(void *const responseStorage)
{
    State &state = *currentState;
    ++state.responseDestroyCalls;
    state.order.push_back(3);
    state.destroyResponseStorage = responseStorage;
    state.responseIsLive = false;
    static_cast<Response *>(responseStorage)->~Response();
}

void DestroyAlignedResponse(void *const responseStorage)
{
    State &state = *currentState;
    ++state.responseDestroyCalls;
    state.order.push_back(3);
    state.destroyResponseStorage = responseStorage;
    state.responseIsLive = false;
    static_cast<AlignedResponse *>(responseStorage)->~AlignedResponse();
}

TkResult Submit(TkServiceJob *const job, void *const userData)
{
    State &state = *static_cast<State *>(userData);
    state.job = job;
    return TK_SUCCESS;
}

TkServiceBindingInfo ResponseBinding(const uint16_t requestPacketId = 11U, const uint16_t responsePacketId = 22U)
{
    TkServiceBindingInfo binding{};
    binding.type = TK_BINDING_TYPE_REQUEST_RESPONSE;
    binding.request.packetId = requestPacketId;
    binding.request.payloadBytes = sizeof(uint32_t);
    binding.request.requestSize = sizeof(Request);
    binding.request.requestAlignment = alignof(Request);
    binding.request.decodeRequest = Decode;
    binding.request.destroyRequest = DestroyRequest;
    binding.operation.requestResponse.packetId = responsePacketId;
    binding.operation.requestResponse.payloadBytes = sizeof(uint32_t);
    binding.operation.requestResponse.responseSize = sizeof(Response);
    binding.operation.requestResponse.responseAlignment = alignof(Response);
    binding.operation.requestResponse.invokeHandler = Handle;
    binding.operation.requestResponse.encodeResponse = Encode;
    binding.operation.requestResponse.destroyResponse = DestroyResponse;
    return binding;
}

TkServiceBindingInfo AlignedResponseBinding()
{
    TkServiceBindingInfo binding = ResponseBinding();
    binding.operation.requestResponse.responseSize = sizeof(AlignedResponse);
    binding.operation.requestResponse.responseAlignment = alignof(AlignedResponse);
    binding.operation.requestResponse.invokeHandler = HandleAligned;
    binding.operation.requestResponse.encodeResponse = EncodeAligned;
    binding.operation.requestResponse.destroyResponse = DestroyAlignedResponse;
    return binding;
}

TkServiceHost *CreateReadyHost(State *const state, TkServiceBindingInfo *const binding)
{
    currentState = state;
    const TkServiceHostCreateInfo createInfo{{Output, state}};
    TkServiceHost *host = nullptr;
    if (TkServiceHostCreate(&createInfo, &host) != TK_SUCCESS)
    {
        return nullptr;
    }

    const TkServiceRegistrationInfo registration{state, {Submit, state}, nullptr, 0U, binding, 1U};
    if (TkServiceHostRegisterService(host, &registration, {nullptr, nullptr}) != TK_SUCCESS ||
        TkServiceHostFinalizeRegistration(host, {nullptr, nullptr}) != TK_SUCCESS)
    {
        TkServiceHostDestroy(host);
        return nullptr;
    }

    return host;
}

void ProcessResponsePacket(TkServiceHost *const host, State &state,
                           const TkDiagnosticCallbackInfo diagnostic = {nullptr, nullptr})
{
    const std::array<uint8_t, sizeof(uint32_t)> payload{7U, 0U, 0U, 0U};
    ASSERT_EQ(TkServiceHostProcessPacket(host, {42U, 3U}, 11U, {payload.data(), payload.size()}, diagnostic),
              TK_SUCCESS);
    ASSERT_NE(state.job, nullptr);
}

} // namespace

TEST(ServiceHostResponsePipeline, SuccessRunsHandlerEncodeAndOutputInOrder)
{
    State state;
    TkServiceBindingInfo binding = ResponseBinding();
    TkServiceHost *const host = CreateReadyHost(&state, &binding);
    ASSERT_NE(host, nullptr);

    ProcessResponsePacket(host, state);
    EXPECT_EQ(state.order, (std::vector<int>{}));
    EXPECT_EQ(TkServiceJobExecute(state.job, {nullptr, nullptr}), TK_SUCCESS);
    EXPECT_EQ(state.order, (std::vector<int>{1, 2, 3, 4}));
    EXPECT_EQ(state.handlerCalls, 1);
    EXPECT_EQ(state.encodeCalls, 1);
    EXPECT_EQ(state.responseDestroyCalls, 1);
    EXPECT_EQ(state.outputCalls, 1);
    EXPECT_FALSE(state.responseIsLive);
    EXPECT_EQ(state.outputInfo.connectionKey.connectionId, 42U);
    EXPECT_EQ(state.outputInfo.connectionKey.generation, 3U);
    EXPECT_EQ(state.outputInfo.packetId, 22U);
    EXPECT_EQ(state.outputBytes, (std::vector<uint8_t>{8U, 0U, 0U, 0U}));

    EXPECT_EQ(TkServiceJobExecute(state.job, {nullptr, nullptr}), TK_ERROR_INVALID_STATE);
    EXPECT_EQ(state.handlerCalls, 1);
    EXPECT_EQ(state.encodeCalls, 1);
    EXPECT_EQ(state.responseDestroyCalls, 1);
    EXPECT_EQ(state.outputCalls, 1);

    TkServiceJobDestroy(state.job);
    EXPECT_EQ(state.requestDestroyCalls, 1);
    TkServiceHostDestroy(host);
    currentState = nullptr;
}

TEST(ServiceHostResponsePipeline, HandlerFailureSelfCleansResponseAndEmitsHostDiagnostic)
{
    State state;
    state.handlerResult = TK_ERROR_REJECTED;
    TkServiceBindingInfo binding = ResponseBinding();
    TkServiceHost *const host = CreateReadyHost(&state, &binding);
    ASSERT_NE(host, nullptr);

    ProcessResponsePacket(host, state);
    DiagnosticCapture capture;
    EXPECT_EQ(TkServiceJobExecute(state.job, {CaptureDiagnostic, &capture}), TK_ERROR_REJECTED);
    EXPECT_EQ(state.handlerCalls, 1);
    EXPECT_EQ(state.encodeCalls, 0);
    EXPECT_EQ(state.outputCalls, 0);
    EXPECT_EQ(state.responseDestroyCalls, 1);
    EXPECT_FALSE(state.responseIsLive);
    EXPECT_EQ(capture.count, 1);
    EXPECT_EQ(capture.id, "PSTK-SERVICE-HOST-HANDLER-FAILED");
    EXPECT_EQ(TkServiceJobExecute(state.job, {CaptureDiagnostic, &capture}), TK_ERROR_INVALID_STATE);

    TkServiceJobDestroy(state.job);
    EXPECT_EQ(state.requestDestroyCalls, 1);
    EXPECT_EQ(state.responseDestroyCalls, 1);
    TkServiceHostDestroy(host);
    currentState = nullptr;
}

TEST(ServiceHostResponsePipeline, EncodeFailureUsesCodecDiagnosticAndDestroysResponseOnce)
{
    State state;
    state.encodeResult = TK_ERROR_INVALID_DATA;
    TkServiceBindingInfo binding = ResponseBinding();
    TkServiceHost *const host = CreateReadyHost(&state, &binding);
    ASSERT_NE(host, nullptr);

    ProcessResponsePacket(host, state);
    DiagnosticCapture capture;
    EXPECT_EQ(TkServiceJobExecute(state.job, {CaptureDiagnostic, &capture}), TK_ERROR_INVALID_DATA);
    EXPECT_EQ(state.handlerCalls, 1);
    EXPECT_EQ(state.encodeCalls, 1);
    EXPECT_EQ(state.responseDestroyCalls, 1);
    EXPECT_EQ(state.outputCalls, 0);
    EXPECT_FALSE(state.responseIsLive);
    EXPECT_EQ(capture.count, 1);
    EXPECT_EQ(capture.id, "PSTK-TEST-ENCODE-FAILED");

    TkServiceJobDestroy(state.job);
    EXPECT_EQ(state.requestDestroyCalls, 1);
    EXPECT_EQ(state.responseDestroyCalls, 1);
    TkServiceHostDestroy(host);
    currentState = nullptr;
}

TEST(ServiceHostResponsePipeline, OutputFailureEmitsOneHostDiagnosticAndReturnsExactResult)
{
    State state;
    state.outputResult = TK_ERROR_IO;
    TkServiceBindingInfo binding = ResponseBinding();
    TkServiceHost *const host = CreateReadyHost(&state, &binding);
    ASSERT_NE(host, nullptr);

    ProcessResponsePacket(host, state);
    DiagnosticCapture capture;
    EXPECT_EQ(TkServiceJobExecute(state.job, {CaptureDiagnostic, &capture}), TK_ERROR_IO);
    EXPECT_EQ(state.handlerCalls, 1);
    EXPECT_EQ(state.encodeCalls, 1);
    EXPECT_EQ(state.responseDestroyCalls, 1);
    EXPECT_EQ(state.outputCalls, 1);
    EXPECT_FALSE(state.responseIsLive);
    EXPECT_EQ(capture.count, 1);
    EXPECT_EQ(capture.id, "PSTK-SERVICE-HOST-OUTPUT-FAILED");
    EXPECT_EQ(state.outputBytes, (std::vector<uint8_t>{8U, 0U, 0U, 0U}));

    TkServiceJobDestroy(state.job);
    EXPECT_EQ(state.requestDestroyCalls, 1);
    EXPECT_EQ(state.responseDestroyCalls, 1);
    TkServiceHostDestroy(host);
    currentState = nullptr;
}

TEST(ServiceHostResponsePipeline, AcceptedJobSnapshotsOutputAdapterForExecutionAfterHostDestruction)
{
    State state;
    TkServiceBindingInfo binding = ResponseBinding();
    TkServiceHost *const host = CreateReadyHost(&state, &binding);
    ASSERT_NE(host, nullptr);

    ProcessResponsePacket(host, state);
    TkServiceHostDestroy(host);
    EXPECT_EQ(TkServiceJobExecute(state.job, {nullptr, nullptr}), TK_SUCCESS);
    EXPECT_EQ(state.outputCalls, 1);
    EXPECT_EQ(state.outputInfo.packetId, 22U);
    EXPECT_EQ(state.outputBytes, (std::vector<uint8_t>{8U, 0U, 0U, 0U}));

    TkServiceJobDestroy(state.job);
    EXPECT_EQ(state.requestDestroyCalls, 1);
    EXPECT_EQ(state.responseDestroyCalls, 1);
    currentState = nullptr;
}

TEST(ServiceHostResponsePipeline, DiscardBeforeExecuteDestroysRequestWithoutResponse)
{
    State state;
    TkServiceBindingInfo binding = ResponseBinding();
    TkServiceHost *const host = CreateReadyHost(&state, &binding);
    ASSERT_NE(host, nullptr);

    ProcessResponsePacket(host, state);
    TkServiceJobDestroy(state.job);
    EXPECT_EQ(state.requestDestroyCalls, 1);
    EXPECT_EQ(state.handlerCalls, 0);
    EXPECT_EQ(state.encodeCalls, 0);
    EXPECT_EQ(state.responseDestroyCalls, 0);
    EXPECT_EQ(state.outputCalls, 0);
    TkServiceHostDestroy(host);
    currentState = nullptr;
}

TEST(ServiceHostResponsePipeline, ResponseStorageHonorsRegisteredOverAlignment)
{
    State state;
    TkServiceBindingInfo binding = AlignedResponseBinding();
    TkServiceHost *const host = CreateReadyHost(&state, &binding);
    ASSERT_NE(host, nullptr);

    ProcessResponsePacket(host, state);
    ASSERT_EQ(TkServiceJobExecute(state.job, {nullptr, nullptr}), TK_SUCCESS);
    EXPECT_TRUE(state.responseStorageAligned);
    EXPECT_EQ(state.handlerResponseStorage, state.encodeResponseStorage);
    EXPECT_EQ(state.handlerResponseStorage, state.destroyResponseStorage);
    EXPECT_EQ(state.responseDestroyCalls, 1);
    EXPECT_EQ(state.outputCalls, 1);

    TkServiceJobDestroy(state.job);
    EXPECT_EQ(state.requestDestroyCalls, 1);
    EXPECT_EQ(state.responseDestroyCalls, 1);
    TkServiceHostDestroy(host);
    currentState = nullptr;
}

TEST(ServiceHostResponsePipeline, OneWayBindingRemainsResponseAndOutputFree)
{
    State state;
    TkServiceBindingInfo binding{};
    binding.type = TK_BINDING_TYPE_ONE_WAY;
    binding.request.packetId = 11U;
    binding.request.payloadBytes = sizeof(uint32_t);
    binding.request.requestSize = sizeof(Request);
    binding.request.requestAlignment = alignof(Request);
    binding.request.decodeRequest = Decode;
    binding.request.destroyRequest = DestroyRequest;
    binding.operation.oneWay.invokeHandler = [](void *const serviceInstance, const TkServiceContext *,
                                                const void *const requestStorage) noexcept {
        State &oneWayState = *static_cast<State *>(serviceInstance);
        ++oneWayState.handlerCalls;
        oneWayState.order.push_back(1);
        EXPECT_EQ(static_cast<const Request *>(requestStorage)->value, 7U);
        return TK_SUCCESS;
    };
    TkServiceHost *const host = CreateReadyHost(&state, &binding);
    ASSERT_NE(host, nullptr);

    ProcessResponsePacket(host, state);
    EXPECT_EQ(TkServiceJobExecute(state.job, {nullptr, nullptr}), TK_SUCCESS);
    EXPECT_EQ(state.handlerCalls, 1);
    EXPECT_EQ(state.encodeCalls, 0);
    EXPECT_EQ(state.responseDestroyCalls, 0);
    EXPECT_EQ(state.outputCalls, 0);

    TkServiceJobDestroy(state.job);
    EXPECT_EQ(state.requestDestroyCalls, 1);
    TkServiceHostDestroy(host);
    currentState = nullptr;
}
