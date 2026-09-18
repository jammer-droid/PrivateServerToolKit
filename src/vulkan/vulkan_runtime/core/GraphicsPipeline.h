#pragma once

#include "VulkanRuntimeExport.h"

#include "common/ClassTraits.h"
#include "common/VulkanHandle.h"

#include <filesystem>

class GraphicsPipeline
{
  public:
    VULKAN_RUNTIME_API explicit GraphicsPipeline(VkDevice device, VkFormat colorFormat,
                                                 const std::filesystem::path &shaderPath);
    VULKAN_RUNTIME_API ~GraphicsPipeline() noexcept;

    VK_NON_COPYABLE(GraphicsPipeline)
    VK_NON_MOVABLE(GraphicsPipeline)

    inline VkPipelineLayout GetPipelineLayout() const noexcept
    {
        return pipelineLayoutHandle_.Get();
    }
    inline VkPipeline GetPipeline() const noexcept
    {
        return pipelineHandle_.Get();
    }

  private:
    PipelineLayoutHandle pipelineLayoutHandle_;
    PipelineHandle pipelineHandle_;
};
