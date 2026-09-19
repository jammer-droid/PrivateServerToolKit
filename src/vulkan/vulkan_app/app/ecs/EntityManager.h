#pragma once

#include "Entitiy.h"

#include <vector>

class EntityManager
{
  public:
    EntityManager() = default;
    ~EntityManager() = default;

    EntityId CreateEntity();

    Entity *FindEntity(EntityId id);
    const Entity *FindEntity(EntityId id) const;

    void DestroyEntity(EntityId id);
    void FlushDestroyed();

    std::vector<Entity> &GetEntities();

  private:
    EntityId nextId_{1};
    std::vector<Entity> entities_;
};
