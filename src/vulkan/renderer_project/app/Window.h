#pragma once

#include "common/ClassTraits.h"

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

  private:
    GLFWwindow *window_{nullptr};
};
