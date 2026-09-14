#pragma once

#include "common/VulkanHeaders.h"

#include <vector>

// 선택한 GPU가 선택한 Surface에 출력할 수 있는 이미지 구성 조건
struct SwapchainSupport
{
    VkSurfaceCapabilitiesKHR capabilities{};        // 이미지 개수, 크기, 용도
    std::vector<VkSurfaceFormatKHR> surfaceFormats; // 이미지의 픽셀 포맷, color space 조건
    std::vector<VkPresentModeKHR> presentModes;     // 화면 출력 방식
};

SwapchainSupport QuerySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
