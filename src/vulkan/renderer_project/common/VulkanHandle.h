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
