#pragma once

#include "VulkanRuntimeExport.h"

#include "common/ClassTraits.h"
#include "common/VulkanHandle.h"

#include <filesystem>

class ShaderModule
{
  public:
    VULKAN_RUNTIME_API explicit ShaderModule(VkDevice device, const std::filesystem::path &shaderPath);
    VULKAN_RUNTIME_API ~ShaderModule() noexcept;

    VK_NON_COPYABLE(ShaderModule)
    VK_NON_MOVABLE(ShaderModule)

    inline VkShaderModule Get() const noexcept
    {
        return shaderHandle_.Get();
    }

  private:
    ShaderModuleHandle shaderHandle_;
};
