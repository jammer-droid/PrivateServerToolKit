#pragma once

#include "common/ClassTraits.h"
#include "common/VulkanHeaders.h"

#include <type_traits>
#include <utility>
#include <cassert>

namespace deleter
{
struct VkInstanceDeleter
{
    void operator()(VkInstance handle) const noexcept
    {
        vkDestroyInstance(handle, nullptr);
    }
};

struct VkDebugUtilsMessengerDeleter
{
    VkInstance instance_;
    PFN_vkDestroyDebugUtilsMessengerEXT destroy_;

    void operator()(VkDebugUtilsMessengerEXT handle) const noexcept
    {
        destroy_(instance_, handle, nullptr);
    }
};

struct VkSurfaceDeleter
{
    VkInstance instance_;

    void operator()(VkSurfaceKHR handle) const noexcept
    {
        vkDestroySurfaceKHR(instance_, handle, nullptr);
    }
};

struct VkDeviceDeleter
{
    void operator()(VkDevice handle) const noexcept
    {
        vkDestroyDevice(handle, nullptr);
    }
};

struct VkSwapchainDeleter
{
    VkDevice device;

    void operator()(VkSwapchainKHR handle) const noexcept
    {
        vkDestroySwapchainKHR(device, handle, nullptr);
    }
};

struct VkImageViewDeleter
{
    VkDevice device;

    void operator()(VkImageView handle) const noexcept
    {
        vkDestroyImageView(device, handle, nullptr);
    }
};

struct VkCommandPoolDeleter
{
    VkDevice device;

    void operator()(VkCommandPool handle) const noexcept
    {
        vkDestroyCommandPool(device, handle, nullptr);
    }
};

struct VkFenceDeleter
{
    VkDevice device;

    void operator()(VkFence handle) const noexcept
    {
        vkDestroyFence(device, handle, nullptr);
    }
};

struct VkSemaphoreDeleter
{
    VkDevice device;

    void operator()(VkSemaphore handle) const noexcept
    {
        vkDestroySemaphore(device, handle, nullptr);
    }
};

struct VkShaderModuleDeleter
{
    VkDevice device;

    void operator()(VkShaderModule handle) const noexcept
    {
        vkDestroyShaderModule(device, handle, nullptr);
    }
};

struct VkPipelineLayoutDeleter
{
    VkDevice device;

    void operator()(VkPipelineLayout handle) const noexcept
    {
        vkDestroyPipelineLayout(device, handle, nullptr);
    }
};

struct VkPipelineDeleter
{
    VkDevice device;

    void operator()(VkPipeline handle) const noexcept
    {
        vkDestroyPipeline(device, handle, nullptr);
    }
};

struct VkBufferDeleter
{
    VkDevice device;
    void operator()(VkBuffer handle) const noexcept
    {
        vkDestroyBuffer(device, handle, nullptr);
    }
};

struct VkDeviceMemoryDeleter
{
    VkDevice device;
    void operator()(VkDeviceMemory handle) const noexcept
    {
        vkFreeMemory(device, handle, nullptr);
    }
};

}; // namespace deleter

// VDELETER : 람다식.(C++ 17)
// - 복사 생성 : 캡처한 멤버가 복사 가능하면 가능
// - 이동 생성 : 캡처한 멤버가 이동 생성 가능하면 가능
// - 복사 대입 : 금지
// - 이동 대입 : 지원하지 않음
template <typename VHANDLE, typename VDELETER> class VulkanHandle
{
  public:
    explicit VulkanHandle(VHANDLE handle, VDELETER deleter) noexcept : handle_{handle}, deleter_{std::move(deleter)}
    {
        static_assert(std::is_nothrow_move_constructible_v<VDELETER>);
    }
    VulkanHandle() noexcept = default;

    VK_NON_COPYABLE(VulkanHandle)
    VK_NON_MOVABLE(VulkanHandle)

    ~VulkanHandle() noexcept
    {
        if (handle_ != VHANDLE{})
        {
            deleter_(handle_);
        }
    }

    void Adopt(VHANDLE handle, VDELETER deleter) noexcept
    {
        static_assert(std::is_nothrow_move_assignable_v<VDELETER>);

        assert(handle_ == VHANDLE{});
        assert(handle != VHANDLE{});

        deleter_ = std::move(deleter);
        handle_ = handle;
    }

    VHANDLE Get() const noexcept
    {
        return handle_;
    }

  private:
    VHANDLE handle_{};
    VDELETER deleter_{};
};

using SurfaceHandle = VulkanHandle<VkSurfaceKHR, deleter::VkSurfaceDeleter>;
using InstanceHandle = VulkanHandle<VkInstance, deleter::VkInstanceDeleter>;
using DebugMessengerHandle = VulkanHandle<VkDebugUtilsMessengerEXT, deleter::VkDebugUtilsMessengerDeleter>;
using DeviceHandle = VulkanHandle<VkDevice, deleter::VkDeviceDeleter>;
using SwapchainHandle = VulkanHandle<VkSwapchainKHR, deleter::VkSwapchainDeleter>;
using ImageViewHandle = VulkanHandle<VkImageView, deleter::VkImageViewDeleter>;
using CommandPoolHandle = VulkanHandle<VkCommandPool, deleter::VkCommandPoolDeleter>;
using FenceHandle = VulkanHandle<VkFence, deleter::VkFenceDeleter>;
using SemaphoreHandle = VulkanHandle<VkSemaphore, deleter::VkSemaphoreDeleter>;
using ShaderModuleHandle = VulkanHandle<VkShaderModule, deleter::VkShaderModuleDeleter>;
using PipelineLayoutHandle = VulkanHandle<VkPipelineLayout, deleter::VkPipelineLayoutDeleter>;
using PipelineHandle = VulkanHandle<VkPipeline, deleter::VkPipelineDeleter>;
using BufferHandle = VulkanHandle<VkBuffer, deleter::VkBufferDeleter>;
using DeviceMemoryHandle = VulkanHandle<VkDeviceMemory, deleter::VkDeviceMemoryDeleter>;
