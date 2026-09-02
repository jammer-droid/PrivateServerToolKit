#pragma once

#include <pstk/TkResult.h>

#include <atomic>
#include <cassert>
#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace pstk::execution
{

template <typename T> class TkBoundedMpmcQueue final
{
    static_assert(std::is_nothrow_move_constructible<T>::value,
                  "TkBoundedMpmcQueue requires nothrow move construction");
    static_assert(std::is_nothrow_move_assignable<T>::value, "TkBoundedMpmcQueue requires nothrow move assignment");
    static_assert(std::is_nothrow_destructible<T>::value, "TkBoundedMpmcQueue requires nothrow destruction");

    struct Slot
    {
        Slot() noexcept : sequence(0)
        {
        }

        std::atomic<std::size_t> sequence;
        alignas(T) std::byte storage[sizeof(T)];
    };

  public:
    static TkResult Create(const std::size_t capacity, std::unique_ptr<TkBoundedMpmcQueue<T>> *const outQueue) noexcept
    {
        if (outQueue == nullptr || !IsValidCapacity(capacity))
        {
            return TK_ERROR_INVALID_ARGUMENT;
        }

        try
        {
            std::unique_ptr<TkBoundedMpmcQueue<T>> queue(new TkBoundedMpmcQueue<T>(capacity));
            *outQueue = std::move(queue);
        }
        catch (const std::bad_alloc &)
        {
            return TK_ERROR_OUT_OF_MEMORY;
        }

        return TK_SUCCESS;
    }

    ~TkBoundedMpmcQueue() noexcept
    {
        std::size_t position = dequeuePosition_.load(std::memory_order_relaxed);
        const std::size_t endPosition = enqueuePosition_.load(std::memory_order_relaxed);
        while (position != endPosition)
        {
            Slot &slot = slots_[position & capacityMask_];
            ItemAt(slot)->~T();
            ++position;
        }
    }

    TkBoundedMpmcQueue(const TkBoundedMpmcQueue &) = delete;
    TkBoundedMpmcQueue &operator=(const TkBoundedMpmcQueue &) = delete;

    TkBoundedMpmcQueue(TkBoundedMpmcQueue &&) = delete;
    TkBoundedMpmcQueue &operator=(TkBoundedMpmcQueue &&) = delete;

    bool TryPush(T &&value) noexcept
    {
        std::size_t position = enqueuePosition_.load(std::memory_order_relaxed);
        Slot *slot = nullptr;

        while (true)
        {
            slot = &slots_[position & capacityMask_];
            const std::size_t sequence = slot->sequence.load(std::memory_order_acquire);
            const std::ptrdiff_t difference = SequenceDifference(sequence, position);

            if (difference == 0)
            {
                if (enqueuePosition_.compare_exchange_weak(position, position + 1, std::memory_order_relaxed,
                                                           std::memory_order_relaxed))
                {
                    break;
                }
            }
            else if (difference < 0)
            {
                return false;
            }
            else // difference > 0
            {
                position = enqueuePosition_.load(std::memory_order_relaxed);
            }
        }

        // call move-constructor with placement-new (::new)
        ::new (static_cast<void *>(&slot->storage)) T(std::move(value));

        // After push complete
        // current slot's validate sequence is current sequence + 1.
        slot->sequence.store(position + 1, std::memory_order_release);

        return true;
    }

    bool TryPop(T *const outItem) noexcept
    {
        assert(outItem != nullptr);

        std::size_t position = dequeuePosition_.load(std::memory_order_relaxed);
        Slot *slot = nullptr;

        while (true)
        {
            slot = &slots_[position & capacityMask_];
            const std::size_t sequence = slot->sequence.load(std::memory_order_acquire);
            const std::ptrdiff_t difference = SequenceDifference(sequence, position + 1);

            if (difference == 0)
            {
                if (dequeuePosition_.compare_exchange_weak(position, position + 1, std::memory_order_relaxed,
                                                           std::memory_order_relaxed))
                {
                    break;
                }
            }
            else if (difference < 0)
            {
                return false;
            }
            else
            {
                position = dequeuePosition_.load(std::memory_order_relaxed);
            }
        }

        T *const item = ItemAt(*slot);
        *outItem = std::move(*item);
        item->~T();
        slot->sequence.store(position + capacity_, std::memory_order_release);
        return true;
    }

    // 현재 dequeuePosition이 가리키는 Slot이 Pop을 할 수 있는 상태인지 확인.
    // 결과값을 반환하는 과정에서 Slot의 상태가 변경될 수 있음.
    bool HasReadyItem() const noexcept
    {
        const std::size_t position = dequeuePosition_.load(std::memory_order_relaxed);

        const Slot &slot = slots_[position & capacityMask_];
        const std::size_t sequence = slot.sequence.load(std::memory_order_acquire);

        return SequenceDifference(sequence, position + 1) == 0;
    }

  private:
    explicit TkBoundedMpmcQueue(const std::size_t capacity)
        : slots_(new Slot[capacity]), capacity_(capacity), capacityMask_(capacity - 1)
    {
        enqueuePosition_.store(0, std::memory_order_relaxed);
        dequeuePosition_.store(0, std::memory_order_relaxed);

        for (std::size_t index = 0; index < capacity; ++index)
        {
            slots_[index].sequence.store(index, std::memory_order_relaxed);
        }
    }

    static bool IsValidCapacity(const std::size_t capacity) noexcept
    {
        return capacity >= 2 && (capacity & (capacity - 1)) == 0 && capacity <= MaxCapacity();
    }

    static constexpr std::size_t MaxCapacity() noexcept
    {
        return std::numeric_limits<std::size_t>::max() / sizeof(Slot);
    }

    /*
     * # 1. sequence == position (diff == 0)
     *      - validate position
     *
     * # 2. sequence > position (diff > 0)
     *      - somebody pushed item already
     *
     * # 3. sequence < position (diff < 0)
     *      - queueu full or something wrong
     */

    static std::ptrdiff_t SequenceDifference(const std::size_t sequence, const std::size_t position) noexcept
    {
        return static_cast<std::ptrdiff_t>(sequence) - static_cast<std::ptrdiff_t>(position);
    }

    static T *ItemAt(Slot &slot) noexcept
    {
        return std::launder(reinterpret_cast<T *>(&slot.storage));
    }

    std::unique_ptr<Slot[]> slots_;
    const std::size_t capacity_;
    const std::size_t capacityMask_;

    std::atomic<std::size_t> enqueuePosition_;
    std::atomic<std::size_t> dequeuePosition_;
};
} // namespace pstk::execution
