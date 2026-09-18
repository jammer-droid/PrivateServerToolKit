#pragma once

#include "common/ClassTraits.h"
#include "common/VulkanHeaders.h"
#include "common/VulkanHandle.h"

#include <cstddef>

class HostVisibleBuffer
{
  public:
    explicit HostVisibleBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize capacityBytes);

    ~HostVisibleBuffer() noexcept;

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

    void Write(const void *data, std::size_t byteCount);

  private:
    VkDevice device_;
    VkDeviceSize capacityBytes_;

    DeviceMemoryHandle memory_;
    BufferHandle buffer_;
    void *mapped_{nullptr};
};
