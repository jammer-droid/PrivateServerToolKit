#include "FrameResources.h"

#include "common/VulkanException.h"

namespace
{

};

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
    commandBufferAllocateInfo.commandBufferCount = kFramesInFlight;

    VK_CHECK(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, commandBuffers_));

    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (std::uint32_t i = 0; i < kFramesInFlight; i++)
    {
        VkFence fence = VK_NULL_HANDLE;
        fenceHandles_.emplace_back();

        VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &fence));

        fenceHandles_.back().Adopt(fence, deleter::VkFenceDeleter{device});
        VK_CHECK(vkGetFenceStatus(device, fenceHandles_.back().Get()));

        VkSemaphoreCreateInfo semaCreateInfo{};
        semaCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkSemaphore semaphore = VK_NULL_HANDLE;

        imageAvailableSemaphoreHandles_.emplace_back();
        VK_CHECK(vkCreateSemaphore(device, &semaCreateInfo, nullptr, &semaphore));
        imageAvailableSemaphoreHandles_.back().Adopt(semaphore, deleter::VkSemaphoreDeleter{device});
    }
}

FrameResources::FrameResources(VkDevice device, std::uint32_t queueFamilyIndex, VkPhysicalDevice physicalDevice,
                               VkDeviceSize capacityBytes)
    : FrameResources{device, queueFamilyIndex}
{
    for (std::uint32_t i = 0; i < kFramesInFlight; i++)
    {
        instanceBuffers_.emplace_back(physicalDevice, device, capacityBytes);
    }
}

std::uint32_t FrameResources::GetFrameIndex() const noexcept
{
    return frameIndex % kFramesInFlight;
}
