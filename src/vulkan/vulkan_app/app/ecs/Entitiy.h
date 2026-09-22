#pragma once

#include "Components.h"

#include <optional>

struct Entity
{
    bool pendingDestroy{false};
    std::optional<Transform2D> transform;
    std::optional<Velocity2D> velocity;
};
