#include "Window.h"

#define GLFW_INCLUDE_NONE // GLFW 헤더가 OpenGL 헤더 자동 include하는 것을 방지
#include <GLFW/glfw3.h>

#include <cstdio>
#include <stdexcept>
#include <cstdint>

#include "common/VulkanException.h"

namespace
{

// GLFW 오류를 받기 위한 콜백 함수
void GlfwErrorCallback(int errorCode, const char *description)
{
    std::fprintf(stderr, "[GLFW] error %d: %s\n", errorCode, description);
}

}; // namespace

Window::Window()
{
    // 1: 오류 callback 등록
    glfwSetErrorCallback(GlfwErrorCallback);

    // 2: GLFW 초기화
    if (glfwInit() == GLFW_FALSE)
    {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    // 3: 창 옵션 설정
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // No OpenGL Context
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);    // 사용자가 창크기 변경 가능

    // 4: 실제 창 생성
    window_ = glfwCreateWindow(1280, 720, "renderer project", nullptr, nullptr);

    if (window_ == nullptr)
    {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window.\n");
    }
}

Window::~Window() noexcept
{
    if (window_ != nullptr)
    {
        glfwDestroyWindow(window_);
    }

    glfwTerminate();
}

bool Window::IsCloseRequested() const
{
    // 닫기 요청시 GLFW가 shouldClose 플래그를 활성화함
    return glfwWindowShouldClose(window_) == GLFW_TRUE;
}

void Window::WaitEvents() const
{
    // glfw에서 이벤트를 대기
    // 이벤트가 없으면 현재 스레드를 대기하고
    // 이벤트가 도착하면 처리한 뒤 반환
    glfwWaitEvents();
}

std::vector<const char *> Window::GetRequiredInstanceExtensions() const
{
    std::uint32_t count = 0;
    std::vector<const char *> extensionNames;

    // Vulkan에서 GLFW 사용을 위해 필요한 instance extensions 목록 질의
    const char **names = glfwGetRequiredInstanceExtensions(&count);

    if (names == nullptr)
    {
        throw std::runtime_error("Failed to glfwGetRequiredInstanceExtensions.\n");
    }

    for (std::uint32_t index = 0; index < count; index++)
    {
        extensionNames.push_back(names[index]);
    }

    return extensionNames;
}

VkSurfaceKHR Window::CreateSurface(VkInstance instance) const
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    VK_CHECK(glfwCreateWindowSurface(instance, window_, nullptr, &surface));

    return surface;
}
