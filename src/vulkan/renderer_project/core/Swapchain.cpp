#include "Swapchain.h"

#include "common/VulkanException.h"

#include <cstdint>
#include <algorithm>
#include <iostream>

namespace
{

std::vector<VkSurfaceFormatKHR> EnumerateSurfaceFormats(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    std::uint32_t surfaceFormatCount = 0;
    std::vector<VkSurfaceFormatKHR> surfaceFormats;

    while (true)
    {
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &surfaceFormatCount, nullptr));

        if (surfaceFormatCount == 0)
        {
            throw std::runtime_error("No SurfaceFormat Found.\n");
        }

        surfaceFormats.resize(surfaceFormatCount);
        VkResult result =
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &surfaceFormatCount, surfaceFormats.data());
        if (result == VK_INCOMPLETE)
        {
            surfaceFormats.clear();
            continue;
        }

        VK_CHECK(result);
        if (surfaceFormatCount == 0)
        {
            throw std::runtime_error("No SurfaceFormat Found.\n");
        }

        surfaceFormats.resize(surfaceFormatCount);
        break;
    }

    return surfaceFormats;
};

std::vector<VkPresentModeKHR> EnumeratePresentModes(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    std::uint32_t presentModeCount = 0;
    std::vector<VkPresentModeKHR> presentModes;

    while (true)
    {
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr));

        if (presentModeCount == 0)
        {
            throw std::runtime_error("No PresentMode Found.\n");
        }

        presentModes.resize(presentModeCount);
        VkResult result =
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, presentModes.data());
        if (result == VK_INCOMPLETE)
        {
            presentModes.clear();
            continue;
        }

        VK_CHECK(result);
        if (presentModeCount == 0)
        {
            throw std::runtime_error("No PresentMode Found.\n");
        }

        presentModes.resize(presentModeCount);
        break;
    }

    return presentModes;
}

} // namespace

SwapchainSupport QuerySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    SwapchainSupport support;

    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &support.capabilities));

    support.surfaceFormats = EnumerateSurfaceFormats(device, surface);
    support.presentModes = EnumeratePresentModes(device, surface);

    return support;
}

bool ConfigureSwapchainSettings(const SwapchainSupport &support, VkExtent2D framebuffer, SwapchainSettings *outSetting)
{
    if (outSetting == nullptr)
    {
        return false;
    }

    SwapchainSettings setting{};

    constexpr std::uint32_t requiredFormatCount = 2;
    VkFormat requiredFormats[requiredFormatCount] = {VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB};
    VkColorSpaceKHR requiredColorSpaces = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkPresentModeKHR requiredPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    VkImageUsageFlags requiredImageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // setting: check image usage
    if ((support.capabilities.supportedUsageFlags & requiredImageUsageFlags) == 0)
    {
        throw std::runtime_error("SwapchainSupport not support required ImageUsageFlags");
    }

    // setting: SurfaceFormat
    bool supportRequiredFormat = false;
    bool supportRequiredColorSpace = false;
    for (const VkSurfaceFormatKHR &surfaceFormat : support.surfaceFormats)
    {
        // VkFormat
        for (std::uint32_t i = 0; i < requiredFormatCount; i++)
        {
            if (requiredFormats[i] == surfaceFormat.format)
            {
                supportRequiredFormat = true;
                break;
            }
        }

        // VkColorSpace
        if (supportRequiredFormat && surfaceFormat.colorSpace == requiredColorSpaces)
        {
            supportRequiredColorSpace = true;
        }

        if (supportRequiredFormat && supportRequiredColorSpace)
        {
            setting.surfaceFormat = surfaceFormat;
            break;
        }

        supportRequiredFormat = false;
        supportRequiredColorSpace = false;
    }

    if (!supportRequiredFormat || !supportRequiredColorSpace)
    {
        throw std::runtime_error("Required SurfaceFormat not found\n");
    }

    // setting: PresentMode
    bool supportPresentMode = false;
    for (const VkPresentModeKHR &presentMode : support.presentModes)
    {
        if (presentMode == requiredPresentMode)
        {
            supportPresentMode = true;
            setting.presentMode = presentMode;
            break;
        }
    }

    if (!supportPresentMode)
    {
        throw std::runtime_error("Required PresentMode not found\n");
    }

    // setting: Extent

    if (framebuffer.width == 0 || framebuffer.height == 0)
    {
        setting.extent = VkExtent2D{0, 0};
        return false;
    }

    VkExtent2D curExt = support.capabilities.currentExtent;
    if (curExt.width == 0 || curExt.height == 0)
    {
        setting.extent = VkExtent2D{0, 0};
        return false;
    }
    else if (curExt.width == UINT32_MAX || curExt.height == UINT32_MAX)
    {
        std::cout << "SwapchainSupport's currentExtent has UINT32_MAX. Use intput framebuffer instead.";
        VkExtent2D minExt = support.capabilities.minImageExtent;
        VkExtent2D maxExt = support.capabilities.maxImageExtent;
        VkExtent2D ext{std::clamp(framebuffer.width, minExt.width, maxExt.width),
                       std::clamp(framebuffer.height, minExt.height, maxExt.height)};
        setting.extent = ext;
    }
    else
    {
        setting.extent = support.capabilities.currentExtent;
    }

    if (setting.extent.width == 0 || setting.extent.height == 0)
    {
        setting.extent = VkExtent2D{0, 0};
        return false;
    }

    // setting: required image count
    setting.minRequiredImageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount != 0)
    {
        setting.minRequiredImageCount = std::min(support.capabilities.maxImageCount, setting.minRequiredImageCount);
    }

    *outSetting = setting;
    return true;
}
