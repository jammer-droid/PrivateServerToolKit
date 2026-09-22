#pragma once

#include "Entitiy.h"
#include "SparseSet.h"

#include <utility>

class EntityManager
{
  public:
    EntityManager() = default;
    ~EntityManager() = default;

    SparseHandle CreateEntity();
    Entity *FindEntity(SparseHandle handle);
    const Entity *FindEntity(SparseHandle handle) const;
    void DestroyEntity(SparseHandle handle);
    void FlushDestroyed();

    template <typename Func> void ForEach(Func &&func)
    {
        entities_.ForEach(std::forward<Func>(func));
    }

    template <typename Func> void ForEach(Func &&func) const
    {
        entities_.ForEach(std::forward<Func>(func));
    }

  private:
    SparseSet<Entity> entities_;
};
