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
    sparse_.resize(initialSize, 0);
    dense_.reserve(initialSize);

    for (std::uint32_t i = 0; i < initialSize; i++)
    {
        PackedUint &packed = sparse_[i];
        packed = 0;
        packed |= (i + 1);
    }
}

SparseHandle SparseSet::Add(EntityId id)
{
    if (head_ == (std::uint32_t)sparse_.size())
    {
        ResizeSparse();
    }

    std::uint32_t sparseIdx = head_;
    PackedUint &packed = sparse_[sparseIdx];
    std::uint32_t nextSparseIdx = GetValue(packed);

    dense_.push_back(DenseEntry{id, sparseIdx});
    packed = 0;
    packed |= MSB;
    packed |= (dense_.size() - 1);

    head_ = nextSparseIdx;

    return sparseIdx;
}

bool SparseSet::Contain(SparseHandle handle)
{
    if (handle >= (std::uint32_t)sparse_.size())
    {
        return false;
    }

    return IsUsedSlot(sparse_[handle]);
}

void SparseSet::Remove(SparseHandle handle)
{
    if (!Contain(handle))
    {
        return;
    }

    PackedUint &packed = sparse_[handle];

    std::uint32_t denseIdx = GetValue(packed);
    std::uint32_t lastDenseIdx = (std::uint32_t)dense_.size() - 1;
    DenseEntry denseEntry = dense_[lastDenseIdx];

    PackedUint &densePacked = sparse_[denseEntry.handle];
    densePacked &= ~Mask;
    SetValue(densePacked, denseIdx);

    dense_[denseIdx] = denseEntry;
    dense_.pop_back();

    packed = 0;
    SetValue(packed, head_);

    head_ = handle;
}

void SparseSet::Clear()
{
    head_ = 0;
    std::uint32_t initialSize = 1024;
    sparse_.resize(initialSize, 0);
    dense_.clear();

    for (std::uint32_t i = 0; i < initialSize; i++)
    {
        PackedUint &packed = sparse_[i];
        packed = 0;
        packed |= (i + 1);
    }
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
        PackedUint &packed = sparse_[i];
        packed = 0;
        packed |= i + 1;
    }
    dense_.reserve(sz * 2);
}
