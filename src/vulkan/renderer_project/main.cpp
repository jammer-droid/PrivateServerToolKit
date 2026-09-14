#include <vulkan/vulkan.h>

#include <iostream>

#include "common/VulkanException.h"
#include "common/VulkanHandle.h"

#include "core/VulkanContext.h"
#include "core/Swapchain.h"
#include "core/FrameResources.h"

#include "app/Window.h"

using SurfaceHandle = VulkanHandle<VkSurfaceKHR, deleter::VkSurfaceDeleter>;

namespace
{

void RecordClearCommands(VkCommandBuffer commandBuffer, const Swapchain &swapchain, std::uint32_t imageIndex)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags =
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // 이번에 기록한 내용을 한 번 제출. reset 후 재사용 가능

    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    /*
     * Barrier: 실제 이미지 layout 전환
     * layout: 이미지를 어떤 용도로 접근할 수 있는 상태로 둘 것인가
     * stage: 어느 실행 단계인가
     * access: 그 실행 단계에서 어떤 메모리 접근인가
     * src/dst: 메모리 의존성 전/후
     */
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; // 이전 이미지 내용 보존하지 않음
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_NONE;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.image = swapchain.GetImage(imageIndex);
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // QueueFamily 소유권 이전 불필요
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    // 원본 이미지에서 동기화/레이아웃 전환을 적용할 범위 지정(Swapchain ImageView와 동일하게 지정함)
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkDependencyInfo dependency{};
    dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.imageMemoryBarrierCount = 1;
    dependency.pImageMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(commandBuffer, &dependency);

    // dynamic rendering - clear
    // 앞에서 메모리 배리어로 전환한 레이아웃 상태에서 이미지에 렌더링
    // 사용하려고 하는 이미지의 layout을 명시
    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = swapchain.GetImageView(imageIndex);
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;   // 렌더링 시작 시 지정 색으로 클리어
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // 결과 보존 후 이후 Present에 사용
    colorAttachment.clearValue.color = {{0.05f, 0.10f, 0.20f, 1.0f}};

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.renderArea.extent = swapchain.GetSettings().extent; // 실제 렌더링에 사용할 픽셀 좌표 영역
    renderingInfo.layerCount = 1; // 연결된 attachment에 대해 렌더링할 레이어 개수 지정
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

    vkCmdBeginRendering(commandBuffer, &renderingInfo);
    vkCmdEndRendering(commandBuffer);

    // PresentLayout 전환
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
    barrier.dstAccessMask = VK_ACCESS_2_NONE;

    vkCmdPipelineBarrier2(commandBuffer, &dependency);

    VK_CHECK(vkEndCommandBuffer(commandBuffer));
}

}; // namespace

int main()
{
    try
    {
        Window window;
        VulkanContext context(window.GetRequiredInstanceExtensions());
        SurfaceHandle surfaceHandle(window.CreateSurface(context.GetInstance()),
                                    deleter::VkSurfaceDeleter{context.GetInstance()});

        const PhysicalDeviceSelection selection = context.SelectPhysicalDevice(surfaceHandle.Get());
        VkPhysicalDeviceProperties selectedProperties{};
        vkGetPhysicalDeviceProperties(selection.physicalDevice, &selectedProperties);
        std::cout << "Selected GPU: " << selectedProperties.deviceName
                  << "\nGraphics family: " << selection.graphicsFamilyIndex
                  << "\nPresent family: " << selection.presentFamilyIndex
                  << "\nRequires portability subset: " << std::boolalpha << selection.requiresPortabilitySubset << '\n';

        context.InitializeDevice(selection); // Initialize Logical Device

        VkExtent2D framebufferSize = window.GetFramebufferSize();
        SwapchainSupport swapchainSupport =
            Swapchain::QuerySwapchainSupport(selection.physicalDevice, surfaceHandle.Get());
        Swapchain::PrintSwapchainSupport(swapchainSupport, framebufferSize);

        SwapchainSettings swapchainSetting{};
        bool configResult = Swapchain::ConfigureSwapchainSettings(swapchainSupport, framebufferSize, &swapchainSetting);
        if (!configResult)
        {
            throw std::runtime_error("Configure SwapchainSettings failed\n");
        }
        Swapchain swapchain(context.GetDevice(), surfaceHandle.Get(), swapchainSetting, selection,
                            swapchainSupport.capabilities);

        FrameResources frame(context.GetDevice(), selection.graphicsFamilyIndex);

        while (!window.IsCloseRequested())
        {
            window.WaitEvents();
        }
    }
    catch (const VulkanException &error)
    {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    catch (const std::runtime_error &error)
    {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }

    return 0;
}
