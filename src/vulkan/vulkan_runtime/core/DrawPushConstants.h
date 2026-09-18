#pragma once

#include <cstddef>

struct DrawPushConstants
{
    float viewportSize[4]; // width, height, unused, unused
};

static_assert(sizeof(float) == 4);
static_assert(offsetof(DrawPushConstants, viewportSize) == 0);
static_assert(sizeof(DrawPushConstants) == 16);
