#pragma once

#include "Components.h"

#include <cstdint>
#include <optional>

using EntityId = std::uint32_t;

struct Entity
{
    EntityId id{0};
    bool pendingDestroy{false};
    std::optional<Transform2D> transform;
    std::optional<Velocity2D> velocity;
};
