#pragma once

#include "SparseSet.h"

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

// The owner validates Entity lifetime and removes components before reusing a slot.
// Structural changes invalidate borrowed references and must occur outside ForEach.
template <typename T> class ComponentPool
{
    static_assert(std::is_nothrow_move_constructible_v<T>, "ComponentPool requires nothrow move construction");
    static_assert(std::is_nothrow_move_assignable_v<T>, "ComponentPool requires nothrow move assignment");

  public:
    // Reject an occupied index, including a different generation, without replacing its value.
    template <typename... Args> T &Emplace(SparseHandle handle, Args &&...args)
    {
        const std::size_t index = handle.index;
        if (index < sparse_.size() && sparse_[index] != Missing)
        {
            throw std::invalid_argument("Component index already occupied");
        }
        if (index >= sparse_.max_size())
        {
            throw std::length_error("Component sparse capacity exceeded");
        }
        if (index >= sparse_.size())
        {
            sparse_.resize(index + 1, Missing);
        }

        dense_.emplace_back(handle, std::forward<Args>(args)...);
        sparse_[index] = dense_.size() - 1;
        return dense_.back().value;
    }

    bool Contains(SparseHandle handle) const
    {
        if (handle.index >= sparse_.size())
        {
            return false;
        }
        const std::size_t denseIndex = sparse_[handle.index];
        return denseIndex < dense_.size() && dense_[denseIndex].handle.index == handle.index &&
               dense_[denseIndex].handle.generation == handle.generation;
    }

    void Remove(SparseHandle handle)
    {
        if (!Contains(handle))
        {
            return;
        }
        const std::size_t denseIndex = sparse_[handle.index];
        if (denseIndex != dense_.size() - 1)
        {
            dense_[denseIndex] = std::move(dense_.back());
            sparse_[dense_[denseIndex].handle.index] = denseIndex;
        }
        dense_.pop_back();
        sparse_[handle.index] = Missing;
    }

    T *TryGet(SparseHandle handle)
    {
        return Contains(handle) ? &dense_[sparse_[handle.index]].value : nullptr;
    }

    const T *TryGet(SparseHandle handle) const
    {
        return Contains(handle) ? &dense_[sparse_[handle.index]].value : nullptr;
    }

    template <typename Func> void ForEach(Func &&func)
    {
        for (DenseEntry &entry : dense_)
        {
            func(std::as_const(entry.handle), entry.value);
        }
    }

    template <typename Func> void ForEach(Func &&func) const
    {
        for (const DenseEntry &entry : dense_)
        {
            func(entry.handle, entry.value);
        }
    }

    std::size_t DenseSize() const
    {
        return dense_.size();
    }

    void Clear()
    {
        for (const DenseEntry &entry : dense_)
        {
            sparse_[entry.handle.index] = Missing;
        }
        dense_.clear();
    }

  private:
    struct DenseEntry
    {
        template <typename... Args>
        DenseEntry(SparseHandle entity, Args &&...args) : value(std::forward<Args>(args)...), handle(entity)
        {
        }

        T value;
        SparseHandle handle;
    };

    static constexpr std::size_t Missing = std::numeric_limits<std::size_t>::max();

    std::vector<std::size_t> sparse_;
    std::vector<DenseEntry> dense_;
};
