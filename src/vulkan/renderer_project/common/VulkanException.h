#pragma once

#include <vulkan/vulkan_core.h>
#include <stdexcept>
#include <string>

#define VK_CHECK(expression) VulkanException::CheckVulkanResult((expression), #expression, __FILE__, __LINE__)

class VulkanException : public std::runtime_error
{
  public:
    VulkanException(VkResult result, const std::string &msg = "Vulkan Error");

    static void CheckVulkanResult(VkResult result, const char *expression, const char *file, int line);

    const char *what() const noexcept override;
    VkResult Result() const noexcept;

  private:
    VkResult result_;
    std::string errorMessage_;
};
