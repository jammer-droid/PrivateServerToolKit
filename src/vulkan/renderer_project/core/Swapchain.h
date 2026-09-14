#pragma once

#include "common/VulkanHeaders.h"
#include "common/VulkanHandle.h"
#include "core/VulkanContext.h"

#include <cstdint>
#include <vector>

// 선택한 GPU가 선택한 Surface에 출력할 수 있는 이미지 구성 조건
struct SwapchainSupport
{
    VkSurfaceCapabilitiesKHR capabilities{};        // 이미지 개수, 크기, 용도
    std::vector<VkSurfaceFormatKHR> surfaceFormats; // 이미지의 픽셀 포맷, color space 조건
    std::vector<VkPresentModeKHR> presentModes;     // 화면 출력 방식
};

// Swapchain 생성에 사용할 설정값
struct SwapchainSettings
{
    VkSurfaceFormatKHR surfaceFormat{};
    VkPresentModeKHR presentMode{};
    VkExtent2D extent{};
    std::uint32_t minRequiredImageCount{};
};

class Swapchain
{
  public:
    explicit Swapchain(VkDevice device, VkSurfaceKHR surface, SwapchainSettings setting,
                       PhysicalDeviceSelection selection, VkSurfaceCapabilitiesKHR surfaceCapabilities);
    ~Swapchain() = default;

  public:
    static SwapchainSupport QuerySwapchainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
    static void PrintSwapchainSupport(const SwapchainSupport &swapchainSupport, VkExtent2D framebufferSize);
    static bool ConfigureSwapchainSettings(const SwapchainSupport &support, VkExtent2D framebuffer,
                                           SwapchainSettings *outSetting);

  private:
    using SwapchainHandle = VulkanHandle<VkSwapchainKHR, deleter::VkSwapchainDeleter>;

    SwapchainSettings setting_;
    SwapchainHandle swapchainHandle_;
    std::vector<VkImage> images_; // image는 VkSwapchain 소속
};
