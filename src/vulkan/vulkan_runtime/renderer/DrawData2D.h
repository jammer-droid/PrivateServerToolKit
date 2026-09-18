#pragma once

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
        float x; // left top x
        float y; // left top y
        float width;
        float height;
    } rect{};

    struct LineData2D
    {
        float startX;
        float startY;
        float endX;
        float endY;
    } line;
};

struct DrawItem2D
{
    DrawShape2D shape{DrawShape2D::Rectangle};

    GeometryData2D geometry{};

    float color[4]{1.0f, 1.0f, 1.0f, 1.0f}; // RGBA, straight alpha
    float thickness{1.0f};                  // 선분의 두께, framebuffer 픽셀 단위
};

struct DrawData2DView
{
    // count가 0이면 nullptr을 허용한다. 그 외에는 count개 원소가 유효해야 한다.
    // items 배열 순서가 그리는 순서
    const DrawItem2D *items{nullptr};
    std::size_t count{0};
};
