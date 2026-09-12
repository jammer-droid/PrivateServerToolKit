#include "VulkanException.h"

VulkanException::VulkanException(VkResult result, const std::string &msg) : result_{result}, std::runtime_error{msg}
{
    errorMessage_ = std::string(std::runtime_error::what() + std::string(" : ") + std::to_string(result));
}

void VulkanException::CheckVulkanResult(VkResult result, const char *expression, const char *file, int line)
{
    if (result != VK_SUCCESS)
    {
        std::string msg = std::string(file) + std::string(", " + std::to_string(line) + " ") + std::string(expression);
        throw VulkanException(result, msg);
    }
}

const char *VulkanException::what() const noexcept
{
    return errorMessage_.c_str();
}

VkResult VulkanException::Result() const noexcept
{
    return result_;
}
