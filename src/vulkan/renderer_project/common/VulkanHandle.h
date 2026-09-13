#pragma once

#include "common/VulkanHeaders.h"

#include <type_traits>
#include <utility>

namespace deleter
{
struct VkInstanceDeleter
{
    void operator()(VkInstance handle) noexcept
    {
        vkDestroyInstance(handle, nullptr);
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

    VK_NON_COPYABLE(VulkanHandle)
    VK_NON_MOVABLE(VulkanHandle)

    ~VulkanHandle() noexcept
    {
        if (handle_ != VHANDLE{})
        {
            deleter_(handle_);
        }
    }

    VHANDLE Get() const noexcept
    {
        return handle_;
    }

  private:
    VHANDLE handle_;
    VDELETER deleter_;
};
