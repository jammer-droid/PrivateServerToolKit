cmake_minimum_required(VERSION 3.24)

project(VulkanApp LANGUAGES CXX)

find_package(VulkanRuntime CONFIG REQUIRED)
find_package(Vulkan REQUIRED COMPONENTS glslc)

add_executable(vulkan_app
    main.cpp)

target_link_libraries(
    vulkan_app
    PRIVATE
    VulkanRuntime::Runtime
)

# SHADER_OUTPUT_DIR가 cache에 저장되어 있으면 덮지 않고, 없으면 여기에서 지정한 값을 사용
set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders" CACHE PATH
    "Directory for app-compiled runtime shaders")

target_compile_definitions(
    vulkan_app
    PRIVATE
    RENDERER_SHADER_DIR="${SHADER_OUTPUT_DIR}"
)

# shader build

add_custom_command(
    OUTPUT "${SHADER_OUTPUT_DIR}/triangle.vert.spv"

    COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADER_OUTPUT_DIR}"
    COMMAND Vulkan::glslc
            --target-env=vulkan1.3
            "${VulkanRuntime_SHADER_SOURCE_DIR}/triangle.vert"
            -o "${SHADER_OUTPUT_DIR}/triangle.vert.spv"

    DEPENDS "${VulkanRuntime_SHADER_SOURCE_DIR}/triangle.vert"

    VERBATIM
)

add_custom_command(
    OUTPUT "${SHADER_OUTPUT_DIR}/triangle.frag.spv"

    COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADER_OUTPUT_DIR}"
    COMMAND Vulkan::glslc
            --target-env=vulkan1.3
            "${VulkanRuntime_SHADER_SOURCE_DIR}/triangle.frag"
            -o "${SHADER_OUTPUT_DIR}/triangle.frag.spv"

    DEPENDS "${VulkanRuntime_SHADER_SOURCE_DIR}/triangle.frag"

    VERBATIM
)

add_custom_target(compile_shaders DEPENDS
    "${SHADER_OUTPUT_DIR}/triangle.vert.spv"
    "${SHADER_OUTPUT_DIR}/triangle.frag.spv"
)

add_dependencies(vulkan_app compile_shaders)
