#include "VulkanContext.h"

#include "common/VulkanException.h"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <cstdio>

namespace
{

constexpr std::uint32_t kRequiredApiVersion = VK_API_VERSION_1_3;
// portability device 열거 기능 활성화
constexpr const char *kRequiredExtensionPortabilityEnumeration = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
constexpr const char *kRequiredExtensionDebugUtils = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
constexpr const char *kValidationLayer = "VK_LAYER_KHRONOS_validation";
constexpr const char *kLayerNames[] = {kValidationLayer};

#ifdef NDEBUG
constexpr bool kEnableValidation = false;
#else
constexpr bool kEnableValidation = true;
#endif

std::string FormatApiVersion(std::uint32_t version)
{
    return std::to_string(VK_API_VERSION_MAJOR(version)) + "." + std::to_string(VK_API_VERSION_MINOR(version)) + "." +
           std::to_string(VK_API_VERSION_PATCH(version));
}

std::vector<VkExtensionProperties> EnumerateInstanceExtensions()
{
    std::uint32_t extensionCount = 0;
    std::vector<VkExtensionProperties> extensionProperties;

    while (true)
    {
        VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr));
        if (extensionCount == 0)
        {
            break;
        }

        extensionProperties.resize(extensionCount);

        VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensionProperties.data());

        if (result == VK_INCOMPLETE)
        {
            extensionProperties.clear();
            continue;
        }

        VK_CHECK(result);
        extensionProperties.resize(extensionCount);
        break;
    }

    std::cout << "Extension Count: " << extensionCount << '\n';
    for (const VkExtensionProperties &property : extensionProperties)
    {
        std::cout << property.extensionName << ", " << property.specVersion << '\n';
    }
    std::cout << '\n';
    return extensionProperties;
}

std::vector<VkLayerProperties> EnumerateInstanceLayers()
{
    std::uint32_t layerCount = 0;
    std::vector<VkLayerProperties> layers;

    while (true)
    {
        VK_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, nullptr));
        if (layerCount == 0)
        {
            break;
        }

        layers.resize(layerCount);
        VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, layers.data());
        if (result == VK_INCOMPLETE)
        {
            layers.clear();
            continue;
        }

        VK_CHECK(result);
        layers.resize(layerCount);
        break;
    }

    std::cout << "Layer Count: " << layerCount << '\n';
    for (const VkLayerProperties &layer : layers)
    {
        std::cout << layer.layerName << ' ' << layer.description << '\n';
    }
    std::cout << '\n';

    return layers;
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                             VkDebugUtilsMessageTypeFlagsEXT types,
                                             const VkDebugUtilsMessengerCallbackDataEXT *data, void *userData) noexcept
{
    std::fprintf(stderr, "[Vulkan] %s\n", data->pMessage);
    return VK_FALSE;
}

VkDebugUtilsMessengerCreateInfoEXT MakeDebugMessengerCreateInfo()
{
    VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo{};
    messengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    messengerCreateInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    messengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    messengerCreateInfo.pfnUserCallback = DebugCallback;
    messengerCreateInfo.pUserData = nullptr;

    return messengerCreateInfo;
}

VkInstance CreateInstance()
{
    std::uint32_t apiVersion = VulkanContext::GetApiVersion();
    std::cout << "ApiVersion: " << VK_API_VERSION_MAJOR(apiVersion) << '.' << VK_API_VERSION_MINOR(apiVersion) << '.'
              << VK_API_VERSION_PATCH(apiVersion) << '\n';

    if (apiVersion < kRequiredApiVersion)
    {
        throw std::runtime_error(std::string("Error. Supported: " + FormatApiVersion(apiVersion) +
                                             ", required: " + FormatApiVersion(kRequiredApiVersion)));
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "RendererProject";
    appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    appInfo.apiVersion = kRequiredApiVersion;
    appInfo.pEngineName = "Custom";
    appInfo.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);

    VkInstanceCreateInfo instanceCreateInfo{};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;

    std::vector<VkExtensionProperties> extensionProperties = EnumerateInstanceExtensions();
    std::vector<const char *> requiredExtensions;

#ifdef __APPLE__
    requiredExtensions.push_back(kRequiredExtensionPortabilityEnumeration);

    // MoltenVK -> Vulkan을 Metal 위에 구현하는 소프트웨어
    //  - PhysicalDevice 조회시 MoltenVK가 Device를 노출함(portability device)
    // 실제 GPU -> Apple GPU HW
    // VkPhysicalDevice -> 앱이 조회하는 GPU 핸들

    // portability device도 조회 결과에 포함하게 설정
    instanceCreateInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    if (kEnableValidation)
    {
        requiredExtensions.push_back(kRequiredExtensionDebugUtils);
    }

    for (const char *requiredExtension : requiredExtensions)
    {
        bool isSupported = false;
        for (const VkExtensionProperties &property : extensionProperties)
        {
            if (std::strcmp(property.extensionName, requiredExtension) == 0)
            {
                isSupported = true;
                break;
            }
        }

        if (!isSupported)
        {
            throw std::runtime_error("Required Extension Not Found: " + std::string(requiredExtension));
        }
    }

    instanceCreateInfo.enabledExtensionCount = static_cast<std::uint32_t>(requiredExtensions.size());
    instanceCreateInfo.ppEnabledExtensionNames = requiredExtensions.empty() ? nullptr : requiredExtensions.data();

    if (kEnableValidation) // Debug Layer
    {
        std::vector<VkLayerProperties> layers = EnumerateInstanceLayers();

        bool flag = false;
        for (const VkLayerProperties &layer : layers)
        {
            if (std::strcmp(layer.layerName, kValidationLayer) == 0)
            {
                flag = true;
            }
        }

        if (!flag)
        {
            throw std::runtime_error("Validation Layer Not Founded.\n");
        }

        instanceCreateInfo.enabledLayerCount = 1;
        instanceCreateInfo.ppEnabledLayerNames = kLayerNames;
    }

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (kEnableValidation) // Instance 생성/파괴 디버깅을 위한 pNext 연결 설정
    {
        debugCreateInfo = MakeDebugMessengerCreateInfo();
        instanceCreateInfo.pNext = &debugCreateInfo;
    }

    VkInstance instance = VK_NULL_HANDLE;
    VK_CHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &instance));

    return instance;
}

VkDebugUtilsMessengerEXT CreateDebugMessenger(VkInstance instance)
{
    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;

    const VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo = MakeDebugMessengerCreateInfo();

    PFN_vkCreateDebugUtilsMessengerEXT createFuntion = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    if (!createFuntion)
    {
        throw std::runtime_error("Get PFN_vkCreateDebugUtilsMessengerEXT failed.\n");
    }

    VK_CHECK(createFuntion(instance, &messengerCreateInfo, nullptr, &messenger));

    return messenger;
}

void SubmitDebugTestMessage(VkInstance instance)
{
    const PFN_vkSubmitDebugUtilsMessageEXT submitMessage = reinterpret_cast<PFN_vkSubmitDebugUtilsMessageEXT>(
        vkGetInstanceProcAddr(instance, "vkSubmitDebugUtilsMessageEXT"));

    if (submitMessage == nullptr)
    {
        throw std::runtime_error("Failed to load vkSubmitDebugUtilsMessageEXT");
    }

    VkDebugUtilsMessengerCallbackDataEXT data{};
    data.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
    data.pMessageIdName = "RendererStudy.DebugTest";
    data.messageIdNumber = 1;
    data.pMessage = "Debug messenger callback connected.";

    submitMessage(instance, VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
                  VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &data);
}

}; // namespace

VulkanContext::VulkanContext() : instanceHandle_{CreateInstance(), deleter::VkInstanceDeleter{}}
{
    if (kEnableValidation)
    {
        // destroy Function 확보 후 객체 생성
        PFN_vkDestroyDebugUtilsMessengerEXT destroyFunction = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instanceHandle_.Get(), "vkDestroyDebugUtilsMessengerEXT"));
        if (destroyFunction == nullptr)
        {
            throw std::runtime_error("Failed to load vkDestroyDebugUtilsMessengerEXT");
        }

        VkDebugUtilsMessengerEXT messenger = CreateDebugMessenger(instanceHandle_.Get());
        messengerHandle_.Adopt(messenger,
                               deleter::VkDebugUtilsMessengerDeleter{instanceHandle_.Get(), destroyFunction});

        SubmitDebugTestMessage(instanceHandle_.Get());
    }
}

std::uint32_t VulkanContext::GetApiVersion()
{
    std::uint32_t apiVersion = 0;

    VK_CHECK(vkEnumerateInstanceVersion(&apiVersion));

    return apiVersion;
}
