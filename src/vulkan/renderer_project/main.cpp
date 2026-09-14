#include <vulkan/vulkan.h>

#include <iostream>

#include "common/VulkanException.h"
#include "common/VulkanHandle.h"

#include "core/VulkanContext.h"
#include "core/Swapchain.h"

#include "app/Window.h"

#include <vulkan/vk_enum_string_helper.h>

using SurfaceHandle = VulkanHandle<VkSurfaceKHR, deleter::VkSurfaceDeleter>;

namespace
{

void PrintExtent2D(VkExtent2D extent)
{
    std::cout << "Width " << extent.width << " x "
              << "Height " << extent.height;
}

}; // namespace

int main()
{
    try
    {
        Window window;
        VulkanContext context(window.GetRequiredInstanceExtensions());
        SurfaceHandle surfaceHandle(window.CreateSurface(context.GetInstance()),
                                    deleter::VkSurfaceDeleter{context.GetInstance()});

        const PhysicalDeviceSelection selection = context.SelectPhysicalDevice(surfaceHandle.Get());
        VkPhysicalDeviceProperties selectedProperties{};
        vkGetPhysicalDeviceProperties(selection.physicalDevice, &selectedProperties);
        std::cout << "Selected GPU: " << selectedProperties.deviceName
                  << "\nGraphics family: " << selection.graphicsFamilyIndex
                  << "\nPresent family: " << selection.presentFamilyIndex
                  << "\nRequires portability subset: " << std::boolalpha << selection.requiresPortabilitySubset << '\n';

        context.InitializeDevice(selection); // Initialize Logical Device

        SwapchainSupport swapchainSupport = QuerySwapchainSupport(selection.physicalDevice, surfaceHandle.Get());
        VkExtent2D framebufferSize = window.GetFramebufferSize();

        std::cout << "Image Count: " << swapchainSupport.capabilities.minImageCount << " ~ "
                  << swapchainSupport.capabilities.maxImageCount << '\n';
        std::cout << "Current VkExtent2D: ";
        PrintExtent2D(swapchainSupport.capabilities.currentExtent);
        std::cout << "\n ExtentRange min: ";
        PrintExtent2D(swapchainSupport.capabilities.minImageExtent);
        std::cout << "\n ExtentRange max: ";
        PrintExtent2D(swapchainSupport.capabilities.maxImageExtent);
        std::cout << "\n Framebuffer: ";
        PrintExtent2D(framebufferSize);
        std::cout << "\nColorAttachmentUsage: " << std::boolalpha
                  << ((swapchainSupport.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0)
                  << '\n';
        std::cout << "Formats\n";
        for (const VkSurfaceFormatKHR &format : swapchainSupport.surfaceFormats)
        {
            std::cout << string_VkFormat(format.format) << " | " << string_VkColorSpaceKHR(format.colorSpace) << '\n';
        }
        std::cout << "PresentModes\n";
        for (const VkPresentModeKHR &presentMode : swapchainSupport.presentModes)
        {
            std::cout << string_VkPresentModeKHR(presentMode) << '\n';
        }

        SwapchainSettings swapchainSetting{};
        bool configResult = ConfigureSwapchainSettings(swapchainSupport, framebufferSize, &swapchainSetting);
        if (!configResult)
        {
            throw std::runtime_error("Configure SwapchainSettings failed\n");
        }

        while (!window.IsCloseRequested())
        {
            window.WaitEvents();
        }
    }
    catch (const VulkanException &error)
    {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    catch (const std::runtime_error &error)
    {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }

    return 0;
}
