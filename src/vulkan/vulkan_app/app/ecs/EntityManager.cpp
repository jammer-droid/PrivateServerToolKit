#include "EntityManager.h"

#include <vector>

SparseHandle EntityManager::CreateEntity()
{
    return entities_.Add(Entity{});
}

Entity *EntityManager::FindEntity(SparseHandle handle)
{
    return entities_.TryGet(handle);
}

const Entity *EntityManager::FindEntity(SparseHandle handle) const
{
    return entities_.TryGet(handle);
}

void EntityManager::DestroyEntity(SparseHandle handle)
{
    Entity *entity = entities_.TryGet(handle);
    if (entity)
    {
        entity->pendingDestroy = true;
    }
}

void EntityManager::FlushDestroyed()
{
    std::vector<SparseHandle> removed;
    ForEach([&removed](const SparseHandle &handle, Entity &value) {
        if (value.pendingDestroy)
        {
            removed.push_back(handle);
        }
    });

    for (SparseHandle &handle : removed)
    {
        entities_.Remove(handle);
    }
}
