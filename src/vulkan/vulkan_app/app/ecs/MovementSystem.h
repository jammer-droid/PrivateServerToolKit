#pragma once

#include "Components.h"
#include "EntityManager.h"

using MovementFunc = void (*)(Transform2D &, const Velocity2D &, float);

inline void IntegrateMovement(Transform2D &transform, const Velocity2D &velocity, float dt)
{
    transform.position += (velocity.value * dt);
}

inline void UpdateMovement(EntityManager &manager, float dt)
{
    MovementFunc func = &IntegrateMovement;
    manager.ForEach([dt, &func](const SparseHandle &handle, Entity &value) {
        if (value.pendingDestroy)
        {
            return;
        }
        if (value.transform.has_value() && value.velocity.has_value())
        {
            func(value.transform.value(), value.velocity.value(), dt);
        }
    });
}
