#pragma once

#include "VulkanRuntimeExport.h"

#include "common/ClassTraits.h"
#include "common/VulkanHeaders.h"
#include "common/VulkanHandle.h"

#include <cstddef>

class HostVisibleBuffer
{
  public:
    VULKAN_RUNTIME_API explicit HostVisibleBuffer(VkPhysicalDevice physicalDevice, VkDevice device,
                                                  VkDeviceSize capacityBytes);

    VULKAN_RUNTIME_API ~HostVisibleBuffer() noexcept;

    VK_NON_COPYABLE(HostVisibleBuffer)
    VK_NON_MOVABLE(HostVisibleBuffer)

    inline VkBuffer GetBuffer() const noexcept
    {
        return buffer_.Get();
    }
    inline VkDeviceSize GetCapacityBytes() const noexcept
    {
        return capacityBytes_;
    }

    VULKAN_RUNTIME_API void Write(const void *data, std::size_t byteCount);

  private:
    VkDevice device_;
    VkDeviceSize capacityBytes_;

    DeviceMemoryHandle memory_;
    BufferHandle buffer_;
    void *mapped_{nullptr};
};
