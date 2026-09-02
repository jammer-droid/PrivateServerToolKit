#include <pstk/service_host/TkServiceHost.h>

#include <algorithm>
#include <cstddef>
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

} // namespace

struct TkServiceHost final
{
    TkHostOutputCallbackInfo outputAdapter{};

    std::vector<StoredService> services;
    std::vector<LookupEntry> lookup;

    HostState state = HostState::Configuring;
};

namespace
{

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
