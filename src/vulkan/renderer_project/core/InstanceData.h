#pragma once

#include <cstdint>
#include <cstddef>

enum class Shape : std::uint32_t
{
    Rectangle = 0,
    Circle = 1
};

struct InstanceData
{
    float positionAndSize[4]; // x, y, width, height
    float color[4];           // r, g, b, a
    Shape shape;
    float padding[3];
};

static_assert(sizeof(float) == 4);
static_assert(offsetof(InstanceData, positionAndSize) == 0);
static_assert(offsetof(InstanceData, color) == 16);
static_assert(offsetof(InstanceData, shape) == 32);
static_assert(sizeof(InstanceData) == 48);
