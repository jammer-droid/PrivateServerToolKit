#pragma once

#include "Components.h"
#include "Entitiy.h"

#include <vector>

inline void IntegrateMovement(Transform2D &transform, const Velocity2D &velocity, float dt)
{
    transform.position += (velocity.value * dt);
}

inline void UpdateMovement(std::vector<Entity> &entities, float dt)
{
    for (Entity &entity : entities)
    {
        if (entity.pendingDestroy)
        {
            continue;
        }
        if (entity.transform.has_value() && entity.velocity.has_value())
        {
            IntegrateMovement(entity.transform.value(), entity.velocity.value(), dt);
        }
    }
}
