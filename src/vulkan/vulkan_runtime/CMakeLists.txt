cmake_minimum_required(VERSION 3.24)

project(VulkanRuntime LANGUAGES CXX)

include(GNUInstallDirs) #  lib, include, bin, share 같은 설치 경로 변수를 제공.
include(CMakePackageConfigHelpers) # 다른 프로젝트 find_package가 읽을 설정 파일 생성 도구 제공.
include(GenerateExportHeader)

find_package(Vulkan REQUIRED COMPONENTS glslc)
find_package(glfw3 REQUIRED CONFIG)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_library(vulkan_runtime SHARED
    common/VulkanException.cpp

    core/VulkanContext.cpp
    core/Swapchain.cpp
    core/FrameResources.cpp
    core/ShaderModule.cpp
    core/GraphicsPipeline.cpp
    core/HostVisibleBuffer.cpp

    renderer/Renderer2D.cpp

    app/Window.cpp
    app/Application.cpp
)

add_library(VulkanRuntime::Runtime ALIAS vulkan_runtime)

# 외부 사용자가 사용할 이름을 Runtime으로 설정
set_target_properties(vulkan_runtime PROPERTIES
    EXPORT_NAME Runtime
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)

# Runtime에서 Vulkan, glfw 공유 라이브러리 설치 경로를 사용할 수 있도록 설정
if(UNIX)
    set_property(TARGET vulkan_runtime PROPERTY INSTALL_RPATH
        "$<TARGET_FILE_DIR:Vulkan::Vulkan>"
        "$<TARGET_FILE_DIR:glfw>"
    )
endif()

generate_export_header(vulkan_runtime
    EXPORT_FILE_NAME "generated/VulkanRuntimeExport.h"
    EXPORT_MACRO_NAME VULKAN_RUNTIME_API
)

target_compile_features(
    vulkan_runtime PUBLIC cxx_std_17
)

target_include_directories(
    vulkan_runtime
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/runtime>
)

target_link_libraries(
    vulkan_runtime
    PRIVATE
    Vulkan::Vulkan
    glfw
)

# install 시에 내보낼 타겟을 목록에 등록
# vulkan_runtime을 설치하고, 타겟 정보를 VulkanRuntimeTargets라는 export 목록에 등록
install(TARGETS vulkan_runtime
    EXPORT VulkanRuntimeTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/VulkanRuntimeExport.h"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES app/Application.h app/IGame.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/runtime/app
)
install(FILES renderer/DrawData2D.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/runtime/renderer
)
install(FILES common/ClassTraits.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/runtime/common
)

set(VULKAN_RUNTIME_BUILTIN_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders/runtime")
set(VULKAN_RUNTIME_SHADER_INSTALL_DIR "shaders/runtime")
set(VULKAN_RUNTIME_SHADER_ROOT_INSTALL_DIR "shaders")
set(VULKAN_RUNTIME_SHADER_SCRIPT_INSTALL_PATH
    "${CMAKE_INSTALL_DATADIR}/VulkanRuntime/tools/compile_shaders.py")
set(VULKAN_RUNTIME_SHADER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/../script/compile_shaders.py")

# This target runs after the library, including shader-only edits without relinking.
add_custom_target(vulkan_runtime_assets ALL
    COMMAND Python3::Interpreter "${VULKAN_RUNTIME_SHADER_SCRIPT}"
        -s "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        -o "${VULKAN_RUNTIME_BUILTIN_SHADER_DIR}"
        --target-env vulkan1.3 --glslc "$<TARGET_FILE:Vulkan::glslc>"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${CMAKE_INSTALL_PREFIX}/${VULKAN_RUNTIME_SHADER_INSTALL_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${VULKAN_RUNTIME_BUILTIN_SHADER_DIR}"
        "${CMAKE_INSTALL_PREFIX}/${VULKAN_RUNTIME_SHADER_INSTALL_DIR}"
    VERBATIM
)
add_dependencies(vulkan_runtime_assets vulkan_runtime)

install(DIRECTORY "${VULKAN_RUNTIME_BUILTIN_SHADER_DIR}/"
    DESTINATION "${VULKAN_RUNTIME_SHADER_INSTALL_DIR}"
    FILES_MATCHING PATTERN "*.spv"
)
install(FILES "${VULKAN_RUNTIME_SHADER_SCRIPT}"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/VulkanRuntime/tools"
)
set(
    VULKAN_RUNTIME_PACKAGE_DIR
    "${CMAKE_INSTALL_LIBDIR}/cmake/VulkanRuntime")

# find_package 진입 지점
configure_package_config_file(
    cmake/VulkanRuntimeConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/VulkanRuntimeConfig.cmake"
    INSTALL_DESTINATION "${VULKAN_RUNTIME_PACKAGE_DIR}"
    PATH_VARS
        VULKAN_RUNTIME_SHADER_INSTALL_DIR
        VULKAN_RUNTIME_SHADER_ROOT_INSTALL_DIR
        VULKAN_RUNTIME_SHADER_SCRIPT_INSTALL_PATH
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
