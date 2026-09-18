#include "Renderer2D.h"

#include "common/VulkanException.h"

#include "core/DrawPushConstants.h"
#include "core/InstanceData.h"

Renderer2D::Renderer2D(VkPhysicalDevice physicalDevice, VkDevice device, VkFormat colorFormat, std::uint32_t frameCount,
                       std::uint32_t maxInstances, const std::filesystem::path &shaderPath)
    : device_{device}, colorFormat_{colorFormat}, frameCount_{frameCount}, maxInstances_{maxInstances},
      shaderPath_{shaderPath}
{
    if (frameCount == 0 || maxInstances == 0)
    {
        throw std::runtime_error("Invalid frameCount or maxInstances.\n");
    }

    graphicsPipelineOwner_ = std::make_unique<GraphicsPipeline>(device, colorFormat, shaderPath);

    instanceCounts_.resize(frameCount, 0);
    VkDeviceSize capacityBytes = sizeof(InstanceData) * maxInstances;
    for (std::uint32_t i = 0; i < frameCount; i++)
    {
        instanceBuffers_.emplace_back(physicalDevice, device, capacityBytes);
    }
}

void Renderer2D::UpdateColorFormat(VkFormat colorFormat)
{
    if (colorFormat_ == colorFormat)
    {
        return;
    }
    std::unique_ptr<GraphicsPipeline> newGraphicsPipeline =
        std::make_unique<GraphicsPipeline>(device_, colorFormat, shaderPath_);

    graphicsPipelineOwner_.swap(newGraphicsPipeline);
    colorFormat_ = colorFormat;
}

void Renderer2D::UpdateInstance(std::uint32_t frameIndex, const std::vector<InstanceData> &instances)
{
    if (frameIndex >= instanceCounts_.size())
    {
        throw std::runtime_error("Invalid frame index");
    }
    if (instances.size() > maxInstances_)
    {
        throw std::runtime_error("Instance capacity exceeded");
    }

    for (const InstanceData &instance : instances)
    {
        if (instance.shape != Shape::Rectangle && instance.shape != Shape::Circle && instance.shape != Shape::Line)
        {
            throw std::runtime_error("Unsupported shape");
        }
    }

    HostVisibleBuffer &hostVisibleBuffer = instanceBuffers_[frameIndex];
    hostVisibleBuffer.Write(instances.data(), sizeof(InstanceData) * instances.size());
    instanceCounts_[frameIndex] = static_cast<std::uint32_t>(instances.size());
}

void Renderer2D::RecordDraws(VkCommandBuffer commandBuffer, VkExtent2D extent, std::uint32_t frameIndex) const
{
    if (frameIndex >= instanceCounts_.size())
    {
        throw std::runtime_error("Invalid frame index");
    }

    // Draw 명령을 사용할 Pipeline 바인딩
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineOwner_->GetPipeline());

    // NDC 좌표를 framebuffer 좌표로 변환할 영역과 깊이 범위 지정
    VkViewport viewport{};
    viewport.x = 0.0f;                                   // Viewport 시작 위치 좌표
    viewport.y = 0.0f;                                   // Viewport 시작 위치 Y 좌표
    viewport.width = static_cast<float>(extent.width);   // 가로
    viewport.height = static_cast<float>(extent.height); // 세로
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    VkBuffer instanceBuffer = instanceBuffers_[frameIndex].GetBuffer();

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &instanceBuffer, &offset);

    DrawPushConstants pushConstants{};
    pushConstants.viewportSize[0] = static_cast<float>(extent.width);
    pushConstants.viewportSize[1] = static_cast<float>(extent.height);
    vkCmdPushConstants(commandBuffer, graphicsPipelineOwner_->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(DrawPushConstants), &pushConstants);

    vkCmdDraw(commandBuffer, 6, instanceCounts_[frameIndex], 3, 0);
}

Renderer2D::~Renderer2D() noexcept = default;
