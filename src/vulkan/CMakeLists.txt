cmake_minimum_required(VERSION 3.24)

project(VulkanStudy LANGUAGES CXX)

find_package(Vulkan REQUIRED)

add_executable(vulkan_study main.cpp)

target_compile_features(
    vulkan_study
    PRIVATE
    cxx_std_17
)

target_link_libraries(
    vulkan_study
    PRIVATE
    Vulkan::Vulkan
)
