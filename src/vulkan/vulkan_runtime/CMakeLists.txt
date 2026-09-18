cmake_minimum_required(VERSION 3.24)

project(VulkanRuntime LANGUAGES CXX)

include(GNUInstallDirs) #  lib, include, bin, share 같은 설치 경로 변수를 제공.
include(CMakePackageConfigHelpers) # 다른 프로젝트 find_package가 읽을 설정 파일 생성 도구 제공.

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED CONFIG)

add_library(vulkan_runtime STATIC
    common/VulkanException.cpp

    core/VulkanContext.cpp
    core/Swapchain.cpp
    core/FrameResources.cpp
    core/ShaderModule.cpp
    core/GraphicsPipeline.cpp
    core/HostVisibleBuffer.cpp

    renderer/Renderer2D.cpp

    app/Window.cpp
)

add_library(VulkanRuntime::Runtime ALIAS vulkan_runtime)

# 외부 사용자가 사용할 이름을 Runtime으로 설정
set_target_properties(vulkan_runtime PROPERTIES EXPORT_NAME Runtime)

target_compile_features(
    vulkan_runtime PUBLIC cxx_std_17
)

target_include_directories(
    vulkan_runtime
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(
    vulkan_runtime
    PUBLIC Vulkan::Vulkan
    PRIVATE glfw
)

# install 시에 내보낼 타겟을 목록에 등록
# vulkan_runtime을 설치하고, 타겟 정보를 VulkanRuntimeTargets라는 export 목록에 등록
install(TARGETS vulkan_runtime
    EXPORT VulkanRuntimeTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Transitional headers required by the current app.
# Narrow this set with the Application API.
install(DIRECTORY app common core renderer
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

install(DIRECTORY shaders/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/VulkanRuntime/shaders
    FILES_MATCHING
        PATTERN "*.vert"
        PATTERN "*.frag"
)

set(
    VULKAN_RUNTIME_SHADER_INSTALL_DIR
    "${CMAKE_INSTALL_DATADIR}/VulkanRuntime/shaders")
set(
    VULKAN_RUNTIME_PACKAGE_DIR
    "${CMAKE_INSTALL_LIBDIR}/cmake/VulkanRuntime")

# find_package 진입 지점
configure_package_config_file(
    cmake/VulkanRuntimeConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/VulkanRuntimeConfig.cmake"
    INSTALL_DESTINATION "${VULKAN_RUNTIME_PACKAGE_DIR}"
    PATH_VARS VULKAN_RUNTIME_SHADER_INSTALL_DIR
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/VulkanRuntimeConfig.cmake"
    DESTINATION "${VULKAN_RUNTIME_PACKAGE_DIR}"
)

# EXPORT: 앞에 등록한 exports 목록에서 VulkanRuntimeTargets 사용
# FILE: 타겟 정보를 담을 파일 이름
# NAMESPACE: 내보낸 타겟 이름 앞에 붙일 접두사
# DESTINATION: 파일을 설치할 위치
# 외부 사용자는 NAMESPACE에 EXPORT_NAME을 붙여 사용
install(EXPORT VulkanRuntimeTargets
    FILE VulkanRuntimeTargets.cmake
    NAMESPACE VulkanRuntime::
    DESTINATION "${VULKAN_RUNTIME_PACKAGE_DIR}"
)
