#include <gtest/gtest.h>

#include <pstk/service_host/TkServiceBinding.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

namespace
{
struct Request final
{
    static constexpr std::uint16_t PacketId = 101U;
    static constexpr std::size_t PayloadBytes = 4U;

    std::uint32_t value = 0U;

    TkResult Decode(const TkByteView input, TkDiagnosticCallbackInfo) noexcept
    {
        std::memcpy(&value, input.data, sizeof(value));
        return TK_SUCCESS;
    }

    TkResult Encode(TkMutableByteView, TkDiagnosticCallbackInfo) const noexcept
    {
        return TK_SUCCESS;
    }
};

struct WrongPacketIdType final
{
    static constexpr std::uint32_t PacketId = 101U;
    static constexpr std::size_t PayloadBytes = 4U;

    TkResult Decode(TkByteView, TkDiagnosticCallbackInfo) noexcept
    {
        return TK_SUCCESS;
    }

    TkResult Encode(TkMutableByteView, TkDiagnosticCallbackInfo) const noexcept
    {
        return TK_SUCCESS;
    }
};

struct WrongPayloadBytesType final
{
    static constexpr std::uint16_t PacketId = 101U;
    static constexpr std::uint32_t PayloadBytes = 4U;

    TkResult Decode(TkByteView, TkDiagnosticCallbackInfo) noexcept
    {
        return TK_SUCCESS;
    }

    TkResult Encode(TkMutableByteView, TkDiagnosticCallbackInfo) const noexcept
    {
        return TK_SUCCESS;
    }
};

struct ResponseRequest final
{
    static constexpr std::uint16_t PacketId = 102U;
    static constexpr std::size_t PayloadBytes = 4U;

    std::uint32_t value = 0U;

    TkResult Decode(const TkByteView input, TkDiagnosticCallbackInfo) noexcept
    {
        std::memcpy(&value, input.data, sizeof(value));
        return TK_SUCCESS;
    }

    TkResult Encode(TkMutableByteView, TkDiagnosticCallbackInfo) const noexcept
    {
        return TK_SUCCESS;
    }
};

struct Response final
{
    static constexpr std::uint16_t PacketId = 201U;
    static constexpr std::size_t PayloadBytes = 4U;

    std::uint32_t value = 0U;

    inline static int destructorCalls = 0;

    ~Response()
    {
        ++destructorCalls;
    }

    TkResult Decode(TkByteView, TkDiagnosticCallbackInfo) noexcept
    {
        return TK_SUCCESS;
    }

    TkResult Encode(const TkMutableByteView output, TkDiagnosticCallbackInfo) const noexcept
    {
        std::memcpy(output.data, &value, sizeof(value));
        return TK_SUCCESS;
    }
};

struct Service final
{
    int oneWayCalls = 0;
    int responseCalls = 0;
    std::uint32_t oneWayValue = 0U;
    std::uint32_t responseValue = 0U;
    TkResult responseResult = TK_SUCCESS;

    TkResult OnRequest(const TkServiceContext &, const Request &request) noexcept
    {
        ++oneWayCalls;
        oneWayValue = request.value;
        return TK_SUCCESS;
    }

    TkResult OnResponse(const TkServiceContext &, const ResponseRequest &request, Response *response) noexcept
    {
        ++responseCalls;
        responseValue = request.value;
        if (responseResult != TK_SUCCESS)
        {
            return responseResult;
        }

        response->value = request.value + 1U;
        return TK_SUCCESS;
    }
};

struct NonNoexceptService final
{
    TkResult OnRequest(const TkServiceContext &, const Request &)
    {
        return TK_SUCCESS;
    }
};

struct WrongRequestService final
{
    TkResult OnRequest(const TkServiceContext &, const ResponseRequest &) noexcept
    {
        return TK_SUCCESS;
    }
};

struct NonNoexceptResponseService final
{
    TkResult OnResponse(const TkServiceContext &, const ResponseRequest &, Response *)
    {
        return TK_SUCCESS;
    }
};

struct WrongResponseService final
{
    TkResult OnResponse(const TkServiceContext &, const Request &, Response *) noexcept
    {
        return TK_SUCCESS;
    }
};

static_assert(pstk::service::detail::IsSupportedOneWayHandler<Service, Request, &Service::OnRequest>,
              "supported one-way signature must pass the trait");
static_assert(
    pstk::service::detail::IsSupportedRequestResponseHandler<Service, ResponseRequest, Response, &Service::OnResponse>,
    "supported request-response signature must pass the trait");
static_assert(
    !pstk::service::detail::IsSupportedOneWayHandler<NonNoexceptService, Request, &NonNoexceptService::OnRequest>,
    "non-noexcept one-way signature must fail the trait");
static_assert(
    !pstk::service::detail::IsSupportedOneWayHandler<WrongRequestService, Request, &WrongRequestService::OnRequest>,
    "request type mismatch must fail the trait");
static_assert(!pstk::service::detail::PacketContract<WrongPacketIdType>::is_valid,
              "PacketId must use the generated uint16_t metadata type");
static_assert(!pstk::service::detail::PacketContract<WrongPayloadBytesType>::is_valid,
              "PayloadBytes must use the generated size_t metadata type");
static_assert(!pstk::service::detail::IsSupportedRequestResponseHandler<
                  NonNoexceptResponseService, ResponseRequest, Response, &NonNoexceptResponseService::OnResponse>,
              "non-noexcept request-response signature must fail the trait");
static_assert(!pstk::service::detail::IsSupportedRequestResponseHandler<WrongResponseService, ResponseRequest, Response,
                                                                        &WrongResponseService::OnResponse>,
              "request-response type mismatch must fail the trait");

constexpr auto bindings = pstk::service::MakeBindings(
    pstk::service::BindOneWay<Service, Request, &Service::OnRequest>(),
    pstk::service::BindRequestResponse<Service, ResponseRequest, Response, &Service::OnResponse>());

static_assert(bindings.size() == 2U, "binding collection must be constexpr-capable");

struct State final
{
    std::vector<TkServiceJob *> jobs;
    int middlewareCalls = 0;
    int outputCalls = 0;
    TkHostOutputInfo outputInfo{};
    std::vector<std::uint8_t> outputBytes;
};

TkResult CaptureMiddleware(const TkServiceMiddlewareCallInfo *, TkDiagnosticCallbackInfo, void *const userData)
{
    ++static_cast<State *>(userData)->middlewareCalls;
    return TK_SUCCESS;
}

TkResult CaptureOutput(const TkHostOutputInfo *const outputInfo, void *const userData)
{
    State &state = *static_cast<State *>(userData);
    ++state.outputCalls;
    state.outputInfo = *outputInfo;
    state.outputBytes.assign(outputInfo->payload.data, outputInfo->payload.data + outputInfo->payload.size);
    return TK_SUCCESS;
}

TkResult CaptureExecutor(TkServiceJob *const job, void *const userData)
{
    static_cast<State *>(userData)->jobs.push_back(job);
    return TK_SUCCESS;
}

TkServiceHost *CreateHost(Service &service, State &state)
{
    const TkServiceHostCreateInfo createInfo{CaptureOutput, &state};
    TkServiceHost *host = nullptr;
    if (TkServiceHostCreate(&createInfo, &host) != TK_SUCCESS)
    {
        return nullptr;
    }

    const std::array<TkServiceMiddlewareInfo, 1U> middlewares{{{CaptureMiddleware, &state}}};
    if (pstk::service::RegisterService(host, service, {CaptureExecutor, &state}, middlewares, bindings) != TK_SUCCESS ||
        TkServiceHostFinalizeRegistration(host, {nullptr, nullptr}) != TK_SUCCESS)
    {
        TkServiceHostDestroy(host);
        return nullptr;
    }

    return host;
}
} // namespace

TEST(ServiceBindingFacade, BuildsSupportedDescriptorsFromPacketMetadata)
{
    std::array<TkServiceBindingInfo, bindings.size()> descriptors{};
    bindings.FillDescriptors(descriptors);

    EXPECT_EQ(descriptors[0].type, TK_BINDING_TYPE_ONE_WAY);
    EXPECT_EQ(descriptors[0].request.packetId, Request::PacketId);
    EXPECT_EQ(descriptors[0].request.payloadBytes, Request::PayloadBytes);
    EXPECT_EQ(descriptors[1].type, TK_BINDING_TYPE_REQUEST_RESPONSE);
    EXPECT_EQ(descriptors[1].request.packetId, ResponseRequest::PacketId);
    EXPECT_EQ(descriptors[1].operation.requestResponse.packetId, Response::PacketId);
    EXPECT_EQ(descriptors[1].operation.requestResponse.payloadBytes, Response::PayloadBytes);
    EXPECT_NE(descriptors[0].operation.oneWay.invokeHandler, nullptr);
    EXPECT_NE(descriptors[1].operation.requestResponse.invokeHandler, nullptr);
    EXPECT_NE(descriptors[1].operation.requestResponse.encodeResponse, nullptr);
}

TEST(ServiceBindingFacade, RegistersAndExecutesOneWayAndRequestResponseBindings)
{
    Service service;
    State state;
    TkServiceHost *const host = CreateHost(service, state);
    ASSERT_NE(host, nullptr);

    const std::array<std::uint8_t, Request::PayloadBytes> oneWayPayload{7U, 0U, 0U, 0U};
    ASSERT_EQ(TkServiceHostProcessPacket(host, {11U, 4U}, Request::PacketId,
                                         {oneWayPayload.data(), oneWayPayload.size()}, {nullptr, nullptr}),
              TK_SUCCESS);
    ASSERT_EQ(state.jobs.size(), 1U);
    EXPECT_EQ(service.oneWayCalls, 0);
    EXPECT_EQ(state.middlewareCalls, 1);
    EXPECT_EQ(TkServiceJobExecute(state.jobs[0], {nullptr, nullptr}), TK_SUCCESS);
    EXPECT_EQ(service.oneWayCalls, 1);
    EXPECT_EQ(service.oneWayValue, 7U);
    EXPECT_EQ(state.outputCalls, 0);
    TkServiceJobDestroy(state.jobs[0]);

    const std::array<std::uint8_t, ResponseRequest::PayloadBytes> responsePayload{9U, 0U, 0U, 0U};
    ASSERT_EQ(TkServiceHostProcessPacket(host, {11U, 4U}, ResponseRequest::PacketId,
                                         {responsePayload.data(), responsePayload.size()}, {nullptr, nullptr}),
              TK_SUCCESS);
    ASSERT_EQ(state.jobs.size(), 2U);
    EXPECT_EQ(state.middlewareCalls, 2);
    ASSERT_EQ(TkServiceJobExecute(state.jobs[1], {nullptr, nullptr}), TK_SUCCESS);
    EXPECT_EQ(service.responseCalls, 1);
    EXPECT_EQ(service.responseValue, 9U);
    EXPECT_EQ(state.outputCalls, 1);
    EXPECT_EQ(state.outputInfo.connectionKey.connectionId, 11U);
    EXPECT_EQ(state.outputInfo.connectionKey.generation, 4U);
    EXPECT_EQ(state.outputInfo.packetId, Response::PacketId);
    EXPECT_EQ(state.outputBytes, (std::vector<std::uint8_t>{10U, 0U, 0U, 0U}));
    TkServiceJobDestroy(state.jobs[1]);

    TkServiceHostDestroy(host);
}

TEST(ServiceBindingFacade, ResponseHandlerFailureDestroysTypedResponseWithoutOutput)
{
    Response::destructorCalls = 0;
    Service service;
    service.responseResult = TK_ERROR_REJECTED;
    State state;
    TkServiceHost *const host = CreateHost(service, state);
    ASSERT_NE(host, nullptr);

    const std::array<std::uint8_t, ResponseRequest::PayloadBytes> payload{9U, 0U, 0U, 0U};
    ASSERT_EQ(TkServiceHostProcessPacket(host, {11U, 4U}, ResponseRequest::PacketId, {payload.data(), payload.size()},
                                         {nullptr, nullptr}),
              TK_SUCCESS);
    ASSERT_EQ(state.jobs.size(), 1U);
    EXPECT_EQ(TkServiceJobExecute(state.jobs[0], {nullptr, nullptr}), TK_ERROR_REJECTED);
    EXPECT_EQ(Response::destructorCalls, 1);
    EXPECT_EQ(state.outputCalls, 0);
    TkServiceJobDestroy(state.jobs[0]);

    TkServiceHostDestroy(host);
}
