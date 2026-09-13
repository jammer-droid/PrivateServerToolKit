#include "VulkanContext.h"

#include "common/VulkanException.h"

namespace
{
VkInstance CreateInstance()
{
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "RendererProject";
    appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;
    appInfo.pEngineName = "Custom";
    appInfo.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);

    VkInstanceCreateInfo instanceCreateInfo{};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;

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
