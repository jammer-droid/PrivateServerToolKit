#pragma once

#include <pstk/TkResult.h>
#include <pstk/execution/TkBoundedMpmcQueue.hpp>

#include <atomic>
#include <cassert>
#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace pstk::execution
{

template <typename T> using TkWorkLaneInvoke = void (*)(void *, T &&) noexcept;

template <typename T> class TkSerialWorkLane final
{
    static_assert(std::is_nothrow_move_constructible<T>::value, "TkSerialWorkLane requires nothrow move construction");
    static_assert(std::is_nothrow_move_assignable<T>::value, "TkSerialWorkLane requires nothrow move assignment");
    static_assert(std::is_nothrow_destructible<T>::value, "TkSerialWorkLane requires nothrow destruction");

    // 입력은 queue가 수락한 뒤에만 move하고, T 자체의 default construction은 요구하지 않는다.
    class QueueValue final
    {
      public:
        QueueValue() noexcept : source_(nullptr), hasValue_(false)
        {
        }

        explicit QueueValue(T &source) noexcept : source_(&source), hasValue_(false)
        {
        }

        ~QueueValue() noexcept
        {
            Reset();
        }

        QueueValue(const QueueValue &) = delete;
        QueueValue &operator=(const QueueValue &) = delete;

        QueueValue(QueueValue &&other) noexcept : source_(nullptr), hasValue_(false)
        {
            MoveFrom(other);
        }

        QueueValue &operator=(QueueValue &&other) noexcept
        {
            if (this != &other)
            {
                Reset();
                MoveFrom(other);
            }

            return *this;
        }

        T &Value() noexcept
        {
            assert(hasValue_);
            return *std::launder(reinterpret_cast<T *>(&storage_));
        }

      private:
        void MoveFrom(QueueValue &other) noexcept
        {
            if (other.source_ != nullptr)
            {
                ::new (static_cast<void *>(&storage_)) T(std::move(*other.source_));
                source_ = nullptr;
                hasValue_ = true;
                other.source_ = nullptr;
                return;
            }

            if (other.hasValue_)
            {
                ::new (static_cast<void *>(&storage_)) T(std::move(other.Value()));
                hasValue_ = true;
                other.Reset();
            }
        }

        void Reset() noexcept
        {
            if (hasValue_)
            {
                Value().~T();
                hasValue_ = false;
            }

            source_ = nullptr;
        }

        T *source_;                               // 임시로 가지고 있을 원본 포인터
        bool hasValue_;                           // QueueValue 내부 값이 있는지
        alignas(T) std::byte storage_[sizeof(T)]; // QueueValue 내부의 데이터. 원본에서 이동해 생성
    };

    using Queue = TkBoundedMpmcQueue<QueueValue>;

    enum class State
    {
        Idle,
        Scheduled,
        Draining
    };

  public:
    static TkResult Create(const std::size_t capacity, std::unique_ptr<TkSerialWorkLane<T>> *const outWorkLane) noexcept
    {
        if (outWorkLane == nullptr)
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
            std::unique_ptr<TkSerialWorkLane<T>> workLane(new TkSerialWorkLane<T>(std::move(queue)));
            *outWorkLane = std::move(workLane);
        }
        catch (const std::bad_alloc &)
        {
            return TK_ERROR_OUT_OF_MEMORY;
        }

        return TK_SUCCESS;
    }

    ~TkSerialWorkLane() noexcept = default;

    TkSerialWorkLane(const TkSerialWorkLane &) = delete;
    TkSerialWorkLane &operator=(const TkSerialWorkLane &) = delete;

    TkSerialWorkLane(TkSerialWorkLane &&) = delete;
    TkSerialWorkLane &operator=(TkSerialWorkLane &&) = delete;

    TkResult TryPublish(T &&item, bool *const outShouldSchedule) noexcept
    {
        if (outShouldSchedule == nullptr)
        {
            return TK_ERROR_INVALID_ARGUMENT;
        }

        QueueValue value(item);
        if (!queue_->TryPush(std::move(value)))
        {
            return TK_ERROR_CAPACITY_EXCEEDED;
        }

        State expected = State::Idle;
        const bool shouldSchedule = state_.compare_exchange_strong(
            expected, State::Scheduled, std::memory_order_acq_rel, std::memory_order_relaxed);

        *outShouldSchedule = shouldSchedule;
        return TK_SUCCESS;
    }

    TkResult Drain(const std::size_t maxMessages, void *const context, const TkWorkLaneInvoke<T> invoke,
                   bool *const outShouldSchedule) noexcept
    {
        if (maxMessages == 0 || invoke == nullptr || outShouldSchedule == nullptr)
        {
            return TK_ERROR_INVALID_ARGUMENT;
        }

        State expected = State::Scheduled;
        if (!state_.compare_exchange_strong(expected, State::Draining, std::memory_order_acquire,
                                            std::memory_order_relaxed))
        {
            return TK_ERROR_INVALID_STATE;
        }

        QueueValue value;
        std::size_t messageCount = 0;
        while (messageCount < maxMessages && queue_->TryPop(&value))
        {
            invoke(context, std::move(value.Value()));
            ++messageCount;
        }

        expected = State::Draining;
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
    explicit TkSerialWorkLane(std::unique_ptr<Queue> queue) noexcept : queue_(std::move(queue)), state_(State::Idle)
    {
    }

    std::unique_ptr<Queue> queue_;
    std::atomic<State> state_;
};
} // namespace pstk::execution
