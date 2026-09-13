#pragma once

#include "common/ClassTraits.h"
#include "common/VulkanHandle.h"

#include <cstdint>
#include <vector>

class VulkanContext
{
  public:
    VulkanContext();
    explicit VulkanContext(const std::vector<const char *> &requiredInstanceExtensions);
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
