#pragma once

#include <pstk/TkResult.h>
#include <pstk/execution/TkSerialWorkLane.hpp>
#include <pstk/execution/TkWorkerPool.h>

#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>

namespace pstk::execution
{

template <typename T> using TkWorkLaneInvoke = void (*)(void *, T &&) noexcept;

template <typename T> class TkWorkerScheduler;

template <typename T> class TkWorkLaneHandle final
{
    friend class TkWorkerScheduler<T>;

  public:
    TkWorkLaneHandle() noexcept = default;

  private:
    const TkWorkerScheduler<T> *scheduler_ = nullptr;
    std::size_t slot_ = std::numeric_limits<std::size_t>::max();
};

template <typename T> struct TkWorkLaneCreateInfo
{
    void *context = nullptr;
    TkWorkLaneInvoke<T> invoke = nullptr;
};

template <typename T> struct TkWorkerSchedulerCreateInfo
{
    std::size_t workerCount = 0;
    std::size_t readyCapacity = 0;
    std::size_t workLaneCapacity = 0;
    std::size_t maxMessagesPerDrain = 0;
    const TkWorkLaneCreateInfo<T> *workLanes = nullptr;
    std::size_t workLaneCount = 0;
};

template <typename T> class TkWorkerScheduler final
{
    using WorkLane = TkSerialWorkLane<T>;
    using WorkLaneHandle = TkWorkLaneHandle<T>;
    using WorkLaneInvoke = TkWorkLaneInvoke<T>;

    enum class State
    {
        Running,
        StoppingDrain,
        StoppingDiscard,
        Stopped
    };

    struct LaneEntry
    {
        std::unique_ptr<WorkLane> workLane;
        void *context;
        WorkLaneInvoke invoke;
        TkWorkerScheduler *scheduler;
    };

  public:
    static TkResult Create(const TkWorkerSchedulerCreateInfo<T> &createInfo,
                           std::unique_ptr<TkWorkerScheduler<T>> *const outScheduler) noexcept
    {
        if (outScheduler == nullptr || !IsValidCreateInfo(createInfo))
        {
            return TK_ERROR_INVALID_ARGUMENT;
        }

        std::vector<LaneEntry> entries;
        try
        {
            entries.reserve(createInfo.workLaneCount);
            for (std::size_t index = 0; index < createInfo.workLaneCount; ++index)
            {
                std::unique_ptr<WorkLane> workLane;
                const TkResult laneResult = WorkLane::Create(createInfo.workLaneCapacity, &workLane);
                if (laneResult != TK_SUCCESS)
                {
                    return laneResult;
                }

                const TkWorkLaneCreateInfo<T> &laneInfo = createInfo.workLanes[index];
                entries.push_back(LaneEntry{std::move(workLane), laneInfo.context, laneInfo.invoke, nullptr});
            }
        }
        catch (const std::bad_alloc &)
        {
            return TK_ERROR_OUT_OF_MEMORY;
        }
        catch (const std::length_error &)
        {
            return TK_ERROR_INVALID_ARGUMENT;
        }

        std::unique_ptr<TkWorkerPool> workerPool;
        const TkResult poolResult = TkWorkerPool::Create(createInfo.workerCount, createInfo.readyCapacity, &workerPool);
        if (poolResult != TK_SUCCESS)
        {
            return poolResult;
        }

        try
        {
            std::unique_ptr<TkWorkerScheduler<T>> scheduler(
                new TkWorkerScheduler<T>(std::move(workerPool), std::move(entries), createInfo.maxMessagesPerDrain));
            *outScheduler = std::move(scheduler);
        }
        catch (const std::bad_alloc &)
        {
            return TK_ERROR_OUT_OF_MEMORY;
        }

        return TK_SUCCESS;
    }

    ~TkWorkerScheduler() noexcept
    {
        const TkResult result = Stop(TkWorkerPoolStopMode::Discard);
        assert(result == TK_SUCCESS);
    }

    TkWorkerScheduler(const TkWorkerScheduler &) = delete;
    TkWorkerScheduler &operator=(const TkWorkerScheduler &) = delete;

    TkWorkerScheduler(TkWorkerScheduler &&) = delete;
    TkWorkerScheduler &operator=(TkWorkerScheduler &&) = delete;

    TkResult GetWorkLaneHandle(const std::size_t index, TkWorkLaneHandle<T> *const outHandle) noexcept
    {
        if (outHandle == nullptr)
        {
            return TK_ERROR_INVALID_ARGUMENT;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != State::Running)
        {
            return TK_ERROR_INVALID_STATE;
        }

        if (index >= entries_.size())
        {
            return TK_ERROR_INVALID_ARGUMENT;
        }

        WorkLaneHandle handle;
        handle.scheduler_ = this;
        handle.slot_ = index;
        *outHandle = handle;
        return TK_SUCCESS;
    }

    TkResult TryPost(const TkWorkLaneHandle<T> &handle, T &&item) noexcept
    {
        LaneEntry *entry = nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (state_ != State::Running)
            {
                return TK_ERROR_INVALID_STATE;
            }

            if (handle.scheduler_ != this || handle.slot_ >= entries_.size())
            {
                return TK_ERROR_INVALID_ARGUMENT;
            }

            ++inFlightPosts_;
            entry = &entries_[handle.slot_];
        }

        bool shouldSchedule = false;
        TkResult result = entry->workLane->TryPublish(std::move(item), &shouldSchedule);
        if (result == TK_SUCCESS && shouldSchedule)
        {
            const TkResult scheduleResult = TryScheduleLane(entry);
            if (scheduleResult != TK_SUCCESS)
            {
                std::lock_guard<std::mutex> lock(mutex_);
                assert(state_ == State::StoppingDiscard || state_ == State::Stopped);
            }
        }

        CompletePost();
        return result;
    }

    TkResult Stop(const TkWorkerPoolStopMode mode) noexcept
    {
        // DrainLane 내부의 Invoke Callback에서 Stop이 호출되는 경우 호출한 Worker가 자기 자신의 종료까지 대기하게 됨
        // 순환 대기를 방지하기 위해 DrainLane을 실행 중인 Worker에서 Stop이 호출되면 실패 처리
        if (IsCurrentDrainCallback())
        {
            return TK_ERROR_INVALID_STATE;
        }

        std::unique_lock<std::mutex> lock(mutex_);
        if (state_ == State::Stopped)
        {
            return TK_SUCCESS;
        }

        if (state_ != State::Running)
        {
            return TK_ERROR_INVALID_STATE;
        }

        state_ = mode == TkWorkerPoolStopMode::Drain ? State::StoppingDrain : State::StoppingDiscard;
        condition_.wait(lock, [this]() { return inFlightPosts_ == 0; });

        if (mode == TkWorkerPoolStopMode::Drain)
        {
            condition_.wait(lock, [this]() { return IsDrainComplete(); });
        }

        lock.unlock();
        const TkResult result = workerPool_->Stop(mode);
        if (result == TK_SUCCESS)
        {
            for (LaneEntry &entry : entries_)
            {
                entry.workLane.reset();
            }
        }

        lock.lock();
        state_ = State::Stopped;
        condition_.notify_all();
        return result;
    }

  private:
    TkWorkerScheduler(std::unique_ptr<TkWorkerPool> workerPool, std::vector<LaneEntry> &&entries,
                      const std::size_t maxMessagesPerDrain) noexcept
        : workerPool_(std::move(workerPool)), entries_(std::move(entries)), maxMessagesPerDrain_(maxMessagesPerDrain),
          state_(State::Running), inFlightPosts_(0), activeDrains_(0)
    {
        for (LaneEntry &entry : entries_)
        {
            entry.scheduler = this;
        }
    }

    static bool IsValidCreateInfo(const TkWorkerSchedulerCreateInfo<T> &createInfo) noexcept
    {
        if (createInfo.workerCount == 0 || !IsValidCapacity(createInfo.readyCapacity) ||
            !IsValidCapacity(createInfo.workLaneCapacity) || createInfo.maxMessagesPerDrain == 0 ||
            createInfo.readyCapacity < createInfo.workLaneCount)
        {
            return false;
        }

        if (createInfo.workLaneCount != 0 && createInfo.workLanes == nullptr)
        {
            return false;
        }

        for (std::size_t index = 0; index < createInfo.workLaneCount; ++index)
        {
            if (createInfo.workLanes[index].invoke == nullptr)
            {
                return false;
            }
        }

        return true;
    }

    static bool IsValidCapacity(const std::size_t capacity) noexcept
    {
        return capacity >= 2 && (capacity & (capacity - 1)) == 0;
    }

    static TkWorkerScheduler<T> *&CurrentDrainScheduler() noexcept
    {
        static thread_local TkWorkerScheduler<T> *currentScheduler = nullptr;

        return currentScheduler;
    }

    bool IsCurrentDrainCallback() const noexcept
    {
        return CurrentDrainScheduler() == this;
    }

    bool CanDrain() const noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_ == State::Running || state_ == State::StoppingDrain;
    }

    TkResult TryScheduleLane(LaneEntry *const entry) noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != State::Running && state_ != State::StoppingDrain)
        {
            return TK_ERROR_INVALID_STATE;
        }

        TkWorkItem workItem(entry, &InvokeLane, NoOpDestroy);
        return workerPool_->TrySchedule(std::move(workItem));
    }

    // Callback function for Invoke DrainLane(entry)
    static void InvokeLane(void *const rawContext) noexcept
    {
        auto *const entry = static_cast<LaneEntry *>(rawContext);
        entry->scheduler->DrainLane(entry);
    }

    void DrainLane(LaneEntry *const entry) noexcept
    {
        if (!CanDrain())
        {
            return;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            ++activeDrains_;
        }

        TkWorkerScheduler<T> *const previousScheduler = CurrentDrainScheduler();
        CurrentDrainScheduler() = this;

        const TkResult beginResult = entry->workLane->BeginDrain();
        if (beginResult == TK_SUCCESS)
        {
            using QueueValue = typename WorkLane::QueueValue;
            QueueValue value;
            std::size_t messageCount = 0;
            while (messageCount < maxMessagesPerDrain_ && entry->workLane->TryPopValue(&value))
            {
                entry->invoke(entry->context, std::move(value.Value()));
                ++messageCount;
            }

            bool shouldSchedule = false;
            const TkResult finishResult = entry->workLane->FinishDrain(&shouldSchedule);
            if (finishResult == TK_SUCCESS && shouldSchedule)
            {
                const TkResult scheduleResult = TryScheduleLane(entry);
                if (scheduleResult != TK_SUCCESS)
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    assert(scheduleResult == TK_ERROR_INVALID_STATE);
                    assert(state_ == State::StoppingDiscard || state_ == State::Stopped);
                }
            }
        }

        CurrentDrainScheduler() = previousScheduler;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            --activeDrains_;
            condition_.notify_all();
        }
    }

    void CompletePost() noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        assert(inFlightPosts_ != 0);
        --inFlightPosts_;
        condition_.notify_all();
    }

    bool IsDrainComplete() const noexcept
    {
        if (activeDrains_ != 0)
        {
            return false;
        }

        for (const LaneEntry &entry : entries_)
        {
            if (!entry.workLane->IsQuiescent())
            {
                return false;
            }
        }

        return true;
    }

    std::unique_ptr<TkWorkerPool> workerPool_;
    std::vector<LaneEntry> entries_;
    const std::size_t maxMessagesPerDrain_;

    mutable std::mutex mutex_;
    std::condition_variable condition_;

    State state_;
    std::size_t inFlightPosts_;
    std::size_t activeDrains_;
};
} // namespace pstk::execution
