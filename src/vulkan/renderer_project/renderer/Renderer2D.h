#pragma once

#include "common/ClassTraits.h"
#include "common/VulkanHeaders.h"
#include "common/VulkanException.h"

#include "core/GraphicsPipeline.h"
#include "core/HostVisibleBuffer.h"
#include "core/InstanceData.h"

#include <cstdint>
#include <memory>
#include <filesystem>
#include <vector>
#include <deque>

class Renderer2D
{
  public:
    explicit Renderer2D(VkPhysicalDevice physical, VkDevice device, VkFormat colorFormat, std::uint32_t frameCount,
                        std::uint32_t maxInstances_, const std::filesystem::path &shaderPath);
    ~Renderer2D() = default;

    VK_NON_COPYABLE(Renderer2D)
    VK_NON_MOVABLE(Renderer2D)

    void UpdateColorFormat(VkFormat colorFormat);
    void UpdateInstance(std::uint32_t frameIndex, const std::vector<InstanceData> &instances);
    void RecordDraws(VkCommandBuffer commandBuffer, VkExtent2D extent, std::uint32_t frameIndex) const;

  private:
    VkDevice device_;
    VkFormat colorFormat_;

    std::uint32_t frameCount_;
    std::uint32_t maxInstances_;
    std::filesystem::path shaderPath_;

    std::unique_ptr<GraphicsPipeline> graphicsPipelineOwner_;
    std::deque<HostVisibleBuffer> instanceBuffers_;
    std::vector<std::uint32_t> instanceCounts_;
};
