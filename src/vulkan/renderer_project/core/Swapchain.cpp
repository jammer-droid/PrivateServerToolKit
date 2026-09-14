#include "Swapchain.h"

#include "common/VulkanException.h"

#include <cstdint>

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
