#pragma once

#include <cstddef>

struct DrawPushConstants
{
    float positionAndSize[4]; // x, y, width, height
    float viewportSize[4];    // width, height, unused, unused
    float color[4];           // r, g, b, a
};

static_assert(sizeof(float) == 4);
static_assert(offsetof(DrawPushConstants, positionAndSize) == 0);
static_assert(offsetof(DrawPushConstants, viewportSize) == 16);
static_assert(offsetof(DrawPushConstants, color) == 32);
static_assert(sizeof(DrawPushConstants) == 48);
