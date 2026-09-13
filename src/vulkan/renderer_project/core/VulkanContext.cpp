#include "VulkanContext.h"

#include "common/VulkanException.h"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <cstdio>
#include <optional>
#include <array>

namespace
{

constexpr std::uint32_t kRequiredApiVersion = VK_API_VERSION_1_3;
// portability device 열거 기능 활성화
constexpr const char *kRequiredExtensionPortabilityEnumeration = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
constexpr const char *kRequiredExtensionDebugUtils = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
constexpr const char *kRequiredExtensionSwapchain = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
constexpr const char *kRequiredExtensionPorabilitySubset = "VK_KHR_portability_subset";

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

// for Instance Extensions
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

std::vector<VkPhysicalDevice> EnumeratePhysicalDevices(VkInstance instance)
{
    std::uint32_t count = 0;
    std::vector<VkPhysicalDevice> devices;

    while (true)
    {
        VK_CHECK(vkEnumeratePhysicalDevices(instance, &count, nullptr));
        if (count == 0)
        {
            throw std::runtime_error("No Vulkan Physical Devices found.\n");
        }

        devices.resize(count);

        VkResult result = vkEnumeratePhysicalDevices(instance, &count, devices.data());
        if (result == VK_INCOMPLETE)
        {
            devices.clear();
            continue;
        }
        VK_CHECK(result);

        if (count == 0)
        {
            throw std::runtime_error("No Vulkan Physical Devices found.\n");
        }

        devices.resize(count);
        break;
    }

    return devices;
}

// for Physical Device Extensions
std::vector<VkExtensionProperties> EnumerateDeviceExtensions(VkPhysicalDevice physicalDevice)
{
    std::uint32_t extensionCount = 0;
    std::vector<VkExtensionProperties> extensions;

    while (true)
    {
        VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr));
        if (extensionCount == 0)
        {
            break;
        }

        extensions.resize(extensionCount);
        VkResult result =
            vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensions.data());
        if (result == VK_INCOMPLETE)
        {
            extensions.clear();
            continue;
        }
        VK_CHECK(result);
        extensions.resize(extensionCount);
        break;
    }

    return extensions;
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

void AddUniqueExtension(std::vector<const char *> &extensions, const char *name)
{
    for (const char *extension : extensions)
    {
        if (std::strcmp(extension, name) == 0)
        {
            return;
        }
    }

    extensions.push_back(name);
}

VkInstance CreateInstance(const std::vector<const char *> &requiredInstanceExtensions = {})
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

    for (const char *extName : requiredInstanceExtensions)
    {
        AddUniqueExtension(requiredExtensions, extName);
    }

#ifdef __APPLE__
    AddUniqueExtension(requiredExtensions, kRequiredExtensionPortabilityEnumeration);

    // MoltenVK -> Vulkan을 Metal 위에 구현하는 소프트웨어
    //  - PhysicalDevice 조회시 MoltenVK가 Device를 노출함(portability device)
    // 실제 GPU -> Apple GPU HW
    // VkPhysicalDevice -> 앱이 조회하는 GPU 핸들

    // portability device도 조회 결과에 포함하게 설정
    instanceCreateInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    if (kEnableValidation)
    {
        AddUniqueExtension(requiredExtensions, kRequiredExtensionDebugUtils);
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

PFN_vkDestroyDebugUtilsMessengerEXT LoadDestroyDebugMessengerFunction(VkInstance instance)
{
    PFN_vkDestroyDebugUtilsMessengerEXT destroyFunction = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (destroyFunction == nullptr)
    {
        throw std::runtime_error("Failed to load vkDestroyDebugUtilsMessengerEXT");
    }

    return destroyFunction;
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

struct QueueFamilySelection
{
    std::uint32_t graphicsFamilyIndex;
    std::uint32_t presentFamilyIndex;
};

struct QueueFamilySupport
{
    std::uint32_t queueCount;
    bool graphics;
    bool present;
};

// queueCount > 0
// support graphics & present
std::optional<QueueFamilySelection> ChooseQueueFamilies(const std::vector<QueueFamilySupport> &families)
{
    std::optional<std::uint32_t> graphicsFamily;
    std::optional<std::uint32_t> presentFamily;

    for (std::uint32_t index = 0; index < families.size(); ++index)
    {
        const QueueFamilySupport &family = families[index];
        if (family.queueCount == 0)
        {
            continue;
        }
        if (family.graphics && family.present) // 둘 다 지원하는 QueueFamily 우선 선택
        {
            return QueueFamilySelection{index, index};
        }
        if (family.graphics && !graphicsFamily.has_value())
        {
            graphicsFamily = index;
        }
        if (family.present && !presentFamily.has_value())
        {
            presentFamily = index;
        }
    }

    if (!graphicsFamily.has_value() || !presentFamily.has_value())
    {
        return std::nullopt;
    }
    return QueueFamilySelection{graphicsFamily.value(), presentFamily.value()};
}

std::optional<QueueFamilySelection> FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    std::uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    if (count == 0)
    {
        return std::nullopt;
    }

    std::vector<VkQueueFamilyProperties> properties(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, properties.data());
    properties.resize(count);

    std::vector<QueueFamilySupport> families;
    families.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        VkBool32 present = VK_FALSE;
        if (properties[index].queueCount != 0)
        {
            // check support present
            VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface, &present));
        }

        // check support graphics
        bool supportGraphics = (properties[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
        bool supportPresent = (present == VK_TRUE);
        families.push_back(QueueFamilySupport{properties[index].queueCount, supportGraphics, supportPresent});
    }
    return ChooseQueueFamilies(families);
}

}; // namespace

VulkanContext::VulkanContext() : VulkanContext(std::vector<const char *>{})
{
}

VulkanContext::VulkanContext(const std::vector<const char *> &requiredInstanceExtensions)
    : instanceHandle_{CreateInstance(requiredInstanceExtensions), deleter::VkInstanceDeleter{}}
{
    if (kEnableValidation)
    {
        // destroy Function 확보 후 객체 생성
        auto destroyFunction = LoadDestroyDebugMessengerFunction(instanceHandle_.Get());
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

PhysicalDeviceSelection VulkanContext::SelectPhysicalDevice(VkSurfaceKHR surface) const
{
    const std::vector<VkPhysicalDevice> physicalDevices = EnumeratePhysicalDevices(instanceHandle_.Get());

    for (const VkPhysicalDevice device : physicalDevices)
    {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device, &properties);
        std::cout << "Candidate GPU: " << properties.deviceName << " | API: " << FormatApiVersion(properties.apiVersion)
                  << '\n';

        if (properties.apiVersion < kRequiredApiVersion)
        {
            std::cout << "Skipped: requires Vulkan " << FormatApiVersion(kRequiredApiVersion) << '\n';
            continue;
        }

        const std::vector<VkExtensionProperties> extensions = EnumerateDeviceExtensions(device);
        bool supportsSwapchain = false;
        bool requiresPortabilitySubset = false;
        for (const VkExtensionProperties &extension : extensions)
        {
            if (std::strcmp(extension.extensionName, kRequiredExtensionSwapchain) == 0)
            {
                supportsSwapchain = true;
            }
            // portability subset을 device에서 지원하는지 확인
            // 지원하는 경우 portability subset을 활성화해 portability의 제약 조건 하에 실행하도록 설정 필요
            if (std::strcmp(extension.extensionName, kRequiredExtensionPorabilitySubset) == 0)
            {
                requiresPortabilitySubset = true;
            }
        }
        if (!supportsSwapchain)
        {
            std::cout << "Skipped: missing " << kRequiredExtensionSwapchain << '\n';
            continue;
        }

        VkPhysicalDeviceVulkan13Features features13{};
        features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

        VkPhysicalDeviceFeatures2 features2{};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.pNext = &features13;

        vkGetPhysicalDeviceFeatures2(device, &features2);

        if (features13.dynamicRendering != VK_TRUE)
        {
            std::cout << "Skipped: dynamicRendering is not supported\n";
            continue;
        }
        if (features13.synchronization2 != VK_TRUE)
        {
            std::cout << "Skipped: synchronization2 is not supported\n";
            continue;
        }

        // device에서 지원하는 Queue Family를 순회하여 graphics, present를 만족하는 queue 반환
        const std::optional<QueueFamilySelection> queues = FindQueueFamilies(device, surface);
        if (!queues.has_value())
        {
            std::cout << "Skipped: missing usable Graphics or Present queue family\n";
            continue;
        }

        return PhysicalDeviceSelection{device, queues->graphicsFamilyIndex, queues->presentFamilyIndex,
                                       requiresPortabilitySubset};
    }

    throw std::runtime_error("No suitable physical device found");
}

void VulkanContext::InitializeDevice(const PhysicalDeviceSelection &selection)
{
    if (deviceHandle_.Get() != VK_NULL_HANDLE)
    {
        throw std::runtime_error("VkDevice already initialized.\n");
    }

    // Device Feature 목록
    VkPhysicalDeviceVulkan13Features enabledFeatures{};
    enabledFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    enabledFeatures.dynamicRendering = VK_TRUE;
    enabledFeatures.synchronization2 = VK_TRUE;

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &enabledFeatures;

    std::uint32_t queueCreateInfoCount = (selection.graphicsFamilyIndex == selection.presentFamilyIndex) ? 1u : 2u;
    const float queuePriority = 1.0f; // 0.0 ~ 1.0, 1.0이 우선순위가 가장 높음

    std::array<VkDeviceQueueCreateInfo, 2> queueInfos{};
    std::array<std::uint32_t, 2> queueFamilyIndices{selection.graphicsFamilyIndex, selection.presentFamilyIndex};
    for (std::uint32_t index = 0; index < queueCreateInfoCount; index++)
    {
        VkDeviceQueueCreateInfo &info = queueInfos[index];
        info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        info.pQueuePriorities = &queuePriority;
        info.queueFamilyIndex = queueFamilyIndices[index];
        info.queueCount = 1;
    }
    deviceCreateInfo.queueCreateInfoCount = queueCreateInfoCount;
    deviceCreateInfo.pQueueCreateInfos = queueInfos.data();

    // Device Extension 활성화 목록
    std::vector<const char *> deviceExtensions{kRequiredExtensionSwapchain};
    if (selection.requiresPortabilitySubset)
    {
        deviceExtensions.push_back(kRequiredExtensionPorabilitySubset);
    }
    deviceCreateInfo.enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkDevice device = VK_NULL_HANDLE;

    VK_CHECK(vkCreateDevice(selection.physicalDevice, &deviceCreateInfo, nullptr, &device));

    deviceHandle_.Adopt(device, deleter::VkDeviceDeleter{});
    vkGetDeviceQueue(deviceHandle_.Get(), selection.graphicsFamilyIndex, 0, &graphicsQueue_);
    vkGetDeviceQueue(deviceHandle_.Get(), selection.presentFamilyIndex, 0, &presentQueue_);
}
