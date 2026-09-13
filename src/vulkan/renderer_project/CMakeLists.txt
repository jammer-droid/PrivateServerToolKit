cmake_minimum_required(VERSION 3.24)

project(RendererStudy LANGUAGES CXX)

find_package(Vulkan REQUIRED)

add_executable(vulkan_renderer
    main.cpp
    common/VulkanException.cpp
    core/VulkanContext.cpp
)

target_compile_features(
    vulkan_renderer
    PRIVATE
    cxx_std_17
)

target_link_libraries(
    vulkan_renderer
    PRIVATE
    Vulkan::Vulkan
)

target_include_directories(
    vulkan_renderer
    PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
