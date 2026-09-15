#pragma once

#include "common/VulkanHeaders.h"
#include "common/ClassTraits.h"

#include <vector>

struct GLFWwindow;

class Window
{
  public:
    Window();
    ~Window() noexcept;

    VK_NON_COPYABLE(Window)
    VK_NON_MOVABLE(Window)

    bool IsCloseRequested() const;
    void WaitEvents() const;
    void PollEvents() const;

    // GLFW로 Surface를 만들기 위해 필요한 extension name 목록 조회
    std::vector<const char *> GetRequiredInstanceExtensions() const;

    VkSurfaceKHR CreateSurface(VkInstance instance) const;
    void ResetFramebuffer(int width, int height) noexcept;
    inline VkExtent2D GetFramebufferSize() const noexcept
    {
        return framebuffer_;
    }
    inline bool HasFramebufferResized() const noexcept
    {
        return framebufferResized_;
    }
    inline void ClearFramebufferResized() noexcept
    {
        framebufferResized_ = false;
    }

  private:
    void EnableFramebufferResizeTracking() noexcept;

  private:
    GLFWwindow *window_{nullptr};

    bool framebufferResized_{false};
    VkExtent2D framebuffer_{};
};
