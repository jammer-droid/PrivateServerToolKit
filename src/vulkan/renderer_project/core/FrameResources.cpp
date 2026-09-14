#include "FrameResources.h"

#include "common/VulkanException.h"

FrameResources::FrameResources(VkDevice device, std::uint32_t queueFamilyIndex)
{
    VkCommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;
    // 개별 Command Buffer를 초기화해서 사용할 수 있도록 설정
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkCommandPool commandPool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool));

    commandPoolHandle_.Adopt(commandPool, deleter::VkCommandPoolDeleter{device});

    // PrimaryBuffer 생성
    // - Primary: 명령을 기록하고 GPU 큐에 submit 할 수 있는 버퍼
    // - Secondary: 명령을 기록하지만 큐에 직접 제출할 수 없고, Primary에서 실행
    VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = commandPoolHandle_.Get();
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;

    VK_CHECK(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer_));

    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkFence fence = VK_NULL_HANDLE;

    VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &fence));

    fenceHandle_.Adopt(fence, deleter::VkFenceDeleter{device});
    VK_CHECK(vkGetFenceStatus(device, fenceHandle_.Get()));
}
