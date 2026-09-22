#include "SparseSet.h"

#include <stdexcept>

namespace
{

constexpr std::uint32_t MSB = (1u << 31u);
constexpr std::uint32_t Mask = ~MSB;

bool IsUsedSlot(PackedUint packed)
{
    return (packed & MSB) != 0;
}

std::uint32_t GetValue(PackedUint packed)
{
    return (packed & Mask);
}

void SetValue(PackedUint &packed, std::uint32_t value)
{
    packed |= value;
}

}; // namespace

SparseSet::SparseSet()
{
    std::uint32_t initialSize = 1024;
    sparse_.resize(initialSize, {0, 0});
    dense_.reserve(initialSize);

    for (std::uint32_t i = 0; i < initialSize; i++)
    {
        SparseSlot &slot = sparse_[i];
        slot.packed = 0;
        slot.packed |= (i + 1);
        slot.generation = 1;
    }
}

SparseHandle SparseSet::Add(EntityId id)
{
    if (head_ == (std::uint32_t)sparse_.size())
    {
        ResizeSparse();
    }

    std::uint32_t sparseIdx = head_;
    SparseSlot &slot = sparse_[sparseIdx];

    std::uint32_t nextSparseIdx = GetValue(slot.packed);

    dense_.push_back(DenseEntry{id, {sparseIdx, slot.generation}});

    // update to used
    slot.packed = 0;
    slot.packed |= MSB;
    slot.packed |= (dense_.size() - 1);

    head_ = nextSparseIdx;

    return {sparseIdx, slot.generation};
}

bool SparseSet::Contain(SparseHandle handle) const
{
    if (handle.index >= (std::uint32_t)sparse_.size())
    {
        return false;
    }

    const SparseSlot &slot = sparse_[handle.index];
    if (slot.generation != handle.generation)
    {
        return false;
    }

    return IsUsedSlot(slot.packed) && (dense_[GetValue(slot.packed)].handle.generation == slot.generation);
}

void SparseSet::Remove(SparseHandle handle)
{
    if (!Contain(handle))
    {
        return;
    }

    // removed slot
    SparseSlot &slot = sparse_[handle.index];

    std::uint32_t denseIdx = GetValue(slot.packed);
    std::uint32_t lastDenseIdx = (std::uint32_t)dense_.size() - 1;
    DenseEntry denseEntry = dense_[lastDenseIdx];

    SparseSlot &dereferSlot = sparse_[denseEntry.handle.index];
    dereferSlot.packed &= ~Mask;
    SetValue(dereferSlot.packed, denseIdx);

    dense_[denseIdx] = denseEntry;
    dense_.pop_back();

    slot.packed = 0;
    slot.generation++;
    SetValue(slot.packed, head_);

    head_ = handle.index;
}

void SparseSet::Clear()
{
    head_ = 0;
    dense_.clear();

    for (std::uint32_t i = 0; i < (std::uint32_t)sparse_.size(); i++)
    {
        SparseSlot &slot = sparse_[i];
        slot.packed = 0;
        slot.packed |= (i + 1);
        slot.generation++;
    }
}

bool SparseSet::TryGet(SparseHandle handle, EntityId *outId) const
{
    if (!Contain(handle) || outId == nullptr)
    {
        return false;
    }

    *outId = dense_[GetValue(sparse_[handle.index].packed)].id;
    return true;
}

void SparseSet::ResizeSparse()
{
    // double size
    std::uint32_t sz = sparse_.size();
    if (sz > Mask / 2)
    {
        throw std::overflow_error("Sparse capacity exceeded");
    }

    sparse_.resize(sz * 2);
    for (std::uint32_t i = sz; i < sz * 2; i++)
    {
        SparseSlot &slot = sparse_[i];
        slot.packed = 0;
        slot.packed |= (i + 1);
        slot.generation = 1;
    }
    dense_.reserve(sz * 2);
}
