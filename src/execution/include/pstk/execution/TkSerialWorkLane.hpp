#pragma once

#include <pstk/TkResult.h>
#include <pstk/execution/TkBoundedMpmcQueue.hpp>

#include <cassert>
#include <cstddef>
#include <memory>
#include <mutex>
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

        // 게시 완료와 예약 책임 결정을 drain 종료 판단과 같은 임계영역으로 묶는다.
        std::lock_guard<std::mutex> lock(mutex_);
        QueueValue value(item);
        if (!queue_->TryPush(std::move(value)))
        {
            return TK_ERROR_CAPACITY_EXCEEDED;
        }

        const bool shouldSchedule = state_ == State::Idle;
        if (shouldSchedule)
        {
            state_ = State::Scheduled;
        }

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

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (state_ != State::Scheduled)
            {
                return TK_ERROR_INVALID_STATE;
            }

            state_ = State::Draining;
        }

        // MPMC queue의 소비와 사용자 코드 실행은 lane mutex 밖에서 진행한다.
        // lane mutex 내부에서 state_ 변경을 완료했기 때문에 실행의 직렬화를 보장하기 때문이다.
        {
            QueueValue value;
            std::size_t messageCount = 0;
            while (messageCount < maxMessages && queue_->TryPop(&value))
            {
                invoke(context, std::move(value.Value()));
                ++messageCount;
            }
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            assert(state_ == State::Draining);
            const bool shouldSchedule = queue_->HasReadyItem();
            // Scheduled는 반환 후 호출자가 실제 ready 예약을 게시할 책임까지 포함한다.
            state_ = shouldSchedule ? State::Scheduled : State::Idle;

            *outShouldSchedule = shouldSchedule;
        }

        return TK_SUCCESS;
    }

    bool IsQuiescent() const noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_ == State::Idle && !queue_->HasReadyItem();
    }

  private:
    explicit TkSerialWorkLane(std::unique_ptr<Queue> queue) noexcept : queue_(std::move(queue)), state_(State::Idle)
    {
    }

    std::unique_ptr<Queue> queue_;
    mutable std::mutex mutex_;
    State state_;
};
} // namespace pstk::execution
