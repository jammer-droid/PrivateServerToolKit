#include "Swapchain.h"

#include "common/VulkanException.h"

#include <vulkan/vk_enum_string_helper.h>

#include <cstdint>
#include <algorithm>
#include <iostream>

namespace
{

std::vector<VkSurfaceFormatKHR> EnumerateSurfaceFormats(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    std::uint32_t surfaceFormatCount = 0;
    std::vector<VkSurfaceFormatKHR> surfaceFormats;

    while (true)
    {
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr));

        if (surfaceFormatCount == 0)
        {
            throw std::runtime_error("No SurfaceFormat Found.\n");
        }

        surfaceFormats.resize(surfaceFormatCount);
        VkResult result =
            vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, surfaceFormats.data());
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

std::vector<VkPresentModeKHR> EnumeratePresentModes(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    std::uint32_t presentModeCount = 0;
    std::vector<VkPresentModeKHR> presentModes;

    while (true)
    {
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr));

        if (presentModeCount == 0)
        {
            throw std::runtime_error("No PresentMode Found.\n");
        }

        presentModes.resize(presentModeCount);
        VkResult result =
            vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());
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

void PrintExtent2D(VkExtent2D extent)
{
    std::cout << "Width " << extent.width << " x "
              << "Height " << extent.height;
}

} // namespace

void Swapchain::PrintSwapchainSupport(const SwapchainSupport &swapchainSupport, VkExtent2D framebufferSize)
{
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
}

SwapchainSupport Swapchain::QuerySwapchainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    SwapchainSupport support;

    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &support.capabilities));

    support.surfaceFormats = EnumerateSurfaceFormats(physicalDevice, surface);
    support.presentModes = EnumeratePresentModes(physicalDevice, surface);

    return support;
}

bool Swapchain::ConfigureSwapchainSettings(const SwapchainSupport &support, VkExtent2D framebuffer,
                                           SwapchainSettings *outSetting)
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

Swapchain::Swapchain(VkDevice device, VkSurfaceKHR surface, SwapchainSettings setting,
                     PhysicalDeviceSelection selection, VkSurfaceCapabilitiesKHR surfaceCapabilities)
    : setting_{setting}
{
    VkSwapchainCreateInfoKHR swapchainCreateInfo{};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = surface;

    swapchainCreateInfo.minImageCount = setting.minRequiredImageCount;
    swapchainCreateInfo.imageFormat = setting.surfaceFormat.format;
    swapchainCreateInfo.imageColorSpace = setting.surfaceFormat.colorSpace;
    swapchainCreateInfo.imageExtent = setting.extent;
    swapchainCreateInfo.presentMode = setting.presentMode;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    const std::uint32_t queueFamilyIndices[2] = {selection.graphicsFamilyIndex, selection.presentFamilyIndex};
    if (selection.graphicsFamilyIndex == selection.presentFamilyIndex)
    {
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainCreateInfo.queueFamilyIndexCount = 0;
        swapchainCreateInfo.pQueueFamilyIndices = nullptr;
    }
    else
    {
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainCreateInfo.queueFamilyIndexCount = 2;
        swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
    }

    // OPAQUE → PRE_MULTIPLIED → POST_MULTIPLIED → INHERIT
    constexpr std::uint32_t requiredCompositeAlphaCount = 4;
    const VkCompositeAlphaFlagBitsKHR orderedRequiredCompositeAlphas[requiredCompositeAlphaCount] = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR};

    for (std::uint32_t i = 0; i < requiredCompositeAlphaCount; i++)
    {
        if ((orderedRequiredCompositeAlphas[i] & surfaceCapabilities.supportedCompositeAlpha) != 0)
        {
            swapchainCreateInfo.compositeAlpha = orderedRequiredCompositeAlphas[i];
            break;
        }
    }

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain));

    swapchainHandle_.Adopt(swapchain, deleter::VkSwapchainDeleter{device});

    std::uint32_t imageCount = 0;
    while (true)
    {
        VK_CHECK(vkGetSwapchainImagesKHR(device, swapchainHandle_.Get(), &imageCount, nullptr));

        if (imageCount == 0)
        {
            throw std::runtime_error("Swapchain has no Images\n");
        }

        images_.resize(imageCount);
        VkResult result = vkGetSwapchainImagesKHR(device, swapchainHandle_.Get(), &imageCount, images_.data());
        if (result == VK_INCOMPLETE)
        {
            images_.clear();
            continue;
        }

        VK_CHECK(result);
        if (imageCount == 0)
        {
            throw std::runtime_error("Swapchain has no Images\n");
        }

        images_.resize(imageCount);
        break;
    }
}
