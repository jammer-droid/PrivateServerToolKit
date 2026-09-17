#include "Renderer2D.h"

#include "common/VulkanException.h"

#include "core/DrawPushConstants.h"
#include "core/InstanceData.h"

Renderer2D::Renderer2D(VkPhysicalDevice physicalDevice, VkDevice device, VkFormat colorFormat, std::uint32_t frameCount,
                       std::uint32_t maxInstances, const std::filesystem::path &shaderPath)
    : physicalDevice_{physicalDevice}, device_{device}, colorFormat_{colorFormat}, frameCount_{frameCount},
      maxInstances_{maxInstances}, shaderPath_{shaderPath}
{
    graphicsPipelineOwner_ = std::make_unique<GraphicsPipeline>(device, colorFormat, shaderPath);

    std::uint32_t capacityBytes = sizeof(InstanceData) * maxInstances;
    for (std::uint32_t i = 0; i < frameCount; i++)
    {
        instanceBuffers_.emplace_back(physicalDevice, device, capacityBytes);
    }
}

void Renderer2D::UpdateInstance(std::uint32_t frameIndex, const std::vector<InstanceData> &instances)
{
    instanceCount_ = static_cast<std::uint32_t>(instances.size());
    HostVisibleBuffer &hostVisibleBuffer = instanceBuffers_[frameIndex];
    hostVisibleBuffer.Write(instances.data(), sizeof(InstanceData) * instances.size());
}

void Renderer2D::RecordDraws(VkCommandBuffer commandBuffer, VkExtent2D extent, std::uint32_t frameIndex) const
{
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

    vkCmdDraw(commandBuffer, 6, instanceCount_, 3, 0);
}
