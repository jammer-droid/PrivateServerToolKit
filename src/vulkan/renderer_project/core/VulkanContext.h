#pragma once

#include "common/ClassTraits.h"
#include "common/VulkanHandle.h"

#include <cstdint>

class VulkanContext
{
  public:
    VulkanContext();
    ~VulkanContext() = default;

    VK_NON_COPYABLE(VulkanContext)
    VK_NON_MOVABLE(VulkanContext)

    inline VkInstance GetInstance() const noexcept
    {
        return instanceHandle_.Get();
    }

  public:
    static std::uint32_t GetApiVersion();

  private:
    using InstanceHandle = VulkanHandle<VkInstance, deleter::VkInstanceDeleter>;
    using DebugMessengerHandle = VulkanHandle<VkDebugUtilsMessengerEXT, deleter::VkDebugUtilsMessengerDeleter>;

    InstanceHandle instanceHandle_;
    DebugMessengerHandle messengerHandle_;
};
