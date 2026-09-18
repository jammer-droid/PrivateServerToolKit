#include "HostVisibleBuffer.h"

#include "common/VulkanException.h"

#include <cstring>

namespace
{

}; // namespace

HostVisibleBuffer::HostVisibleBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize capacityBytes)
    : device_{device}, capacityBytes_{capacityBytes}
{
    if (capacityBytes == 0)
    {
        throw std::runtime_error("HostVisibleBuffer.ctor: CapacityBytes is zero\n");
    }

    VkBufferCreateInfo bufferCI{};
    bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCI.size = capacityBytes;
    bufferCI.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(device, &bufferCI, nullptr, &buffer));
    buffer_.Adopt(buffer, deleter::VkBufferDeleter{device});

    VkMemoryRequirements memoryRequirements{};
    vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);
    /*
     * MemoryRequirements fields
     * - size: 실제로 확보해야 하는 메모리 크기. capacityBytes(논리적 크기)와 다를 수 있음
     * - alignment: 메모리 바인딩 offset의 정렬 조건
     * - memoryTypeBits: 해당 Buffer에 사용할 수 있는 메모리 타입 목록(최대 32개)
     */

    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    const VkMemoryPropertyFlags requiredMemoryPropertyFlag =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    std::uint32_t selectedIndex = 0;
    bool hasMemory = false;
    for (std::uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
    {
        const bool allowed = (memoryRequirements.memoryTypeBits & (1u << i)) != 0;
        const bool hasRequired =
            (memoryProperties.memoryTypes[i].propertyFlags & requiredMemoryPropertyFlag) == requiredMemoryPropertyFlag;

        if (allowed && hasRequired)
        {
            hasMemory = true;
            selectedIndex = i;
            break;
        }
    }

    if (!hasMemory)
    {
        throw std::runtime_error("HostVisibleBuffer.ctor: PhysicalDevice has no required memory property flags\n");
    }

    // 다음 메모리 할당은 버퍼 전용 메모리 할당임을 Vulkan에 알리기 위한 구조체
    VkMemoryDedicatedAllocateInfo dedicatedAllocInfo{};
    dedicatedAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    dedicatedAllocInfo.buffer = buffer;

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = selectedIndex;
    allocateInfo.pNext = &dedicatedAllocInfo;

    VkDeviceMemory deviceMemory = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(device, &allocateInfo, nullptr, &deviceMemory));
    memory_.Adopt(deviceMemory, deleter::VkDeviceMemoryDeleter{device});

    VK_CHECK(vkBindBufferMemory(device, buffer, deviceMemory, 0));

    VK_CHECK(vkMapMemory(device, deviceMemory, 0, capacityBytes, 0, &mapped_));
}

HostVisibleBuffer::~HostVisibleBuffer() noexcept
{
    if (mapped_)
    {
        vkUnmapMemory(device_, memory_.Get());
        mapped_ = nullptr;
    }
}

// GPU가 해당 Buffer를 사용하지 않는 시점에 호출해야 한다.
void HostVisibleBuffer::Write(const void *data, std::size_t byteCount)
{
    if (byteCount == 0)
    {
        return;
    }

    if (byteCount > capacityBytes_)
    {
        throw std::runtime_error("HostVisibleBuffer.Write: Required Bytes(" + std::to_string(byteCount) +
                                 ") > Buffer Capacity(" + std::to_string(capacityBytes_) + ")\n");
    }

    if (data == nullptr)
    {
        throw std::runtime_error("HostVisibleBuffer.Write: Write data is nullptr\n");
    }

    std::memcpy(mapped_, data, byteCount);
}
