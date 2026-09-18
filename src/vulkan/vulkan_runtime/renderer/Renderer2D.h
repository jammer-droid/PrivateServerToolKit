#pragma once

#include "common/ClassTraits.h"
#include "common/VulkanHeaders.h"
#include "common/VulkanException.h"

#include "core/GraphicsPipeline.h"
#include "core/HostVisibleBuffer.h"
#include "core/InstanceData.h"
#include "renderer/DrawData2D.h"

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
    ~Renderer2D() noexcept;

    VK_NON_COPYABLE(Renderer2D)
    VK_NON_MOVABLE(Renderer2D)

    void UpdateColorFormat(VkFormat colorFormat);
    // 현재 프레임 슬롯의 GPU 사용 완료 후 호출한다. 입력 뷰는 호출 중에만 읽는다.
    void UpdateDrawData(std::uint32_t frameIndex, DrawData2DView drawData);
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
