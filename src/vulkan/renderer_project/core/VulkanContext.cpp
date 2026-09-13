#include "VulkanContext.h"

#include "common/VulkanException.h"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace
{

constexpr std::uint32_t kRequiredApiVersion = VK_API_VERSION_1_3;
constexpr const char *kRequiredExtensionName =
    VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME; // MoltenVK 같은 이식용 구현 조회

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
        std::cout << "Extension Count: " << extensionCount << '\n';
        for (const VkExtensionProperties &property : extensionProperties)
        {
            std::cout << property.extensionName << ", " << property.specVersion << '\n';
        }
        break;
    }

    return extensionProperties;
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

#ifdef __APPLE__
    std::vector<VkExtensionProperties> extensionProperties = EnumerateInstanceExtensions();
    std::vector<std::string> extensions;
    for (const VkExtensionProperties &property : extensionProperties)
    {
        if (std::strcmp(property.extensionName, kRequiredExtensionName) == 0)
        {
            extensions.push_back(property.extensionName);
        }
    }

    if (extensions.empty())
    {
        throw std::runtime_error("Required Extension Not Found: " + std::string(kRequiredExtensionName));
    }
    std::vector<const char *> names;
    names.reserve(extensions.size());
    for (const std::string &name : extensions)
    {
        names.push_back(name.c_str());
    }

    // MoltenVK -> Vulkan을 Metal 위에 구현하는 소프트웨어
    //  - PhysicalDevice 조회시 MoltenVK가 Device를 노출함(portability device)
    // 실제 GPU -> Apple GPU HW
    // VkPhysicalDevice -> 앱이 조회하는 GPU 핸들

    // portability device도 VkPhysicalDevice 열거에 포함하는 기능 활성화
    instanceCreateInfo.enabledExtensionCount = std::uint32_t(names.size());
    instanceCreateInfo.ppEnabledExtensionNames = names.data();

    // portability device도 조회 결과에 포함하게 설정
    instanceCreateInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    VkInstance instance = VK_NULL_HANDLE;
    VK_CHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &instance));

    return instance;
}
}; // namespace

VulkanContext::VulkanContext() : instanceHandle_(CreateInstance(), deleter::VkInstanceDeleter{}){};

std::uint32_t VulkanContext::GetApiVersion()
{
    std::uint32_t apiVersion = 0;

    VK_CHECK(vkEnumerateInstanceVersion(&apiVersion));

    return apiVersion;
}
