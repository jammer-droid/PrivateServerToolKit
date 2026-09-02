#include <pstk/service_host/TkServiceHost.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{

enum class HostState
{
    Configuring,
    Ready
};

struct StoredBinding final
{
    TkServiceBindingInfo info{};

    explicit StoredBinding(const TkServiceBindingInfo &source) noexcept
    {
        info.type = source.type;
        info.request = source.request;

        if (source.type == TK_BINDING_TYPE_ONE_WAY)
        {
            info.operation.oneWay = source.operation.oneWay;
        }
        else
        {
            info.operation.requestResponse = source.operation.requestResponse;
        }
    }
};

struct StoredService final
{
    void *serviceInstance = nullptr;
    TkServiceExecutorCallbackInfo executor{};
    std::vector<TkServiceMiddlewareInfo> middlewares;
    std::vector<StoredBinding> bindings;
};

struct LookupEntry final
{
    uint16_t packetId = 0;
    std::size_t serviceIndex = 0;
    std::size_t bindingIndex = 0;
};

enum class JobState
{
    Ready,
    Executed
};

} // namespace

struct TkServiceHost final
{
    TkHostOutputCallbackInfo outputAdapter{};

    std::vector<StoredService> services;
    std::vector<LookupEntry> lookup;

    HostState state = HostState::Configuring;
};

struct TkServiceJob final
{
    TkServiceJob() noexcept = default;

    void *allocation = nullptr;           // allocation bytes for TkServiceJob instance
    std::size_t allocationAlignment = 0U; // align

    void *requestStorage = nullptr; // storage for decoded Payload
    TkHostConnectionKey connectionKey{};
    void *serviceInstance = nullptr;
    TkRequestDestroyCallback destroyRequest = nullptr;
    TkOneWayHandlerCallback invokeHandler = nullptr;

    JobState state = JobState::Ready;
};

namespace
{

inline constexpr const char *UnknownPacketId = "PSTK-SERVICE-HOST-UNKNOWN-PACKET";
inline constexpr const char *InvalidPayloadSizeId = "PSTK-SERVICE-HOST-INVALID-PAYLOAD-SIZE";
inline constexpr const char *ExecutorSubmitFailedId = "PSTK-SERVICE-HOST-EXECUTOR-SUBMIT-FAILED";
inline constexpr const char *HandlerFailedId = "PSTK-SERVICE-HOST-HANDLER-FAILED";

bool IsPowerOfTwo(const std::size_t value) noexcept
{
    return value != 0U && (value & (value - 1U)) == 0U;
}

bool IsValidStorageLayout(const std::size_t objectSize, const std::size_t objectAlignment) noexcept
{
    if (objectSize == 0U || !IsPowerOfTwo(objectAlignment))
    {
        return false;
    }

    return objectSize <= std::numeric_limits<std::size_t>::max() - (objectAlignment - 1U);
}

bool IsKnownBindingType(const TkBindingType type) noexcept
{
    return type == TK_BINDING_TYPE_ONE_WAY || type == TK_BINDING_TYPE_REQUEST_RESPONSE;
}

bool IsValidPointerForCount(const void *const pointer, const std::size_t count) noexcept
{
    return count == 0U || pointer != nullptr;
}

bool HasPacketId(const std::vector<StoredService> &services, const uint16_t packetId) noexcept
{
    for (const StoredService &service : services)
    {
        for (const StoredBinding &binding : service.bindings)
        {
            if (binding.info.request.packetId == packetId)
            {
                return true;
            }
        }
    }

    return false;
}

bool HasPacketId(const std::vector<StoredBinding> &bindings, const uint16_t packetId) noexcept
{
    for (const StoredBinding &binding : bindings)
    {
        if (binding.info.request.packetId == packetId)
        {
            return true;
        }
    }

    return false;
}

void EmitServiceDiagnostic(const TkDiagnosticCallbackInfo diagnostic, const char *const id,
                           const char *const message) noexcept
{
    const TkDiagnostic serviceDiagnostic = {
        TK_DIAGNOSTIC_ERROR,
        id,
        message,
        {nullptr, 0U, 0U, 0U},
    };
    TkEmitDiagnostic(diagnostic, &serviceDiagnostic);
}

void EmitUnknownPacketDiagnostic(const uint16_t packetId, const TkDiagnosticCallbackInfo diagnostic) noexcept
{
    char message[96]{};
    std::snprintf(message, sizeof(message), "packet id %u is not registered", static_cast<unsigned int>(packetId));
    EmitServiceDiagnostic(diagnostic, UnknownPacketId, message);
}

void EmitInvalidPayloadSizeDiagnostic(const std::size_t expectedSize, const std::size_t actualSize,
                                      const TkDiagnosticCallbackInfo diagnostic) noexcept
{
    char message[128]{};
    std::snprintf(message, sizeof(message), "payload size must be exactly %zu bytes; actual size is %zu bytes",
                  expectedSize, actualSize);
    EmitServiceDiagnostic(diagnostic, InvalidPayloadSizeId, message);
}

void EmitResultDiagnostic(const char *const id, const char *const operation, const TkResult result,
                          const TkDiagnosticCallbackInfo diagnostic) noexcept
{
    char message[128]{};
    std::snprintf(message, sizeof(message), "%s failed with result %d", operation, static_cast<int>(result));
    EmitServiceDiagnostic(diagnostic, id, message);
}

TkResult ValidateBinding(const TkServiceBindingInfo &binding) noexcept
{
    if (!IsKnownBindingType(binding.type))
    {
        return TK_ERROR_INVALID_ARGUMENT;
    }

    if (!IsValidStorageLayout(binding.request.requestSize, binding.request.requestAlignment) ||
        binding.request.payloadBytes == 0U || binding.request.decodeRequest == nullptr ||
        binding.request.destroyRequest == nullptr)
    {
        return TK_ERROR_INVALID_ARGUMENT;
    }

    if (binding.type == TK_BINDING_TYPE_ONE_WAY)
    {
        if (binding.operation.oneWay.invokeHandler == nullptr)
        {
            return TK_ERROR_INVALID_ARGUMENT;
        }

        return TK_SUCCESS;
    }

    const TkRequestResponseBindingInfo &response = binding.operation.requestResponse;
    if (!IsValidStorageLayout(response.responseSize, response.responseAlignment) || response.payloadBytes == 0U ||
        response.invokeHandler == nullptr || response.encodeResponse == nullptr || response.destroyResponse == nullptr)
    {
        return TK_ERROR_INVALID_ARGUMENT;
    }

    return TK_SUCCESS;
}

TkResult StageRegistration(const TkServiceHost &host, const TkServiceRegistrationInfo &registration,
                           StoredService *const outService) noexcept
{
    if (outService == nullptr || registration.serviceInstance == nullptr || registration.executor.callback == nullptr ||
        registration.bindingCount == 0U || registration.bindings == nullptr ||
        !IsValidPointerForCount(registration.middlewares, registration.middlewareCount))
    {
        return TK_ERROR_INVALID_ARGUMENT;
    }

    StoredService stagedService;
    stagedService.serviceInstance = registration.serviceInstance;
    stagedService.executor = registration.executor;

    try
    {
        stagedService.middlewares.reserve(registration.middlewareCount);
        for (std::size_t index = 0; index < registration.middlewareCount; ++index)
        {
            const TkServiceMiddlewareInfo &middleware = registration.middlewares[index];
            if (middleware.callback == nullptr)
            {
                return TK_ERROR_INVALID_ARGUMENT;
            }

            stagedService.middlewares.push_back(middleware);
        }

        stagedService.bindings.reserve(registration.bindingCount);
        for (std::size_t index = 0; index < registration.bindingCount; ++index)
        {
            const TkServiceBindingInfo &binding = registration.bindings[index];
            const TkResult bindingResult = ValidateBinding(binding);
            if (bindingResult != TK_SUCCESS)
            {
                return bindingResult;
            }

            if (HasPacketId(stagedService.bindings, binding.request.packetId) ||
                HasPacketId(host.services, binding.request.packetId))
            {
                return TK_ERROR_INVALID_ARGUMENT;
            }

            stagedService.bindings.emplace_back(binding);
        }
    }
    catch (const std::bad_alloc &)
    {
        return TK_ERROR_OUT_OF_MEMORY;
    }
    catch (const std::length_error &)
    {
        return TK_ERROR_OUT_OF_MEMORY;
    }
    catch (...)
    {
        return TK_ERROR_UNKNOWN;
    }

    *outService = std::move(stagedService);
    return TK_SUCCESS;
}

// std::lower_bound는 comp의 결과가 'true'이면 탐색 범위를 start = mid+1로 이동한다.
// 따라서 packetId와 같은 값을 찾으려면 candidate.packetId >= value가 되는 첫 구간을 찾아야 한다.
// - *mid < value : 이 경우에는 mid의 위치가 value보다 작기 때문에 start = mid + 1 로 이동한다.
// - *mid >= value : 이 경우에는 mid가 value보다 크거나 같은 구간에 속하기 때문에 end = mid 로 이동한다.
const StoredBinding *FindBinding(const TkServiceHost &host, const uint16_t packetId) noexcept
{
    const std::vector<LookupEntry>::const_iterator entry =
        std::lower_bound(host.lookup.begin(), host.lookup.end(), packetId,
                         [](const LookupEntry &candidate, const uint16_t value) { return candidate.packetId < value; });
    if (entry == host.lookup.end() || entry->packetId != packetId)
    {
        return nullptr;
    }

    return &host.services[entry->serviceIndex].bindings[entry->bindingIndex];
}

const StoredService *FindService(const TkServiceHost &host, const uint16_t packetId) noexcept
{
    const std::vector<LookupEntry>::const_iterator entry =
        std::lower_bound(host.lookup.begin(), host.lookup.end(), packetId,
                         [](const LookupEntry &candidate, const uint16_t value) { return candidate.packetId < value; });
    if (entry == host.lookup.end() || entry->packetId != packetId)
    {
        return nullptr;
    }

    return &host.services[entry->serviceIndex];
}

// value를 value 이상인 가장 가까운 alignment 배수로 올림 정렬한다.
// alignment는 2의 거듭제곱이고 mask는 alignment - 1이다.
// (value + mask)에서 alignment 나머지를 나타내는 하위 비트를 ~mask로 제거한다.
bool TryAlignUp(const std::size_t value, const std::size_t alignment, std::size_t *const outValue) noexcept
{
    const std::size_t mask = alignment - 1U;
    if (value > std::numeric_limits<std::size_t>::max() - mask)
    {
        return false;
    }

    *outValue = (value + mask) & ~mask;
    return true;
}

TkResult AllocateOneWayJob(const TkHostConnectionKey connectionKey, const StoredService &service,
                           const StoredBinding &binding, TkServiceJob **const outJob) noexcept
{
    if (outJob == nullptr)
    {
        return TK_ERROR_INVALID_ARGUMENT;
    }

    const std::size_t requestAlignment = binding.info.request.requestAlignment;
    const std::size_t allocationAlignment = std::max(alignof(TkServiceJob), requestAlignment);

    std::size_t requestOffset = 0U;
    if (!TryAlignUp(sizeof(TkServiceJob), requestAlignment, &requestOffset) ||
        requestOffset > std::numeric_limits<std::size_t>::max() - binding.info.request.requestSize)
    {
        return TK_ERROR_OUT_OF_MEMORY;
    }

    const std::size_t allocationSize = requestOffset + binding.info.request.requestSize;

    // 전역 메모리 할당 함수를 직접 호출 (객체 생성자는 실행하지 않음)
    // 결과는 raw memory
    void *const allocation = ::operator new(allocationSize, std::align_val_t(allocationAlignment), std::nothrow);
    if (allocation == nullptr)
    {
        return TK_ERROR_OUT_OF_MEMORY;
    }

    // 전역 allocation function 사용을 지정한 new-expression
    // 메모리를 확보하거나 지정된 위치를 받은 다음 T의 생성자 실행
    TkServiceJob *const job = new (allocation) TkServiceJob();
    job->allocation = allocation;
    job->allocationAlignment = allocationAlignment;
    job->requestStorage = static_cast<void *>(static_cast<unsigned char *>(allocation) + requestOffset);
    job->connectionKey = connectionKey;
    job->serviceInstance = service.serviceInstance;
    job->destroyRequest = binding.info.request.destroyRequest;
    job->invokeHandler = binding.info.operation.oneWay.invokeHandler;
    *outJob = job;
    return TK_SUCCESS;
}

void FreeJobAllocation(TkServiceJob *const job) noexcept
{
    if (job == nullptr)
    {
        return;
    }

    const std::size_t allocationAlignment = job->allocationAlignment;
    void *const allocation = job->allocation;
    job->~TkServiceJob();

    ::operator delete(allocation, std::align_val_t(allocationAlignment));
}

void DestroyJob(TkServiceJob *const job) noexcept
{
    if (job == nullptr)
    {
        return;
    }

    job->destroyRequest(job->requestStorage);
    FreeJobAllocation(job);
}

TkResult ProcessPacket(const TkServiceHost &host, const TkHostConnectionKey connectionKey, const uint16_t packetId,
                       const TkByteView payload, const TkDiagnosticCallbackInfo diagnostic) noexcept
{
    const StoredBinding *const binding = FindBinding(host, packetId);
    const StoredService *const service = FindService(host, packetId);
    if (binding == nullptr || service == nullptr)
    {
        EmitUnknownPacketDiagnostic(packetId, diagnostic);
        return TK_ERROR_INVALID_DATA;
    }

    if (payload.size != binding->info.request.payloadBytes)
    {
        EmitInvalidPayloadSizeDiagnostic(binding->info.request.payloadBytes, payload.size, diagnostic);
        return TK_ERROR_INVALID_DATA;
    }

    if (binding->info.type != TK_BINDING_TYPE_ONE_WAY)
    {
        return TK_ERROR_INVALID_STATE;
    }

    TkServiceJob *job = nullptr;
    const TkResult allocationResult = AllocateOneWayJob(connectionKey, *service, *binding, &job);
    if (allocationResult != TK_SUCCESS)
    {
        return allocationResult;
    }

    TkResult result = binding->info.request.decodeRequest(payload, job->requestStorage, diagnostic);
    if (result != TK_SUCCESS)
    {
        FreeJobAllocation(job);
        return result;
    }

    const TkServiceMiddlewareCallInfo middlewareCallInfo = {connectionKey, packetId};
    for (const TkServiceMiddlewareInfo &middleware : service->middlewares)
    {
        result = middleware.callback(&middlewareCallInfo, diagnostic, middleware.userData);
        if (result != TK_SUCCESS)
        {
            DestroyJob(job);
            return result;
        }
    }

    result = service->executor.callback(job, service->executor.userData);
    if (result == TK_SUCCESS)
    {
        job = nullptr;
        return TK_SUCCESS;
    }

    EmitResultDiagnostic(ExecutorSubmitFailedId, "executor submit", result, diagnostic);
    DestroyJob(job);
    return result;
}

TkResult InvokeOneWayHandler(TkServiceJob &job, const TkDiagnosticCallbackInfo diagnostic) noexcept
{
    job.state = JobState::Executed;
    const TkServiceContext context = {job.connectionKey};
    const TkResult result = job.invokeHandler(job.serviceInstance, &context, job.requestStorage);
    if (result != TK_SUCCESS)
    {
        EmitResultDiagnostic(HandlerFailedId, "service handler", result, diagnostic);
    }

    return result;
}

} // namespace

PSTK_SERVICE_HOST_API TkResult TkServiceHostGetApiVersion(uint32_t *const outVersion)
{
    if (outVersion == nullptr)
    {
        return TK_ERROR_INVALID_ARGUMENT;
    }

    *outVersion = 1U;
    return TK_SUCCESS;
}

PSTK_SERVICE_HOST_API TkResult TkServiceHostCreate(const TkServiceHostCreateInfo *const createInfo,
                                                   TkServiceHost **const outHost)
{
    if (createInfo == nullptr || outHost == nullptr || createInfo->outputAdapter.callback == nullptr)
    {
        return TK_ERROR_INVALID_ARGUMENT;
    }

    try
    {
        std::unique_ptr<TkServiceHost> host(new TkServiceHost());
        host->outputAdapter = createInfo->outputAdapter;
        *outHost = host.release();
        return TK_SUCCESS;
    }
    catch (const std::bad_alloc &)
    {
        return TK_ERROR_OUT_OF_MEMORY;
    }
    catch (const std::length_error &)
    {
        return TK_ERROR_OUT_OF_MEMORY;
    }
    catch (...)
    {
        return TK_ERROR_UNKNOWN;
    }
}

PSTK_SERVICE_HOST_API TkResult TkServiceHostRegisterService(TkServiceHost *const host,
                                                            const TkServiceRegistrationInfo *const registration,
                                                            const TkDiagnosticCallbackInfo diagnostic)
{
    (void)diagnostic;

    if (host == nullptr || registration == nullptr)
    {
        return TK_ERROR_INVALID_ARGUMENT;
    }

    if (host->state != HostState::Configuring)
    {
        return TK_ERROR_INVALID_STATE;
    }

    StoredService stagedService;
    const TkResult stageResult = StageRegistration(*host, *registration, &stagedService);
    if (stageResult != TK_SUCCESS)
    {
        return stageResult;
    }

    try
    {
        host->services.push_back(std::move(stagedService));
    }
    catch (const std::bad_alloc &)
    {
        return TK_ERROR_OUT_OF_MEMORY;
    }
    catch (const std::length_error &)
    {
        return TK_ERROR_OUT_OF_MEMORY;
    }
    catch (...)
    {
        return TK_ERROR_UNKNOWN;
    }

    return TK_SUCCESS;
}

PSTK_SERVICE_HOST_API TkResult TkServiceHostFinalizeRegistration(TkServiceHost *const host,
                                                                 const TkDiagnosticCallbackInfo diagnostic)
{
    (void)diagnostic;

    if (host == nullptr)
    {
        return TK_ERROR_INVALID_ARGUMENT;
    }

    if (host->state != HostState::Configuring)
    {
        return TK_ERROR_INVALID_STATE;
    }

    std::vector<LookupEntry> stagedLookup;
    try
    {
        std::size_t bindingCount = 0U;
        for (const StoredService &service : host->services)
        {
            if (bindingCount > std::numeric_limits<std::size_t>::max() - service.bindings.size())
            {
                return TK_ERROR_OUT_OF_MEMORY;
            }

            bindingCount += service.bindings.size();
        }

        stagedLookup.reserve(bindingCount);
        for (std::size_t serviceIndex = 0; serviceIndex < host->services.size(); ++serviceIndex)
        {
            const StoredService &service = host->services[serviceIndex];
            for (std::size_t bindingIndex = 0; bindingIndex < service.bindings.size(); ++bindingIndex)
            {
                stagedLookup.push_back(
                    LookupEntry{service.bindings[bindingIndex].info.request.packetId, serviceIndex, bindingIndex});
            }
        }

        std::sort(stagedLookup.begin(), stagedLookup.end(),
                  [](const LookupEntry &left, const LookupEntry &right) { return left.packetId < right.packetId; });
    }
    catch (const std::bad_alloc &)
    {
        return TK_ERROR_OUT_OF_MEMORY;
    }
    catch (const std::length_error &)
    {
        return TK_ERROR_OUT_OF_MEMORY;
    }
    catch (...)
    {
        return TK_ERROR_UNKNOWN;
    }

    host->lookup.swap(stagedLookup);
    host->state = HostState::Ready;
    return TK_SUCCESS;
}

PSTK_SERVICE_HOST_API void TkServiceHostDestroy(TkServiceHost *const host)
{
    delete host;
}

PSTK_SERVICE_HOST_API TkResult TkServiceHostProcessPacket(TkServiceHost *const host,
                                                          const TkHostConnectionKey connectionKey,
                                                          const uint16_t packetId, const TkByteView payload,
                                                          const TkDiagnosticCallbackInfo diagnostic)
{
    if (host == nullptr || !TkIsValidByteRange(payload.data, payload.size) || connectionKey.generation == 0U)
    {
        return TK_ERROR_INVALID_ARGUMENT;
    }

    if (host->state != HostState::Ready)
    {
        return TK_ERROR_INVALID_STATE;
    }

    return ProcessPacket(*host, connectionKey, packetId, payload, diagnostic);
}

PSTK_SERVICE_HOST_API TkResult TkServiceJobExecute(TkServiceJob *const job, const TkDiagnosticCallbackInfo diagnostic)
{
    if (job == nullptr)
    {
        return TK_ERROR_INVALID_ARGUMENT;
    }

    if (job->state != JobState::Ready)
    {
        return TK_ERROR_INVALID_STATE;
    }

    return InvokeOneWayHandler(*job, diagnostic);
}

PSTK_SERVICE_HOST_API void TkServiceJobDestroy(TkServiceJob *const job)
{
    DestroyJob(job);
}
