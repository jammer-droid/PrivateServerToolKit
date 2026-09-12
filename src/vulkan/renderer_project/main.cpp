#include <vulkan/vulkan.h>

#include <iostream>
#include <cstdint>

#include "common/VulkanException.h"
#include "common/VulkanHandle.h"

int main()
{
    try
    {
        std::uint32_t apiVersion = 0;
        VK_CHECK(vkEnumerateInstanceVersion(&apiVersion));
        std::cout << VK_API_VERSION_MAJOR(apiVersion) << '.' << VK_API_VERSION_MINOR(apiVersion) << '.'
                  << VK_API_VERSION_PATCH(apiVersion) << '\n';

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
        VulkanHandle<VkInstance, deleter::VkInstanceDeleter> instanceHandle(instance, deleter::VkInstanceDeleter{});
    }
    catch (const VulkanException &error)
    {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    };

    return 0;
}
