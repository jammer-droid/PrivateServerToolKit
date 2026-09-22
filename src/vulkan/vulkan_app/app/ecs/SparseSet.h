#pragma once

#include "Entitiy.h"

#include <vector>

using PackedUint = std::uint32_t;

struct SparseHandle
{
    std::uint32_t index{0};
    std::uint32_t generation{0};
};

struct SparseSlot
{
    PackedUint packed{0};
    std::uint32_t generation{1};
};

struct DenseEntry
{
    EntityId id;
    SparseHandle handle;
};

class SparseSet
{
  public:
    SparseSet();

  public:
    SparseHandle Add(EntityId id);
    bool Contain(SparseHandle handle) const;
    void Remove(SparseHandle handle);
    void Clear();
    bool TryGet(SparseHandle handle, EntityId *outId) const;
    std::uint32_t DenseSize() const
    {
        return (std::uint32_t)dense_.size();
    }

  private:
    void ResizeSparse();

  private:
    std::uint32_t head_{0}; // head of Free SparseSlot index's linked list
    /*
     * MSB
     *      - 0: UNUSED
     *          - rest: next UNUSED index
     *      - 1: USED
     *          - rest: dense_ index
     *
     */
    std::vector<SparseSlot> sparse_{};
    std::vector<DenseEntry> dense_{};
};
