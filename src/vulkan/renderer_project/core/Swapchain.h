#pragma once

#include "common/VulkanHeaders.h"
#include "common/VulkanHandle.h"
#include "core/VulkanContext.h"

#include <cstdint>
#include <vector>
#include <deque>

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

    VK_NON_COPYABLE(Swapchain)
    VK_NON_MOVABLE(Swapchain)

    inline VkSwapchainKHR GetSwapchain() const noexcept
    {
        return swapchainHandle_.Get();
    }
    inline std::uint32_t GetImageCount() const noexcept
    {
        return static_cast<std::uint32_t>(images_.size());
    }
    inline VkImage GetImage(std::uint32_t index) const
    {
        return images_.at(index);
    }
    inline VkImageView GetImageView(std::uint32_t index) const
    {
        return imageViews_.at(index).Get();
    }
    inline const SwapchainSettings &GetSettings() const noexcept
    {
        return setting_;
    }
    inline VkSemaphore GetRenderFinished(std::uint32_t imageIndex) const
    {
        return renderFinishedHandles_.at(imageIndex).Get();
    }

  public:
    static SwapchainSupport QuerySwapchainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
    static void PrintSwapchainSupport(const SwapchainSupport &swapchainSupport, VkExtent2D framebufferSize);
    static bool ConfigureSwapchainSettings(const SwapchainSupport &support, VkExtent2D framebuffer,
                                           SwapchainSettings *outSetting);

  private:
    SwapchainSettings setting_;
    SwapchainHandle swapchainHandle_;
    std::vector<VkImage> images_; // image는 VkSwapchain 소속
    // vector 재할당 과정에서 ImageViewHandle의 이동/복사가 발생할 수 있어 deque 사용
    std::deque<ImageViewHandle> imageViews_;
    // imageCount 개수와 동일하게 생성
    // Graphics 제출 Fence가 signaled가 되어도, Present에서 사용하는 semaphore 사용 완료를 보장하지 않음
    std::deque<SemaphoreHandle> renderFinishedHandles_;
};
