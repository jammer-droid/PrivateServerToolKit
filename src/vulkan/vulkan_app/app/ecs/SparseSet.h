#pragma once

#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <utility>
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

template <typename T> class SparseSet
{
    static_assert(std::is_nothrow_move_constructible_v<T>, "SparseSet requires nothrow move construction");
    static_assert(std::is_nothrow_move_assignable_v<T>, "SparseSet requires nothrow move assignment");

  public:
    SparseSet()
    {
        constexpr std::uint32_t InitialSize = 1024;
        sparse_.resize(InitialSize);
        dense_.reserve(InitialSize);
        for (std::uint32_t i = 0; i < InitialSize; ++i)
        {
            sparse_[i].packed = i + 1;
        }
    }

    SparseHandle Add(T value)
    {
        if (head_ == sparse_.size())
        {
            ResizeSparse();
        }

        const std::uint32_t sparseIdx = head_;
        SparseSlot &slot = sparse_[sparseIdx];
        const std::uint32_t nextSparseIdx = GetValue(slot.packed);
        const SparseHandle handle{sparseIdx, slot.generation};

        dense_.push_back(DenseEntry{std::move(value), handle});
        slot.packed = MSB | static_cast<std::uint32_t>(dense_.size() - 1);
        head_ = nextSparseIdx;
        return handle;
    }

    bool Contain(SparseHandle handle) const
    {
        if (handle.index >= sparse_.size())
        {
            return false;
        }

        const SparseSlot &slot = sparse_[handle.index];
        if (!IsUsedSlot(slot.packed) || slot.generation != handle.generation)
        {
            return false;
        }

        const std::uint32_t denseIdx = GetValue(slot.packed);
        return denseIdx < dense_.size() && dense_[denseIdx].handle.index == handle.index &&
               dense_[denseIdx].handle.generation == handle.generation;
    }

    void Remove(SparseHandle handle)
    {
        if (!Contain(handle))
        {
            return;
        }

        SparseSlot &slot = sparse_[handle.index];
        const std::uint32_t denseIdx = GetValue(slot.packed);
        const std::uint32_t lastDenseIdx = static_cast<std::uint32_t>(dense_.size() - 1);
        if (denseIdx != lastDenseIdx)
        {
            dense_[denseIdx] = std::move(dense_.back());
            sparse_[dense_[denseIdx].handle.index].packed = MSB | denseIdx;
        }
        dense_.pop_back();

        slot.packed = head_;
        ++slot.generation;
        head_ = handle.index;
    }

    void Clear()
    {
        dense_.clear();
        for (std::uint32_t i = 0; i < sparse_.size(); ++i)
        {
            sparse_[i].packed = i + 1;
            ++sparse_[i].generation;
        }
        head_ = 0;
    }

    // Borrowed pointers: reacquire through the handle after structural changes.
    T *TryGet(SparseHandle handle)
    {
        if (!Contain(handle))
        {
            return nullptr;
        }
        return &dense_[GetValue(sparse_[handle.index].packed)].value;
    }

    const T *TryGet(SparseHandle handle) const
    {
        if (!Contain(handle))
        {
            return nullptr;
        }
        return &dense_[GetValue(sparse_[handle.index].packed)].value;
    }

    std::uint32_t DenseSize() const
    {
        return static_cast<std::uint32_t>(dense_.size());
    }

  private:
    struct DenseEntry
    {
        T value;
        SparseHandle handle;
    };

    static constexpr std::uint32_t MSB = 1u << 31u;
    static constexpr std::uint32_t Mask = ~MSB;

    static bool IsUsedSlot(PackedUint packed)
    {
        return (packed & MSB) != 0;
    }

    static std::uint32_t GetValue(PackedUint packed)
    {
        return packed & Mask;
    }

    void ResizeSparse()
    {
        const std::uint32_t size = static_cast<std::uint32_t>(sparse_.size());
        if (size > Mask / 2)
        {
            throw std::overflow_error("Sparse capacity exceeded");
        }

        sparse_.resize(size * 2);
        for (std::uint32_t i = size; i < size * 2; ++i)
        {
            sparse_[i].packed = i + 1;
        }
    }

    std::uint32_t head_{0}; // First free sparse slot; sparse_.size() marks the end.

    // MSB: occupied flag. Remaining bits: dense index or next free sparse index.
    // Generation wraparound is outside the current contract.
    std::vector<SparseSlot> sparse_;
    std::vector<DenseEntry> dense_;
};
