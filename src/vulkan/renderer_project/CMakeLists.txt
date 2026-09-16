cmake_minimum_required(VERSION 3.24)

project(RendererStudy LANGUAGES CXX)

find_package(Vulkan REQUIRED COMPONENTS glslc)
find_package(glfw3 REQUIRED CONFIG) # use GLFW's CMake Config

add_executable(vulkan_renderer
    main.cpp
    common/VulkanException.cpp

    core/VulkanContext.cpp
    core/Swapchain.cpp
    core/FrameResources.cpp
    core/ShaderModule.cpp
    core/GraphicsPipeline.cpp

    app/Window.cpp
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
    glfw
)

target_include_directories(
    vulkan_renderer
    PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")

target_compile_definitions(
    vulkan_renderer
    PRIVATE
    RENDERER_SHADER_DIR="${SHADER_OUTPUT_DIR}"
)

# shader build

add_custom_command(
    OUTPUT "${SHADER_OUTPUT_DIR}/triangle.vert.spv"

    COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADER_OUTPUT_DIR}"
    COMMAND Vulkan::glslc
            --target-env=vulkan1.3
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert"
            -o "${SHADER_OUTPUT_DIR}/triangle.vert.spv"

    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert"

    VERBATIM
)

add_custom_command(
    OUTPUT "${SHADER_OUTPUT_DIR}/triangle.frag.spv"

    COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADER_OUTPUT_DIR}"
    COMMAND Vulkan::glslc
            --target-env=vulkan1.3
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag"
            -o "${SHADER_OUTPUT_DIR}/triangle.frag.spv"

    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag"

    VERBATIM
)

add_custom_target(renderer_shaders DEPENDS
    "${SHADER_OUTPUT_DIR}/triangle.vert.spv"
    "${SHADER_OUTPUT_DIR}/triangle.frag.spv"
)

add_dependencies(vulkan_renderer renderer_shaders)
