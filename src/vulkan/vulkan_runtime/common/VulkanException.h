#pragma once

#include "VulkanRuntimeExport.h"

#include "common/VulkanHeaders.h"

#include <stdexcept>
#include <string>

#define VK_CHECK(expression) VulkanException::CheckVulkanResult((expression), #expression, __FILE__, __LINE__)

// Export the exception type as well as its methods for cross-library RTTI/catch.
class VULKAN_RUNTIME_API VulkanException : public std::runtime_error
{
  public:
    VulkanException(VkResult result, const std::string &msg = "Vulkan Error");

    ~VulkanException() noexcept override;

    static void CheckVulkanResult(VkResult result, const char *expression, const char *file, int line);

    const char *what() const noexcept override;
    VkResult Result() const noexcept;

  private:
    VkResult result_;
    std::string errorMessage_;
};
