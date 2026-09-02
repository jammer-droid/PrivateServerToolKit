#include <gtest/gtest.h>

#include "WorldTimeSyncRequest.generated.h"
#include "WorldTimeSyncResponse.generated.h"

#include <pstk/service_host/TkServiceBinding.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace
{
using WorldTimeSyncRequest = pstk::packet::generated_time_sync::WorldTimeSyncRequest;
using WorldTimeSyncResponse = pstk::packet::generated_time_sync::WorldTimeSyncResponse;

struct TimeService final
{
    std::uint32_t currentServerTick = 0U;
    int handlerCalls = 0;

    TkResult OnTimeSync(const TkServiceContext &context, const WorldTimeSyncRequest &request,
                        WorldTimeSyncResponse *response) noexcept
    {
        EXPECT_EQ(context.connectionKey.connectionId, 700U);
        EXPECT_EQ(context.connectionKey.generation, 9U);
        ++handlerCalls;
        response->probeSequence = request.probeSequence;
        response->serverTick = currentServerTick;
        return TK_SUCCESS;
    }
};

struct PipelineState final
{
    TkServiceJob *job = nullptr;
    int middlewareCalls = 0;
    std::vector<std::uint8_t> outputBytes;
    TkHostOutputInfo outputInfo{};
};

TkResult CaptureMiddleware(const TkServiceMiddlewareCallInfo *const callInfo, TkDiagnosticCallbackInfo,
                           void *const userData)
{
    PipelineState &state = *static_cast<PipelineState *>(userData);
    ++state.middlewareCalls;
    EXPECT_EQ(callInfo->connectionKey.connectionId, 700U);
    EXPECT_EQ(callInfo->connectionKey.generation, 9U);
    EXPECT_EQ(callInfo->packetId, WorldTimeSyncRequest::PacketId);
    return TK_SUCCESS;
}

TkResult CaptureExecutor(TkServiceJob *const job, void *const userData)
{
    PipelineState &state = *static_cast<PipelineState *>(userData);
    EXPECT_EQ(state.job, nullptr);
    state.job = job;
    return TK_SUCCESS;
}

TkResult CaptureOutput(const TkHostOutputInfo *const outputInfo, void *const userData)
{
    PipelineState &state = *static_cast<PipelineState *>(userData);
    state.outputInfo = *outputInfo;
    state.outputBytes.assign(outputInfo->payload.data, outputInfo->payload.data + outputInfo->payload.size);
    return TK_SUCCESS;
}

constexpr auto timeServiceBindings =
    pstk::service::MakeBindings(pstk::service::BindRequestResponse<TimeService, WorldTimeSyncRequest,
                                                                   WorldTimeSyncResponse, &TimeService::OnTimeSync>());

static_assert(timeServiceBindings.size() == 1U, "TimeSync must have one typed binding");
static_assert(std::is_same_v<typename decltype(timeServiceBindings)::service_type, TimeService>,
              "TimeSync binding must retain the service type");
} // namespace

TEST(ServiceHostTimeSync, RunsGeneratedRequestResponseThroughTypedFacade)
{
    TimeService timeService;
    timeService.currentServerTick = 123456U;
    PipelineState state;
    const TkServiceHostCreateInfo createInfo{CaptureOutput, &state};
    TkServiceHost *host = nullptr;
    ASSERT_EQ(TkServiceHostCreate(&createInfo, &host), TK_SUCCESS);

    const std::array<TkServiceMiddlewareInfo, 1U> middlewares{{{CaptureMiddleware, &state}}};
    ASSERT_EQ(
        pstk::service::RegisterService(host, timeService, {CaptureExecutor, &state}, middlewares, timeServiceBindings),
        TK_SUCCESS);
    ASSERT_EQ(TkServiceHostFinalizeRegistration(host, {nullptr, nullptr}), TK_SUCCESS);

    WorldTimeSyncRequest request;
    request.probeSequence = 42U;
    std::array<std::uint8_t, WorldTimeSyncRequest::PayloadBytes> ownedRequestBytes{};
    ASSERT_EQ(request.Encode({ownedRequestBytes.data(), ownedRequestBytes.size()}), TK_SUCCESS);

    ASSERT_EQ(TkServiceHostProcessPacket(host, {700U, 9U}, WorldTimeSyncRequest::PacketId,
                                         {ownedRequestBytes.data(), ownedRequestBytes.size()}, {nullptr, nullptr}),
              TK_SUCCESS);
    EXPECT_EQ(timeService.handlerCalls, 0);
    EXPECT_EQ(state.middlewareCalls, 1);
    ASSERT_NE(state.job, nullptr);

    TkServiceHostDestroy(host);
    ASSERT_EQ(TkServiceJobExecute(state.job, {nullptr, nullptr}), TK_SUCCESS);
    EXPECT_EQ(timeService.handlerCalls, 1);
    EXPECT_EQ(state.outputInfo.connectionKey.connectionId, 700U);
    EXPECT_EQ(state.outputInfo.connectionKey.generation, 9U);
    EXPECT_EQ(state.outputInfo.packetId, WorldTimeSyncResponse::PacketId);

    WorldTimeSyncResponse decodedResponse;
    ASSERT_EQ(decodedResponse.Decode({state.outputBytes.data(), state.outputBytes.size()}), TK_SUCCESS);
    EXPECT_EQ(decodedResponse.probeSequence, 42U);
    EXPECT_EQ(decodedResponse.serverTick, 123456U);

    TkServiceJobDestroy(state.job);
}
