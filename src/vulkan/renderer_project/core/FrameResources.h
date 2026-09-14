#pragma once

#include "common/VulkanHeaders.h"
#include "common/VulkanHandle.h"

class FrameResources
{
  public:
    explicit FrameResources(VkDevice device, std::uint32_t queueFamilyIndex);
    ~FrameResources() = default;

    VK_NON_COPYABLE(FrameResources)
    VK_NON_MOVABLE(FrameResources)

    inline VkCommandBuffer GetCommandBuffer() const noexcept
    {
        return commandBuffer_;
    }
    inline VkFence GetFence() const noexcept
    {
        return fenceHandle_.Get();
    }

  private:
    // CommandPool, Fence RAII handle
    using CommandPoolHandle = VulkanHandle<VkCommandPool, deleter::VkCommandPoolDeleter>;
    using FenceHandle = VulkanHandle<VkFence, deleter::VkFenceDeleter>;

    CommandPoolHandle commandPoolHandle_;
    VkCommandBuffer commandBuffer_{VK_NULL_HANDLE}; // CommandBuffer는 CommandPool 소속으로 Pool의 라이프사이클에 종속됨

    FenceHandle fenceHandle_;
};
