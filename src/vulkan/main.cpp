#include <vulkan/vulkan.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>
#include <optional>
#include <array>
#include <cstring>
#include <fstream>

#include "Constants.h"

// Vulkan API -> Vulkan Loader -> Vulkan Driver -> GPU
int main()
{
    std::uint32_t apiVersion = 0;

    const VkResult result = vkEnumerateInstanceVersion(&apiVersion);

    if (result != VK_SUCCESS)
    {
        return EXIT_FAILURE;
    }

    std::cout << VK_API_VERSION_MAJOR(apiVersion) << '.' << VK_API_VERSION_MINOR(apiVersion) << '.'
              << VK_API_VERSION_PATCH(apiVersion);

    VkApplicationInfo applicationInfo{};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "VulkanStudy";
    applicationInfo.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    applicationInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo instanceCreateInfo{};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &applicationInfo;

#ifdef __APPLE__
    // macOS 설정
    // moltenVK 같은 portability 디바이스도 GPU 목록에 포함
    const char *instanceExtensions[] = {VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME};

    instanceCreateInfo.enabledExtensionCount = 1;
    instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions;
    instanceCreateInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    VkInstance instance = VK_NULL_HANDLE;
    const VkResult createResult = vkCreateInstance(&instanceCreateInfo, nullptr, &instance);

    if (createResult != VK_SUCCESS)
    {
        std::cerr << "Instance creation failed: " << createResult << '\n';
        return EXIT_FAILURE;
    }

    const auto destroyInstance = [](VkInstance handle) noexcept { vkDestroyInstance(handle, nullptr); };

    const std::unique_ptr<VkInstance_T, decltype(destroyInstance)> instanceOwner(instance, destroyInstance);

    std::vector<VkPhysicalDevice> physicalDevices;
    while (true)
    {
        std::uint32_t deviceCount = 0;

        const VkResult countResult = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (countResult != VK_SUCCESS)
        {
            std::cerr << "Device Count Query Failed: " << countResult << '\n';
            return EXIT_FAILURE;
        }

        if (deviceCount == 0)
        {
            std::cerr << "No Vulkan physical devices found.\n" << '\n';
            return EXIT_FAILURE;
        }

        physicalDevices.resize(deviceCount);

        const VkResult listResult = vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());

        // 준비한 배열에 전체 목록을 담지 못함
        // 호출 사이에 목록이 달라질 수 있기 때문에 개수 조회부터 다시 수행
        if (listResult == VK_INCOMPLETE)
        {
            continue;
        }

        if (listResult != VK_SUCCESS)
        {
            std::cerr << "Device enumeration failed: " << listResult << '\n';
            return EXIT_FAILURE;
        }

        physicalDevices.resize(deviceCount);
        break;
    }

    std::cout << "Physical device count: " << physicalDevices.size() << '\n';

    VkPhysicalDevice selectedPhysicalDevice = VK_NULL_HANDLE;
    for (const VkPhysicalDevice physicalDevice : physicalDevices)
    {
        VkPhysicalDeviceDriverProperties driverProperties{};
        driverProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;

        VkPhysicalDeviceProperties2 properties{};
        properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        properties.pNext = &driverProperties;

        vkGetPhysicalDeviceProperties2(physicalDevice, &properties);

        std::cout << "Device: " << properties.properties.deviceName << '\n';

        std::cout << "Driver: " << driverProperties.driverName << '\n';
        std::cout << "Driver Info: " << driverProperties.driverInfo << "\n\n";

        if (driverProperties.driverID == VK_DRIVER_ID_MOLTENVK && selectedPhysicalDevice == VK_NULL_HANDLE)
        {
            selectedPhysicalDevice = physicalDevice;
        }
    }

    if (selectedPhysicalDevice == VK_NULL_HANDLE)
    {
        std::cerr << "MoltenVK Physical device not found.\n";
        return EXIT_FAILURE;
    }

    std::uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(selectedPhysicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);

    vkGetPhysicalDeviceQueueFamilyProperties(selectedPhysicalDevice, &queueFamilyCount, queueFamilies.data());
    queueFamilies.resize(queueFamilyCount);

    std::optional<std::uint32_t> computeQueueFamilyIndex;

    for (std::uint32_t index = 0; index < queueFamilyCount; ++index)
    {
        const VkQueueFamilyProperties &family = queueFamilies[index];

        const bool supportsGraphics = (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
        const bool supportsCompute = (family.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0;

        std::cout << std::boolalpha << "Family " << index << " | queues: " << family.queueCount
                  << " | graphics: " << supportsGraphics << " | compute: " << supportsCompute << '\n';

        if (!computeQueueFamilyIndex.has_value() && family.queueCount > 0 && supportsCompute)
        {
            computeQueueFamilyIndex = index;
        }
    }

    if (!computeQueueFamilyIndex.has_value())
    {
        std::cerr << "Compute queue family not found.\n";
        return EXIT_FAILURE;
    }

    std::cout << "Selected compute family: " << computeQueueFamilyIndex.value() << '\n';

    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = computeQueueFamilyIndex.value();
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    // 해당 Device의 portability 계약을 사용
    const char *deviceExtensions[] = {"VK_KHR_portability_subset"};

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = 1;
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;

    VkDevice device = VK_NULL_HANDLE;
    const VkResult deviceResult = vkCreateDevice(selectedPhysicalDevice, &deviceCreateInfo, nullptr, &device);

    if (deviceResult != VK_SUCCESS)
    {
        std::cerr << "Device creation failed: " << deviceResult << '\n';
        return EXIT_FAILURE;
    }

    const auto destroyDevice = [](VkDevice handle) noexcept { vkDestroyDevice(handle, nullptr); };

    const std::unique_ptr<VkDevice_T, decltype(destroyDevice)> deviceOwner(device, destroyDevice);

    std::cout << "Device created.\n";

    VkQueue computeQueue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, computeQueueFamilyIndex.value(), 0, &computeQueue);

    std::cout << "Compute queue acquired."
              << " family = " << computeQueueFamilyIndex.value() << ", queue=0\n";

    constexpr VkDeviceSize BufferSize = sizeof(std::uint32_t) * kBufferCapacity;

    VkBufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = BufferSize;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer = VK_NULL_HANDLE;

    const auto freeMemory = [device](VkDeviceMemory handle) noexcept { vkFreeMemory(device, handle, nullptr); };

    std::unique_ptr<VkDeviceMemory_T, decltype(freeMemory)> memoryOwner(nullptr, freeMemory);

    const VkResult bufferResult = vkCreateBuffer(device, &bufferCreateInfo, nullptr, &buffer);

    if (bufferResult != VK_SUCCESS)
    {
        std::cerr << "Buffer creation failed: " << bufferResult << '\n';
        return EXIT_FAILURE;
    }

    const auto destroyBuffer = [device](VkBuffer handle) noexcept { vkDestroyBuffer(device, handle, nullptr); };

    std::unique_ptr<VkBuffer_T, decltype(destroyBuffer)> bufferOwner(buffer, destroyBuffer);

    // Buffer에 필요한 메모리 요구 사항
    VkMemoryRequirements memoryRequirements{};
    vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);

    std::cout << "Buffer Size: " << BufferSize << '\n';
    std::cout << "Required memory size: " << memoryRequirements.size << '\n';
    std::cout << "Required alignment: " << memoryRequirements.alignment << '\n';
    std::cout << "Compatible memory types: 0x" << std::hex << memoryRequirements.memoryTypeBits << std::dec << '\n';

    // PhysicalDevice의 메모리 프로퍼티
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(selectedPhysicalDevice, &memoryProperties);

    constexpr VkMemoryPropertyFlags RequiredMemoryProperties =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    std::optional<std::uint32_t> memoryTypeIndex;
    for (std::uint32_t index = 0; index < memoryProperties.memoryTypeCount; index++)
    {
        const VkMemoryPropertyFlags propertyFlags = memoryProperties.memoryTypes[index].propertyFlags;

        // buffer 호환성 확인
        const bool isCompatible = (memoryRequirements.memoryTypeBits & std::uint32_t{1} << index) != 0;

        // 요구 조건 확인
        const bool hasRequiredProperties = (propertyFlags & RequiredMemoryProperties) != 0;

        std::cout << "Memory type " << index << " | flags: 0x" << std::hex << propertyFlags << std::dec
                  << " | compatible: " << std::boolalpha << isCompatible << " | suitable: " << hasRequiredProperties
                  << '\n';

        if (!memoryTypeIndex.has_value() && isCompatible && hasRequiredProperties)
        {
            memoryTypeIndex = index;
        }
    }

    if (!memoryTypeIndex.has_value())
    {
        std::cerr << "No compatible host-visible coherent memory type.\n";
        return EXIT_FAILURE;
    }

    std::cout << "selected memory type: " << memoryTypeIndex.value() << '\n';

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = memoryTypeIndex.value();

    VkDeviceMemory deviceMemory = VK_NULL_HANDLE;

    const VkResult allocationResult = vkAllocateMemory(device, &allocateInfo, nullptr, &deviceMemory);

    if (allocationResult != VK_SUCCESS)
    {
        std::cerr << "Memory allocation failed: " << allocationResult << '\n';
        return EXIT_FAILURE;
    }

    memoryOwner.reset(deviceMemory);

    const VkResult bindResult = vkBindBufferMemory(device, buffer, deviceMemory, 0);

    if (bindResult != VK_SUCCESS)
    {
        std::cerr << "Buffer Memory binding failed: " << bindResult << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "Memory Allocated: " << memoryRequirements.size << " bytes\n";
    std::cout << "Buffer memory bound at offset 0.\n";

    std::array<std::uint32_t, kBufferCapacity> inputValue{};
    for (std::uint32_t index = 0; index < kBufferCapacity; index++)
    {
        inputValue[index] = index + 1;
    }

    std::array<std::uint32_t, kBufferCapacity> readValue{};
    {
        void *mappedData = nullptr;
        const VkResult mapResult = vkMapMemory(device, deviceMemory, 0, BufferSize, 0, &mappedData);

        if (mapResult != VK_SUCCESS)
        {
            std::cerr << "Memory mapping failed: " << mapResult << '\n';
            return EXIT_FAILURE;
        }

        const auto unmapMemory = [device, deviceMemory](void *) noexcept { vkUnmapMemory(device, deviceMemory); };

        const std::unique_ptr<void, decltype(unmapMemory)> mappingOwner(mappedData, unmapMemory);

        std::memcpy(mappedData, inputValue.data(), inputValue.size() * sizeof(std::uint32_t));

        std::memcpy(readValue.data(), mappedData, readValue.size() * sizeof(std::uint32_t));
    }

    if (readValue != inputValue)
    {
        std::cerr << "Mapped memory verification failed.\n";
        return EXIT_FAILURE;
    }

    // std::cout << "Read Values: ";
    // for (const std::uint32_t value : readValue)
    // {
    //     std::cout << value << ' ';
    // }
    // std::cout << "\nMapped memory verification passed.\n";

    VkDescriptorSetLayoutBinding storageBinding{};
    storageBinding.binding = 0;
    storageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    storageBinding.descriptorCount = 1;
    storageBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.bindingCount = 1;
    descriptorSetLayoutCreateInfo.pBindings = &storageBinding;

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

    const VkResult layoutResult =
        vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout);

    if (layoutResult != VK_SUCCESS)
    {
        std::cerr << "vkCreateDescriptorSetLayout failed: " << layoutResult << '\n';
        return EXIT_FAILURE;
    }

    const auto destroyDescriptorSetLayout = [device](VkDescriptorSetLayout handle) noexcept {
        vkDestroyDescriptorSetLayout(device, handle, nullptr);
    };
    const std::unique_ptr<VkDescriptorSetLayout_T, decltype(destroyDescriptorSetLayout)> descriptorSetLayoutOwner(
        descriptorSetLayout, destroyDescriptorSetLayout);

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.maxSets = 1;
    descriptorPoolCreateInfo.poolSizeCount = 1;
    descriptorPoolCreateInfo.pPoolSizes = &poolSize;

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

    const VkResult poolResult = vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool);

    if (poolResult != VK_SUCCESS)
    {
        std::cerr << "vkCreateDescriptorPool failed: " << poolResult << '\n';
        return EXIT_FAILURE;
    }

    const auto destroyDescriptorPool = [device](VkDescriptorPool handle) noexcept {
        vkDestroyDescriptorPool(device, handle, nullptr);
    };

    const std::unique_ptr<VkDescriptorPool_T, decltype(destroyDescriptorPool)> descriptorPoolOwner(
        descriptorPool, destroyDescriptorPool);

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    const VkResult allocateSetResult = vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet);

    if (allocateSetResult != VK_SUCCESS)
    {
        std::cerr << "vkAllocateDescriptorSets failed: " << allocateSetResult << '\n';
        return EXIT_FAILURE;
    }

    VkDescriptorBufferInfo descriptorBufferInfo{};
    descriptorBufferInfo.buffer = buffer;    // 사용할 buffer
    descriptorBufferInfo.offset = 0;         // buffer 시작 기준 바이트 오프셋
    descriptorBufferInfo.range = BufferSize; // 그 위치부터 사용할 바이트 수

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrite.pBufferInfo = &descriptorBufferInfo;

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

    std::cout << "Descriptor set allocated and updated. binding = 0, range = " << descriptorBufferInfo.range
              << " bytes\n";

    std::ifstream shaderFile("shaders/double.comp.spv", std::ios::binary | std::ios::ate);

    if (!shaderFile)
    {
        std::cerr << "Failed to open shaders/double.comp.spv\n";
        return EXIT_FAILURE;
    }

    const std::streamoff shaderFileSize = shaderFile.tellg();

    // SPIR-V의 바이트 크기는 4의 배수여야 함
    if (shaderFileSize <= 0 || shaderFileSize % 4 != 0)
    {
        std::cerr << "Invalid SPIR-V file size\n";
        return EXIT_FAILURE;
    }

    std::vector<std::uint32_t> shaderCode(static_cast<std::size_t>(shaderFileSize / 4));

    shaderFile.seekg(0, std::ios::beg);

    if (!shaderFile.read(reinterpret_cast<char *>(shaderCode.data()), static_cast<std::streamsize>(shaderFileSize)))
    {
        std::cerr << "Failed to read SPRI-V file\n";
        return EXIT_FAILURE;
    }

    VkShaderModuleCreateInfo shaderModuleCreateInfo{};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize = shaderCode.size() * sizeof(std::uint32_t);
    shaderModuleCreateInfo.pCode = shaderCode.data();

    VkShaderModule shaderModule = VK_NULL_HANDLE;

    const VkResult shaderModuleResult = vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule);
    if (shaderModuleResult != VK_SUCCESS)
    {
        std::cerr << "vkCreateShaderModule failed: " << shaderModuleResult << '\n';
        return EXIT_FAILURE;
    }

    const auto destroyShaderModule = [device](VkShaderModule handle) noexcept {
        vkDestroyShaderModule(device, handle, nullptr);
    };

    const std::unique_ptr<VkShaderModule_T, decltype(destroyShaderModule)> shaderModuleOwner(shaderModule,
                                                                                             destroyShaderModule);

#pragma pack(push, 1)
    struct ComputePushConstants
    {
        std::uint32_t elementCount;
        std::uint32_t multiplier;
    };
#pragma pack(pop)

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(ComputePushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;

    // Compute Shader가 Push Constant의 range 만큼의 바이트 사용
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

    const VkResult pipelineLayoutResult =
        vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);

    if (pipelineLayoutResult != VK_SUCCESS)
    {
        std::cerr << "vkCreatePipelineLayout failed: " << pipelineLayoutResult << '\n';
        return EXIT_FAILURE;
    }

    const auto destroyPipelineLayout = [device](VkPipelineLayout handle) noexcept {
        vkDestroyPipelineLayout(device, handle, nullptr);
    };

    const std::unique_ptr<VkPipelineLayout_T, decltype(destroyPipelineLayout)> pipelineLayoutOwner(
        pipelineLayout, destroyPipelineLayout);

    VkPipelineShaderStageCreateInfo shaderStageCreateInfo{};
    shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageCreateInfo.module = shaderModule;
    shaderStageCreateInfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.layout = pipelineLayout;
    computePipelineCreateInfo.stage = shaderStageCreateInfo;

    VkPipeline computePipeline = VK_NULL_HANDLE;

    const VkResult pipelineResult =
        vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &computePipeline);

    if (pipelineResult != VK_SUCCESS)
    {
        std::cerr << "vkCreateComputePipelines failed: " << pipelineResult << '\n';
        return EXIT_FAILURE;
    }

    const auto destroyPipeline = [device](VkPipeline handle) noexcept { vkDestroyPipeline(device, handle, nullptr); };

    const std::unique_ptr<VkPipeline_T, decltype(destroyPipeline)> computePipelineOwner(computePipeline,
                                                                                        destroyPipeline);

    std::cout << "Compute Pipeline created.\n\n";

    VkCommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.queueFamilyIndex = computeQueueFamilyIndex.value();
    commandPoolCreateInfo.flags =
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // Pool에서 할당한 Command Buffer를 개별적으로 초기화 가능

    VkCommandPool commandPool = VK_NULL_HANDLE;

    const VkResult commandPoolResult = vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool);

    if (commandPoolResult != VK_SUCCESS)
    {
        std::cerr << "vkCreateCommandPool failed: " << commandPoolResult << '\n';
        return EXIT_FAILURE;
    }

    const auto destroyCommandPool = [device](VkCommandPool handle) noexcept {
        vkDestroyCommandPool(device, handle, nullptr);
    };

    const std::unique_ptr<VkCommandPool_T, decltype(destroyCommandPool)> commandPoolOwner(commandPool,
                                                                                          destroyCommandPool);

    VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

    const VkResult commandBufferResult = vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer);

    if (commandBufferResult != VK_SUCCESS)
    {
        std::cout << "vkAllocateCommandBuffers failed: " << commandBufferResult << '\n';
        return EXIT_FAILURE;
    }

    // job
    const std::array<ComputePushConstants, 3> jobs{{
        {33u, 3u},
        {17u, 0u},
        {1024u, 1u},
    }};

    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    VkFence computeFence = VK_NULL_HANDLE;
    const VkResult fenceResult = vkCreateFence(device, &fenceCreateInfo, nullptr, &computeFence);

    if (fenceResult != VK_SUCCESS)
    {
        std::cerr << "vkCreateFence failed: " << fenceResult << '\n';
        return EXIT_FAILURE;
    }

    const auto destroyFence = [device](VkFence handle) noexcept { vkDestroyFence(device, handle, nullptr); };

    const std::unique_ptr<VkFence_T, decltype(destroyFence)> fenceOwner(computeFence, destroyFence);

    for (std::size_t index = 0; index < jobs.size(); index++)
    {
        if (jobs[index].elementCount > kBufferCapacity)
        {
            std::cerr << "Job " << index << " : elementCount exceeds buffer capacity.\n";
            return EXIT_FAILURE;
        }

        std::cout << "jobs[" << index << "] information\n";
        std::cout << "N = " << jobs[index].elementCount << " Multiplier = " << jobs[index].multiplier << '\n';

        {
            void *mappedData = nullptr;
            const VkResult mappedResult = vkMapMemory(device, deviceMemory, 0, BufferSize, 0, &mappedData);

            if (mappedResult != VK_SUCCESS)
            {
                std::cerr << "vkMapMemory failed: " << mappedResult << '\n';
                return EXIT_FAILURE;
            }

            std::memcpy(mappedData, inputValue.data(), sizeof(std::uint32_t) * inputValue.size());

            vkUnmapMemory(device, deviceMemory);
        }

        const VkResult resetCommandBufferResult = vkResetCommandBuffer(commandBuffer, 0);
        if (resetCommandBufferResult != VK_SUCCESS)
        {
            std::cerr << "vkResetCommandBuffer failed: " << resetCommandBufferResult << '\n';
            return EXIT_FAILURE;
        }

        VkCommandBufferBeginInfo commandBufferBeginInfo{};
        commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        const VkResult beginResult = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
        if (beginResult != VK_SUCCESS)
        {
            std::cerr << "vkBeginCommandBuffer failed: " << beginResult << '\n';
            return EXIT_FAILURE;
        }

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0,
                                nullptr);

        // Push Constants
        ComputePushConstants pushConstants{.elementCount = jobs[index].elementCount,
                                           .multiplier = jobs[index].multiplier};
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants),
                           &pushConstants);

        // dispatch는 1회로 고정
        std::uint32_t groupCountX = (pushConstants.elementCount + kWorkGroupLocalSize - 1) / kWorkGroupLocalSize;
        std::uint32_t groupCountY = 1;
        std::uint32_t groupCountZ = 1;

        if (groupCountX > 0)
        {
            vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);
        }

        VkMemoryBarrier readbackBarrier{};
        readbackBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        readbackBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        readbackBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1,
                             &readbackBarrier, 0, nullptr, 0, nullptr);

        const VkResult endResult = vkEndCommandBuffer(commandBuffer);
        if (endResult != VK_SUCCESS)
        {
            std::cerr << "vkEndCommandBuffer failed: " << endResult << '\n';
            return EXIT_FAILURE;
        }

        std::cout << "Compute command buffer recorded.\n";

        const VkResult resetFenceResult = vkResetFences(device, 1, &computeFence);
        if (resetFenceResult != VK_SUCCESS)
        {
            std::cerr << "vkResetFences failed: " << resetFenceResult << '\n';
            return EXIT_FAILURE;
        }

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        const VkResult submitResult = vkQueueSubmit(computeQueue, 1, &submitInfo, computeFence);

        if (submitResult != VK_SUCCESS)
        {
            std::cerr << "vkQueueSubmit failed: " << submitResult << '\n';
            return EXIT_FAILURE;
        }

        std::cout << "Compute work submitted.\n";

        const VkResult waitReuslt =
            vkWaitForFences(device, 1, &computeFence, VK_TRUE, std::numeric_limits<std::uint64_t>::max());

        if (waitReuslt != VK_SUCCESS)
        {
            std::cerr << "vkWaitForFences failed: " << waitReuslt << '\n';

            std::abort();
        }

        std::cout << "Compute work completed.\n";

        std::array<std::uint32_t, kBufferCapacity> gpuValues{};
        {
            void *mappedResultData = nullptr;

            const VkResult resultMapResult = vkMapMemory(device, deviceMemory, 0, BufferSize, 0, &mappedResultData);

            if (resultMapResult != VK_SUCCESS)
            {
                std::cerr << "vkMapMemory for result failed: " << resultMapResult << '\n';
                return EXIT_FAILURE;
            }

            const auto unmapResultMemory = [device, deviceMemory](void *) noexcept {
                vkUnmapMemory(device, deviceMemory);
            };

            const std::unique_ptr<void, decltype(unmapResultMemory)> resultMappingOwner(mappedResultData,
                                                                                        unmapResultMemory);

            std::memcpy(gpuValues.data(), mappedResultData, gpuValues.size() * sizeof(std::uint32_t));
        }

        // std::cout << "GPU result: ";
        // for (const std::uint32_t value : gpuValues)
        // {
        //     std::cout << value << ' ';
        // }

        std::cout << '\n';

        for (std::uint32_t index = 0; index < kBufferCapacity; ++index)
        {
            const std::uint32_t initialValue = index + 1u;
            const std::uint32_t expectedValue =
                index < pushConstants.elementCount ? initialValue * pushConstants.multiplier : initialValue;

            if (gpuValues[index] != expectedValue)
            {
                std::cerr << "Result mismatch at index: " << index << ". expected " << expectedValue << ", got "
                          << gpuValues[index] << '\n';
                return EXIT_FAILURE;
            }
        }

        std::cout << "jobs[" << index << "] result\n";
        std::cout << " verification passed\n\n";
    }

    std::cout << "GPU Compute verification passed.\n";

    return EXIT_SUCCESS;
}
