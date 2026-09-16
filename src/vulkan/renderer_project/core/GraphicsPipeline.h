#pragma once

#include "common/ClassTraits.h"
#include "common/VulkanHandle.h"

#include <filesystem>

class GraphicsPipeline
{
  public:
    explicit GraphicsPipeline(VkDevice device, VkFormat colorFormat, const std::filesystem::path &shaderPath);
    ~GraphicsPipeline() = default;

    VK_NON_COPYABLE(GraphicsPipeline)
    VK_NON_MOVABLE(GraphicsPipeline)

    inline VkPipeline GetPipeline() const noexcept
    {
        return pipelineHandle_.Get();
    }

  private:
    PipelineLayoutHandle pipelineLayoutHandle_;
    PipelineHandle pipelineHandle_;
};
