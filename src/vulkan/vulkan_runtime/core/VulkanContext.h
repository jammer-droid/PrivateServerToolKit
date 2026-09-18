#pragma once

#include "common/ClassTraits.h"
#include "common/VulkanHandle.h"

#include <cstdint>
#include <vector>

struct PhysicalDeviceSelection
{
    VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
    std::uint32_t graphicsFamilyIndex{0};
    std::uint32_t presentFamilyIndex{0};
    bool requiresPortabilitySubset = false;
};

class VulkanContext
{
  public:
    VulkanContext();
    explicit VulkanContext(const std::vector<const char *> &requiredInstanceExtensions);
    ~VulkanContext() noexcept;

    VK_NON_COPYABLE(VulkanContext)
    VK_NON_MOVABLE(VulkanContext)

    inline VkInstance GetInstance() const noexcept
    {
        return instanceHandle_.Get();
    }

    inline VkDevice GetDevice() const noexcept
    {
        return deviceHandle_.Get();
    }
    VkQueue GetGraphicsQueue() const noexcept
    {
        return graphicsQueue_;
    }
    VkQueue GetPresentQueue() const noexcept
    {
        return presentQueue_;
    }

  public:
    static std::uint32_t GetApiVersion();

    PhysicalDeviceSelection SelectPhysicalDevice(VkSurfaceKHR surface) const;

    void InitializeDevice(const PhysicalDeviceSelection &selection);

  private:
    InstanceHandle instanceHandle_;
    DebugMessengerHandle messengerHandle_;

    DeviceHandle deviceHandle_;
    // Queue는 Device가 소유하기 때문에 RAII 래퍼 불필요
    VkQueue graphicsQueue_{VK_NULL_HANDLE};
    VkQueue presentQueue_{VK_NULL_HANDLE};
};
