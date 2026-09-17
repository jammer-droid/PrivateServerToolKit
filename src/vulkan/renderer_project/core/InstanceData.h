#pragma once

#include <cstddef>

struct InstanceData
{
    float positionAndSize[4]; // x, y, width, height
    float color[4];           // r, g, b, a
};

static_assert(sizeof(float) == 4);
static_assert(offsetof(InstanceData, positionAndSize) == 0);
static_assert(offsetof(InstanceData, color) == 16);
static_assert(sizeof(InstanceData) == 32);
