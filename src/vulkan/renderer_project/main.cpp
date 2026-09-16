#include <vulkan/vulkan.h>

#include <iostream>
#include <memory>
#include <filesystem>

#include "common/VulkanException.h"
#include "common/VulkanHandle.h"

#include "core/DrawPushConstants.h"
#include "core/VulkanContext.h"
#include "core/Swapchain.h"
#include "core/FrameResources.h"
#include "core/GraphicsPipeline.h"

#include "app/Window.h"

using SurfaceHandle = VulkanHandle<VkSurfaceKHR, deleter::VkSurfaceDeleter>;

namespace
{

void RecordFrameCommands(VkCommandBuffer commandBuffer, const Swapchain &swapchain, std::uint32_t imageIndex,
                         const GraphicsPipeline &pipeline, const DrawPushConstants &pushConstants)
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
    // Attachment: ImageView를 이번 렌더링에서 어떤 역할로 사용할지 설정
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

    // Draw 명령을 사용할 Pipeline 바인딩
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetPipeline());

    const VkExtent2D extent = swapchain.GetSettings().extent;

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

    vkCmdPushConstants(commandBuffer, pipeline.GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(DrawPushConstants), &pushConstants);

    /*
     * vertexCount: 인스턴스 하나당 사용할 정점 개수
     * instanceCount: 같은 정점 구성으로 그릴 인스턴스 개수
     * firstVertex: 시작 정점 번호
     * firstInstance: 시작 인스턴스 번호
     */
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

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

        std::unique_ptr<Swapchain> swapchainOwner = std::make_unique<Swapchain>(
            context.GetDevice(), surfaceHandle.Get(), swapchainSetting, selection, swapchainSupport.capabilities);

        FrameResources frame(context.GetDevice(), selection.graphicsFamilyIndex);

        const std::filesystem::path shaderDir{RENDERER_SHADER_DIR};

        std::unique_ptr<GraphicsPipeline> graphicsPipelineOwner = std::make_unique<GraphicsPipeline>(
            context.GetDevice(), swapchainOwner->GetSettings().surfaceFormat.format, shaderDir);

        try
        {
            bool running = true;
            bool recreateSwapchain = false;
            while (running)
            {
                window.PollEvents();
                if (window.IsCloseRequested())
                {
                    running = false;
                    continue;
                }

                if (recreateSwapchain || window.HasFramebufferResized())
                {
                    VkExtent2D newFramebuffer = window.GetFramebufferSize();
                    while (newFramebuffer.width == 0 || newFramebuffer.height == 0)
                    {
                        if (window.IsCloseRequested())
                        {
                            running = false;
                            break;
                        }
                        window.WaitEvents();
                        newFramebuffer = window.GetFramebufferSize();
                    }

                    VkFormat prevFormat = swapchainSetting.surfaceFormat.format;
                    swapchainSupport = Swapchain::QuerySwapchainSupport(selection.physicalDevice, surfaceHandle.Get());
                    configResult =
                        Swapchain::ConfigureSwapchainSettings(swapchainSupport, newFramebuffer, &swapchainSetting);
                    if (!configResult)
                    {
                        running = false;
                        continue;
                    }

                    VK_CHECK(vkDeviceWaitIdle(context.GetDevice()));

                    std::unique_ptr<Swapchain> newSwapchain = std::make_unique<Swapchain>(
                        context.GetDevice(), surfaceHandle.Get(), swapchainSetting, selection,
                        swapchainSupport.capabilities, swapchainOwner->GetSwapchain());
                    swapchainOwner.swap(newSwapchain);

                    if (prevFormat != swapchainSetting.surfaceFormat.format)
                    {
                        std::unique_ptr<GraphicsPipeline> newGraphicsPipeline = std::make_unique<GraphicsPipeline>(
                            context.GetDevice(), swapchainSetting.surfaceFormat.format, shaderDir);
                        graphicsPipelineOwner.swap(newGraphicsPipeline);
                    }

                    window.ClearFramebufferResized();
                    recreateSwapchain = false;
                    continue;
                }

                framebufferSize = window.GetFramebufferSize();
                if (framebufferSize.width == 0 || framebufferSize.height == 0)
                {
                    std::cout << "framebuffer size is zero\n";
                    running = false;
                    continue;
                }

                const std::uint32_t currentFrameIndex = frame.GetFrameIndex();
                VkDevice device = context.GetDevice();
                VkSwapchainKHR sc = swapchainOwner->GetSwapchain();

                VkFence fence = frame.GetFence(currentFrameIndex);
                VK_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));

                std::uint32_t imageIndex = 0;
                VkSemaphore imageAvailable = frame.GetImageAvailable(currentFrameIndex);
                // Acquire에서 반환된 이미지가 GPU가 접근해도 되는 시점에 imageAvailable이 signal 됨
                // submit에서는 해당 세마포어를 wait하고 signaled가 되면 이미지를 사용함
                VkResult acquireResult =
                    vkAcquireNextImageKHR(device, sc, 100'000'000, imageAvailable, VK_NULL_HANDLE, &imageIndex);

                if (acquireResult == VK_TIMEOUT || acquireResult == VK_NOT_READY)
                {
                    continue;
                }
                if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
                {
                    recreateSwapchain = true;
                    continue;
                }

                // SUBOPTIMAL은 이미지 획득에는 성공했지만, 스왑체인 설정이 현재 Surface와 맞지 않는다는 의미
                recreateSwapchain = (acquireResult == VK_SUBOPTIMAL_KHR);
                if (acquireResult != VK_SUBOPTIMAL_KHR)
                {
                    VK_CHECK(acquireResult);
                }

                VkCommandBuffer commandBuffer = frame.GetCommandBuffer(currentFrameIndex);

                VK_CHECK(vkResetCommandBuffer(commandBuffer, 0));

                DrawPushConstants pushConstants{};
                pushConstants.positionAndSize[0] = 100.0f;
                pushConstants.positionAndSize[1] = 80.0f;
                pushConstants.positionAndSize[2] = 240.0f;
                pushConstants.positionAndSize[3] = 180.0f;

                VkExtent2D extent = swapchainOwner->GetSettings().extent;
                pushConstants.viewportSize[0] = static_cast<float>(extent.width);
                pushConstants.viewportSize[1] = static_cast<float>(extent.height);

                pushConstants.color[0] = 1.0f;
                pushConstants.color[1] = 0.4f;
                pushConstants.color[2] = 0.1f;
                pushConstants.color[3] = 1.0f;

                RecordFrameCommands(commandBuffer, *swapchainOwner.get(), imageIndex, *graphicsPipelineOwner,
                                    pushConstants);

                // for wait
                VkSemaphoreSubmitInfo imageAvailableSemaInfo{}; // binary semaphore라 value는 0 사용
                imageAvailableSemaInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
                imageAvailableSemaInfo.semaphore = imageAvailable;
                imageAvailableSemaInfo.stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

                VkCommandBufferSubmitInfo commandBufferSubmitInfo{};
                commandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
                commandBufferSubmitInfo.commandBuffer = commandBuffer;

                // for signaled
                VkSemaphore renderFinishedSemaphore = swapchainOwner->GetRenderFinished(imageIndex);
                VkSemaphoreSubmitInfo renderFinishedSemaInfo{}; // binary semaphore라 value는 0 사용
                renderFinishedSemaInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
                renderFinishedSemaInfo.semaphore = renderFinishedSemaphore;
                renderFinishedSemaInfo.stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

                VkSubmitInfo2 submitInfo{};
                submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
                submitInfo.commandBufferInfoCount = 1;
                submitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;
                submitInfo.waitSemaphoreInfoCount = 1;
                submitInfo.pWaitSemaphoreInfos = &imageAvailableSemaInfo;
                submitInfo.signalSemaphoreInfoCount = 1;
                submitInfo.pSignalSemaphoreInfos = &renderFinishedSemaInfo;

                VK_CHECK(vkResetFences(device, 1, &fence));
                VK_CHECK(vkQueueSubmit2(context.GetGraphicsQueue(), 1, &submitInfo, fence));

                VkPresentInfoKHR presentInfo{};
                presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
                presentInfo.waitSemaphoreCount = 1;
                presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
                presentInfo.swapchainCount = 1;
                presentInfo.pSwapchains = &sc;
                presentInfo.pImageIndices = &imageIndex;

                VkResult presentResult = vkQueuePresentKHR(context.GetPresentQueue(), &presentInfo);

                if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
                {
                    recreateSwapchain = true;
                }
                else
                {
                    VK_CHECK(presentResult);
                }

                frame.AdvanceFrameIndex();
            }
        }
        catch (...)
        {
            (void)vkDeviceWaitIdle(context.GetDevice());
            throw;
        }

        VK_CHECK(vkDeviceWaitIdle(context.GetDevice()));
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
