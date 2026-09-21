#pragma once

#include "Entitiy.h"

#include <vector>

using PackedUint = std::uint32_t;
using SparseHandle = std::uint32_t;

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
    bool Contain(SparseHandle handle);
    void Remove(SparseHandle handle);
    void Clear();

  private:
    void ResizeSparse();

  private:
    std::uint32_t head_{0};
    /*
     * MSB
     *      - 0: UNUSED
     *          - rest: next UNUSED index
     *      - 1: USED
     *          - rest: dense_ index
     *
     */
    std::vector<PackedUint> sparse_{};
    std::vector<DenseEntry> dense_{};
};
