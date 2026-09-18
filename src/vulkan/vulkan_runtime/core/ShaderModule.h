#pragma once

#include "common/ClassTraits.h"
#include "common/VulkanHandle.h"

#include <filesystem>

class ShaderModule
{
  public:
    explicit ShaderModule(VkDevice device, const std::filesystem::path &shaderPath);
    ~ShaderModule() noexcept;

    VK_NON_COPYABLE(ShaderModule)
    VK_NON_MOVABLE(ShaderModule)

    inline VkShaderModule Get() const noexcept
    {
        return shaderHandle_.Get();
    }

  private:
    ShaderModuleHandle shaderHandle_;
};
