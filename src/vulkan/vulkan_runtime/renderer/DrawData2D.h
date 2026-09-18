#pragma once

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <cstddef>
#include <cstdint>

enum class DrawShape2D : std::uint32_t
{
    Rectangle = 0,
    Circle = 1,
    Line = 2
};

union GeometryData2D {
    struct RectData2D
    {
        glm::vec2 position{0.0f}; // left top, framebuffer pixels
        glm::vec2 size{0.0f};
    } rect;

    struct LineData2D
    {
        glm::vec2 start{0.0f};
        glm::vec2 end{0.0f};
    } line;

    GeometryData2D() : rect{}
    {
    }

    explicit GeometryData2D(RectData2D value) : rect{value}
    {
    }

    explicit GeometryData2D(LineData2D value) : line{value}
    {
    }
};

struct DrawItem2D
{
    DrawShape2D shape{DrawShape2D::Rectangle};

    GeometryData2D geometry{};

    glm::vec4 color{1.0f}; // RGBA, straight alpha
    float thickness{1.0f}; // 선분의 두께, framebuffer 픽셀 단위
};

struct DrawData2DView
{
    // count가 0이면 nullptr을 허용한다. 그 외에는 count개 원소가 유효해야 한다.
    // items 배열 순서가 그리는 순서
    const DrawItem2D *items{nullptr};
    std::size_t count{0};
};
