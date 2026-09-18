#pragma once

#include "common/VulkanHeaders.h"
#include "common/VulkanHandle.h"

#include <deque>

class FrameResources
{
  public:
    explicit FrameResources(VkDevice device, std::uint32_t queueFamilyIndex);
    // explicit FrameResources(VkDevice device, std::uint32_t queueFamilyIndex, VkPhysicalDevice physicalDevice,
    //                         VkDeviceSize capacityBytes);

    ~FrameResources() = default;

    VK_NON_COPYABLE(FrameResources)
    VK_NON_MOVABLE(FrameResources)

    inline VkCommandBuffer GetCommandBuffer(std::uint32_t index) const noexcept
    {
        return commandBuffers_[index];
    }
    inline VkFence GetFence(std::uint32_t index) const noexcept
    {
        return fenceHandles_.at(index).Get();
    }
    inline VkSemaphore GetImageAvailable(std::uint32_t index) const noexcept
    {
        return imageAvailableSemaphoreHandles_.at(index).Get();
    }
    inline void AdvanceFrameIndex() noexcept
    {
        frameIndex++;
    }
    static inline const std::uint32_t FramesInFlight() noexcept
    {
        return kFramesInFlight;
    }

    std::uint32_t GetFrameIndex() const noexcept;

  private:
    static const std::uint32_t kFramesInFlight = 2;

    CommandPoolHandle commandPoolHandle_;
    // CommandBuffer는 CommandPool 소속으로 Pool의 라이프사이클에 종속됨
    // Primary Buffer로 Graphics Queue에 명령을 준비하기 위한 용도
    VkCommandBuffer commandBuffers_[kFramesInFlight]{
        VK_NULL_HANDLE,
    };

    std::deque<FenceHandle> fenceHandles_;
    std::deque<SemaphoreHandle> imageAvailableSemaphoreHandles_;

    std::uint32_t frameIndex = 0;
};
