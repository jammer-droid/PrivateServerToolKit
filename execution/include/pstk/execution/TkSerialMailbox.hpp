#pragma once

#include <pstk/TkResult.h>
#include <pstk/execution/TkBoundedMpmcQueue.hpp>

#include <atomic>
#include <cstddef>
#include <memory>
#include <new>
#include <utility>

namespace pstk::execution
{

template <typename T> class TkSerialMailbox final
{
    using Queue = TkBoundedMpmcQueue<T>;

    enum class State
    {
        Idle,
        Scheduled,
        Draining
    };

  public:
    static TkResult Create(const std::size_t capacity, std::unique_ptr<TkSerialMailbox<T>> *const outMailbox) noexcept
    {
        if (outMailbox == nullptr)
        {
            return TK_ERROR_INVALID_ARGUMENT;
        }

        std::unique_ptr<Queue> queue;
        const TkResult queueResult = Queue::Create(capacity, &queue);
        if (queueResult != TK_SUCCESS)
        {
            return queueResult;
        }

        try
        {
            std::unique_ptr<TkSerialMailbox<T>> mailbox(new TkSerialMailbox<T>(std::move(queue)));
            *outMailbox = std::move(mailbox);
        }
        catch (const std::bad_alloc &)
        {
            return TK_ERROR_OUT_OF_MEMORY;
        }

        return TK_SUCCESS;
    }

    ~TkSerialMailbox() noexcept = default;

    TkSerialMailbox(const TkSerialMailbox &) = delete;
    TkSerialMailbox &operator=(const TkSerialMailbox &) = delete;

    TkSerialMailbox(TkSerialMailbox &&) = delete;
    TkSerialMailbox &operator=(TkSerialMailbox &&) = delete;

    TkResult TryPublish(T &&item, bool *const outShouldSchedule) noexcept
    {
        if (outShouldSchedule == nullptr)
        {
            return TK_ERROR_INVALID_ARGUMENT;
        }

        if (!queue_->TryPush(std::move(item)))
        {
            return TK_ERROR_CAPACITY_EXCEEDED;
        }

        State expected = State::Idle;
        const bool shouldSchedule = state_.compare_exchange_strong(
            expected, State::Scheduled, std::memory_order_acq_rel, std::memory_order_relaxed);

        *outShouldSchedule = shouldSchedule;
        return TK_SUCCESS;
    }

    // State
    // Scheduled -> Draining
    TkResult BeginDrain() noexcept
    {
        State expected = State::Scheduled;
        if (!state_.compare_exchange_strong(expected, State::Draining, std::memory_order_acquire,
                                            std::memory_order_relaxed))
        {
            return TK_ERROR_INVALID_STATE;
        }

        return TK_SUCCESS;
    }

    bool TryPop(T *const outItem) noexcept
    {
        return queue_->TryPop(outItem);
    }

    // State
    // Draining -> Idle
    TkResult FinishDrain(bool *const outShouldSchedule) noexcept
    {
        if (outShouldSchedule == nullptr)
        {
            return TK_ERROR_INVALID_ARGUMENT;
        }

        State expected = State::Draining;
        if (!state_.compare_exchange_strong(expected, State::Idle, std::memory_order_release,
                                            std::memory_order_relaxed))
        {
            return TK_ERROR_INVALID_STATE;
        }

        bool shouldSchedule = false;
        if (queue_->HasReadyItem())
        {
            expected = State::Idle;
            shouldSchedule = state_.compare_exchange_strong(expected, State::Scheduled, std::memory_order_release,
                                                            std::memory_order_relaxed);
        }

        *outShouldSchedule = shouldSchedule;
        return TK_SUCCESS;
    }

    bool IsQuiescent() const noexcept
    {
        return state_.load(std::memory_order_acquire) == State::Idle && !queue_->HasReadyItem();
    }

  private:
    explicit TkSerialMailbox(std::unique_ptr<Queue> queue) noexcept : queue_(std::move(queue)), state_(State::Idle)
    {
    }

    std::unique_ptr<Queue> queue_;
    std::atomic<State> state_;
};
} // namespace pstk::execution
