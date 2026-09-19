#include "EntityManager.h"

#include <algorithm>
#include <stdexcept>

EntityId EntityManager::CreateEntity()
{
    if (nextId_ == 0)
    {
        throw std::overflow_error("Id is overflowed");
    }

    Entity entity{};

    entity.id = nextId_;

    entities_.push_back(entity);

    nextId_++;
    return entity.id;
}

Entity *EntityManager::FindEntity(EntityId id)
{
    for (Entity &entity : entities_)
    {
        if (entity.id == id)
        {
            return &entity;
        }
    }

    return nullptr;
}

const Entity *EntityManager::FindEntity(EntityId id) const
{
    for (const Entity &entity : entities_)
    {
        if (entity.id == id)
        {
            return &entity;
        }
    }

    return nullptr;
}

void EntityManager::DestroyEntity(EntityId id)
{
    for (Entity &entity : entities_)
    {
        if (entity.id == id)
        {
            entity.pendingDestroy = true;
        }
    }
}

void EntityManager::FlushDestroyed()
{
    auto iter =
        std::remove_if(entities_.begin(), entities_.end(), [](Entity &entity) { return entity.pendingDestroy; });
    entities_.erase(iter, entities_.end());
}

std::vector<Entity> &EntityManager::GetEntities()
{
    return entities_;
}
