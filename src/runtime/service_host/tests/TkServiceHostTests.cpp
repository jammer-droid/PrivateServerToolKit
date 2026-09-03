#include <cstddef>
#include <cstdint>

#include <gtest/gtest.h>

#include <pstk/service_host/TkServiceHost.h>

namespace
{

TkResult NoOpOutput(const TkHostOutputInfo *, void *)
{
    return TK_SUCCESS;
}

TkResult NoOpMiddleware(const TkServiceMiddlewareCallInfo *, TkDiagnosticCallbackInfo, void *)
{
    return TK_SUCCESS;
}

TkResult NoOpSubmit(TkServiceJob *, void *)
{
    return TK_SUCCESS;
}

TkResult NoOpDecode(TkByteView, void *, TkDiagnosticCallbackInfo)
{
    return TK_SUCCESS;
}

void NoOpRequestDestroy(void *)
{
}

TkResult NoOpOneWay(void *, const TkServiceContext *, const void *)
{
    return TK_SUCCESS;
}

TkResult NoOpResponse(void *, const TkServiceContext *, const void *, void *)
{
    return TK_SUCCESS;
}

TkResult NoOpEncode(const void *, TkMutableByteView, TkDiagnosticCallbackInfo)
{
    return TK_SUCCESS;
}

void NoOpResponseDestroy(void *)
{
}

TkServiceHostCreateInfo ValidCreateInfo()
{
    return TkServiceHostCreateInfo{{NoOpOutput, nullptr}};
}

TkServiceBindingInfo ValidOneWayBinding(const uint16_t packetId)
{
    TkServiceBindingInfo binding{};
    binding.type = TK_BINDING_TYPE_ONE_WAY;
    binding.request.packetId = packetId;
    binding.request.payloadBytes = 4U;
    binding.request.requestSize = sizeof(uint32_t);
    binding.request.requestAlignment = alignof(uint32_t);
    binding.request.decodeRequest = NoOpDecode;
    binding.request.destroyRequest = NoOpRequestDestroy;
    binding.operation.oneWay.invokeHandler = NoOpOneWay;
    return binding;
}

TkServiceBindingInfo ValidRequestResponseBinding(const uint16_t packetId)
{
    TkServiceBindingInfo binding = ValidOneWayBinding(packetId);
    binding.type = TK_BINDING_TYPE_REQUEST_RESPONSE;
    binding.operation.requestResponse.packetId = static_cast<uint16_t>(packetId + 100U);
    binding.operation.requestResponse.payloadBytes = 8U;
    binding.operation.requestResponse.responseSize = sizeof(uint64_t);
    binding.operation.requestResponse.responseAlignment = alignof(uint64_t);
    binding.operation.requestResponse.invokeHandler = NoOpResponse;
    binding.operation.requestResponse.encodeResponse = NoOpEncode;
    binding.operation.requestResponse.destroyResponse = NoOpResponseDestroy;
    return binding;
}

TkServiceRegistrationInfo ValidRegistration(void *const serviceInstance, const TkServiceBindingInfo *bindings,
                                            const std::size_t bindingCount,
                                            const TkServiceMiddlewareInfo *middlewares = nullptr,
                                            const std::size_t middlewareCount = 0U)
{
    TkServiceRegistrationInfo registration{};
    registration.serviceInstance = serviceInstance;
    registration.executor = {NoOpSubmit, nullptr};
    registration.middlewares = middlewares;
    registration.middlewareCount = middlewareCount;
    registration.bindings = bindings;
    registration.bindingCount = bindingCount;
    return registration;
}

class ServiceHostTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        const TkServiceHostCreateInfo createInfo = ValidCreateInfo();
        ASSERT_EQ(TkServiceHostCreate(&createInfo, &host), TK_SUCCESS);
        ASSERT_NE(host, nullptr);
    }

    void TearDown() override
    {
        TkServiceHostDestroy(host);
        host = nullptr;
    }

    TkServiceHost *host = nullptr;
    int service = 0;
};

} // namespace

TEST(ServiceHostApi, ReturnsVersionAndPreservesOutputOnInvalidCall)
{
    uint32_t version = 99U;
    EXPECT_EQ(TkServiceHostGetApiVersion(&version), TK_SUCCESS);
    EXPECT_EQ(version, 1U);

    EXPECT_EQ(TkServiceHostGetApiVersion(nullptr), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(version, 1U);
}

TEST(ServiceHostApi, CreateRejectsInvalidArgumentsAndPreservesOutput)
{
    TkServiceHost *sentinel = reinterpret_cast<TkServiceHost *>(static_cast<uintptr_t>(1U));
    TkServiceHost *outHost = sentinel;
    TkServiceHostCreateInfo createInfo = ValidCreateInfo();

    EXPECT_EQ(TkServiceHostCreate(nullptr, &outHost), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(outHost, sentinel);
    EXPECT_EQ(TkServiceHostCreate(&createInfo, nullptr), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(outHost, sentinel);

    createInfo.outputAdapter.callback = nullptr;
    EXPECT_EQ(TkServiceHostCreate(&createInfo, &outHost), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(outHost, sentinel);

    TkServiceHostDestroy(nullptr);
}

TEST_F(ServiceHostTest, EmptyFinalizeSucceedsAndClosesConfiguration)
{
    EXPECT_EQ(TkServiceHostFinalizeRegistration(host, {nullptr, nullptr}), TK_SUCCESS);
    EXPECT_EQ(TkServiceHostFinalizeRegistration(host, {nullptr, nullptr}), TK_ERROR_INVALID_STATE);
}

TEST_F(ServiceHostTest, RejectsEmptyAndMalformedRegistration)
{
    TkServiceBindingInfo binding = ValidOneWayBinding(1U);
    TkServiceRegistrationInfo registration = ValidRegistration(&service, &binding, 1U);

    EXPECT_EQ(TkServiceHostRegisterService(nullptr, &registration, {nullptr, nullptr}), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(TkServiceHostFinalizeRegistration(nullptr, {nullptr, nullptr}), TK_ERROR_INVALID_ARGUMENT);

    EXPECT_EQ(TkServiceHostRegisterService(host, nullptr, {nullptr, nullptr}), TK_ERROR_INVALID_ARGUMENT);

    registration = ValidRegistration(&service, &binding, 0U);
    EXPECT_EQ(TkServiceHostRegisterService(host, &registration, {nullptr, nullptr}), TK_ERROR_INVALID_ARGUMENT);

    registration = ValidRegistration(&service, nullptr, 1U);
    EXPECT_EQ(TkServiceHostRegisterService(host, &registration, {nullptr, nullptr}), TK_ERROR_INVALID_ARGUMENT);

    registration = ValidRegistration(nullptr, &binding, 1U);
    EXPECT_EQ(TkServiceHostRegisterService(host, &registration, {nullptr, nullptr}), TK_ERROR_INVALID_ARGUMENT);

    registration = ValidRegistration(&service, &binding, 1U);
    registration.executor.callback = nullptr;
    EXPECT_EQ(TkServiceHostRegisterService(host, &registration, {nullptr, nullptr}), TK_ERROR_INVALID_ARGUMENT);

    registration = ValidRegistration(&service, &binding, 1U);
    registration.middlewares = reinterpret_cast<const TkServiceMiddlewareInfo *>(static_cast<uintptr_t>(1U));
    registration.middlewareCount = 0U;
    EXPECT_EQ(TkServiceHostRegisterService(host, &registration, {nullptr, nullptr}), TK_SUCCESS);

    registration = ValidRegistration(&service, &binding, 1U);
    registration.middlewares = nullptr;
    registration.middlewareCount = 1U;
    EXPECT_EQ(TkServiceHostRegisterService(host, &registration, {nullptr, nullptr}), TK_ERROR_INVALID_ARGUMENT);
}

TEST_F(ServiceHostTest, RejectsInvalidCallbacksSizesAlignmentAndType)
{
    TkServiceBindingInfo binding = ValidOneWayBinding(1U);
    TkServiceRegistrationInfo registration = ValidRegistration(&service, &binding, 1U);

    binding.request.decodeRequest = nullptr;
    EXPECT_EQ(TkServiceHostRegisterService(host, &registration, {nullptr, nullptr}), TK_ERROR_INVALID_ARGUMENT);
    binding = ValidOneWayBinding(1U);
    binding.request.destroyRequest = nullptr;
    EXPECT_EQ(TkServiceHostRegisterService(host, &registration, {nullptr, nullptr}), TK_ERROR_INVALID_ARGUMENT);
    binding = ValidOneWayBinding(1U);
    binding.request.requestSize = 0U;
    EXPECT_EQ(TkServiceHostRegisterService(host, &registration, {nullptr, nullptr}), TK_ERROR_INVALID_ARGUMENT);
    binding = ValidOneWayBinding(1U);
    binding.request.requestAlignment = 3U;
    EXPECT_EQ(TkServiceHostRegisterService(host, &registration, {nullptr, nullptr}), TK_ERROR_INVALID_ARGUMENT);
    binding = ValidOneWayBinding(1U);
    binding.request.requestSize = static_cast<std::size_t>(-1);
    EXPECT_EQ(TkServiceHostRegisterService(host, &registration, {nullptr, nullptr}), TK_ERROR_INVALID_ARGUMENT);
    binding = ValidOneWayBinding(1U);
    binding.operation.oneWay.invokeHandler = nullptr;
    EXPECT_EQ(TkServiceHostRegisterService(host, &registration, {nullptr, nullptr}), TK_ERROR_INVALID_ARGUMENT);
    binding = ValidOneWayBinding(1U);
    binding.type = static_cast<TkBindingType>(99);
    EXPECT_EQ(TkServiceHostRegisterService(host, &registration, {nullptr, nullptr}), TK_ERROR_INVALID_ARGUMENT);

    TkServiceMiddlewareInfo middleware{nullptr, nullptr};
    registration = ValidRegistration(&service, &binding, 1U, &middleware, 1U);
    EXPECT_EQ(TkServiceHostRegisterService(host, &registration, {nullptr, nullptr}), TK_ERROR_INVALID_ARGUMENT);
}

TEST_F(ServiceHostTest, OneWayDoesNotRequireInactiveResponseFields)
{
    TkServiceBindingInfo binding = ValidOneWayBinding(1U);
    binding.operation.oneWay.invokeHandler = NoOpOneWay;
    TkServiceRegistrationInfo registration = ValidRegistration(&service, &binding, 1U);

    EXPECT_EQ(TkServiceHostRegisterService(host, &registration, {nullptr, nullptr}), TK_SUCCESS);
}

TEST_F(ServiceHostTest, ValidatesRequestResponseFields)
{
    TkServiceBindingInfo binding = ValidRequestResponseBinding(1U);
    TkServiceRegistrationInfo registration = ValidRegistration(&service, &binding, 1U);

    binding.operation.requestResponse.responseSize = 0U;
    EXPECT_EQ(TkServiceHostRegisterService(host, &registration, {nullptr, nullptr}), TK_ERROR_INVALID_ARGUMENT);
    binding = ValidRequestResponseBinding(1U);
    binding.operation.requestResponse.responseAlignment = 3U;
    EXPECT_EQ(TkServiceHostRegisterService(host, &registration, {nullptr, nullptr}), TK_ERROR_INVALID_ARGUMENT);
    binding = ValidRequestResponseBinding(1U);
    binding.operation.requestResponse.invokeHandler = nullptr;
    EXPECT_EQ(TkServiceHostRegisterService(host, &registration, {nullptr, nullptr}), TK_ERROR_INVALID_ARGUMENT);
    binding = ValidRequestResponseBinding(1U);
    binding.operation.requestResponse.encodeResponse = nullptr;
    EXPECT_EQ(TkServiceHostRegisterService(host, &registration, {nullptr, nullptr}), TK_ERROR_INVALID_ARGUMENT);
    binding = ValidRequestResponseBinding(1U);
    binding.operation.requestResponse.destroyResponse = nullptr;
    EXPECT_EQ(TkServiceHostRegisterService(host, &registration, {nullptr, nullptr}), TK_ERROR_INVALID_ARGUMENT);
}

TEST_F(ServiceHostTest, RejectsDuplicateWithinRegistrationAndAcrossServicesAtomically)
{
    TkServiceBindingInfo initialBinding = ValidOneWayBinding(1U);
    TkServiceRegistrationInfo initialRegistration = ValidRegistration(&service, &initialBinding, 1U);
    ASSERT_EQ(TkServiceHostRegisterService(host, &initialRegistration, {nullptr, nullptr}), TK_SUCCESS);

    TkServiceBindingInfo duplicateBindings[] = {ValidOneWayBinding(2U), ValidOneWayBinding(1U)};
    TkServiceRegistrationInfo duplicateRegistration = ValidRegistration(&service, duplicateBindings, 2U);
    EXPECT_EQ(TkServiceHostRegisterService(host, &duplicateRegistration, {nullptr, nullptr}),
              TK_ERROR_INVALID_ARGUMENT);

    TkServiceBindingInfo newBinding = ValidOneWayBinding(2U);
    TkServiceRegistrationInfo newRegistration = ValidRegistration(&service, &newBinding, 1U);
    EXPECT_EQ(TkServiceHostRegisterService(host, &newRegistration, {nullptr, nullptr}), TK_SUCCESS);

    EXPECT_EQ(TkServiceHostRegisterService(host, &initialRegistration, {nullptr, nullptr}), TK_ERROR_INVALID_ARGUMENT);
}

TEST_F(ServiceHostTest, RejectsDuplicatePacketIdsWithinNewRegistration)
{
    TkServiceBindingInfo duplicateBindings[] = {ValidOneWayBinding(1U), ValidOneWayBinding(1U)};
    TkServiceRegistrationInfo duplicateRegistration = ValidRegistration(&service, duplicateBindings, 2U);

    EXPECT_EQ(TkServiceHostRegisterService(host, &duplicateRegistration, {nullptr, nullptr}),
              TK_ERROR_INVALID_ARGUMENT);

    TkServiceBindingInfo validBinding = ValidOneWayBinding(1U);
    TkServiceRegistrationInfo validRegistration = ValidRegistration(&service, &validBinding, 1U);
    EXPECT_EQ(TkServiceHostRegisterService(host, &validRegistration, {nullptr, nullptr}), TK_SUCCESS);
}

TEST_F(ServiceHostTest, CopiesBorrowedBindingArray)
{
    TkServiceBindingInfo binding = ValidOneWayBinding(1U);
    TkServiceRegistrationInfo registration = ValidRegistration(&service, &binding, 1U);
    ASSERT_EQ(TkServiceHostRegisterService(host, &registration, {nullptr, nullptr}), TK_SUCCESS);

    binding.request.packetId = 2U;
    TkServiceRegistrationInfo changedRegistration = ValidRegistration(&service, &binding, 1U);
    ASSERT_EQ(TkServiceHostRegisterService(host, &changedRegistration, {nullptr, nullptr}), TK_SUCCESS);

    TkServiceBindingInfo originalId = ValidOneWayBinding(1U);
    TkServiceRegistrationInfo duplicateOriginal = ValidRegistration(&service, &originalId, 1U);
    EXPECT_EQ(TkServiceHostRegisterService(host, &duplicateOriginal, {nullptr, nullptr}), TK_ERROR_INVALID_ARGUMENT);
}

TEST_F(ServiceHostTest, AcceptsValidMiddlewareRegistration)
{
    TkServiceMiddlewareInfo middleware{NoOpMiddleware, nullptr};
    TkServiceBindingInfo binding = ValidOneWayBinding(1U);
    TkServiceRegistrationInfo registration = ValidRegistration(&service, &binding, 1U, &middleware, 1U);

    EXPECT_EQ(TkServiceHostRegisterService(host, &registration, {nullptr, nullptr}), TK_SUCCESS);
}

TEST_F(ServiceHostTest, RejectsNullMiddlewareCallback)
{
    TkServiceMiddlewareInfo middleware{nullptr, nullptr};
    TkServiceBindingInfo binding = ValidOneWayBinding(1U);
    TkServiceRegistrationInfo registration = ValidRegistration(&service, &binding, 1U, &middleware, 1U);
    EXPECT_EQ(TkServiceHostRegisterService(host, &registration, {nullptr, nullptr}), TK_ERROR_INVALID_ARGUMENT);
}

TEST_F(ServiceHostTest, RejectsRegistrationAfterFinalize)
{
    ASSERT_EQ(TkServiceHostFinalizeRegistration(host, {nullptr, nullptr}), TK_SUCCESS);

    TkServiceBindingInfo binding = ValidOneWayBinding(1U);
    TkServiceRegistrationInfo registration = ValidRegistration(&service, &binding, 1U);
    EXPECT_EQ(TkServiceHostRegisterService(host, &registration, {nullptr, nullptr}), TK_ERROR_INVALID_STATE);
}
