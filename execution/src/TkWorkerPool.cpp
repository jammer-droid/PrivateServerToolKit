#include <pstk/execution/TkWorkerPool.h>

#include <pstk/execution/TkBoundedMpmcQueue.hpp>

#include <cassert>
#include <condition_variable>
#include <mutex>
#include <new>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace pstk::execution
{

class TkWorkerPool::Impl final
{
    using ReadyQueue = TkBoundedMpmcQueue<TkWorkItem>;

    enum class State
    {
        Running,
        StoppingDrain,   // Stop after finish Drain
        StoppingDiscard, // Stop after finish Discard
        Stopped
    };

  public:
    Impl(std::unique_ptr<ReadyQueue> readyQueue, const std::size_t workerCount)
        : readyQueue_(std::move(readyQueue)), workerCount_(workerCount), state_(State::Stopped), queuedCount_(0)
    {
        workers_.reserve(workerCount_);
    }

    ~Impl() noexcept
    {
        assert(state_ == State::Stopped);
    }

    TkResult Start() noexcept
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            state_ = State::Running;
        }

        try
        {
            for (std::size_t index = 0; index < workerCount_; ++index)
            {
                workers_.emplace_back(&Impl::WorkerMain, this);
            }
        }
        catch (const std::bad_alloc &)
        {
            AbortStart();
            return TK_ERROR_OUT_OF_MEMORY;
        }
        catch (const std::system_error &)
        {
            AbortStart();
            return TK_ERROR_UNKNOWN;
        }

        return TK_SUCCESS;
    }

    TkResult TrySchedule(TkWorkItem &&item) noexcept
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (state_ != State::Running)
        {
            return TK_ERROR_INVALID_STATE;
        }

        if (!readyQueue_->TryPush(std::move(item)))
        {
            return TK_ERROR_CAPACITY_EXCEEDED;
        }

        ++queuedCount_;
        lock.unlock();
        condition_.notify_one();
        return TK_SUCCESS;
    }

    TkResult Stop(const TkWorkerPoolStopMode mode) noexcept
    {
        if (IsCurrentWorker())
        {
            return TK_ERROR_INVALID_STATE;
        }

        {
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
        }

        condition_.notify_all();
        JoinWorkers();

        // mode == StoppingDrain인 경우에는 WorkerMain에서 readyQueue를 소비하고 thread join을 하고
        // mode == StoppingDiscard인 경우에는 WorkerMain에서 readyQueue를 소비하지 않고 즉시 thread join을 한다.
        // 따라서 readyQueue에 남아 있는 item을 버리는 작업을 별도로 진행한다.
        if (mode == TkWorkerPoolStopMode::Discard)
        {
            DiscardQueued();
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            state_ = State::Stopped;
        }

        return TK_SUCCESS;
    }

  private:
    void WorkerMain() noexcept
    {
        while (true)
        {
            TkWorkItem item(nullptr);
            {
                std::unique_lock<std::mutex> lock(mutex_);
                condition_.wait(lock, [this]() { return queuedCount_ != 0 || state_ != State::Running; });

                if (state_ == State::StoppingDiscard || state_ == State::Stopped)
                {
                    return;
                }

                if (queuedCount_ == 0)
                {
                    return;
                }

                if (!readyQueue_->TryPop(&item))
                {
                    continue;
                }

                --queuedCount_;
            }

            item.Invoke();
        }
    }

    void AbortStart() noexcept
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            state_ = State::StoppingDiscard;
        }

        condition_.notify_all();
        JoinWorkers();

        {
            std::lock_guard<std::mutex> lock(mutex_);
            state_ = State::Stopped;
        }
    }

    bool IsCurrentWorker() const noexcept
    {
        const std::thread::id currentId = std::this_thread::get_id();
        for (const std::thread &worker : workers_)
        {
            if (worker.get_id() == currentId)
            {
                return true;
            }
        }

        return false;
    }

    void JoinWorkers() noexcept
    {
        for (std::thread &worker : workers_)
        {
            if (!worker.joinable())
            {
                continue;
            }

            worker.join();
        }
    }

    void DiscardQueued() noexcept
    {
        while (true)
        {
            TkWorkItem item(nullptr);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (queuedCount_ == 0)
                {
                    return;
                }

                if (!readyQueue_->TryPop(&item))
                {
                    return;
                }

                --queuedCount_;
            }
        }
    }

    std::unique_ptr<ReadyQueue> readyQueue_;

    std::vector<std::thread> workers_;
    const std::size_t workerCount_;

    std::mutex mutex_;
    std::condition_variable condition_;

    State state_;
    std::size_t queuedCount_;
};

TkWorkerPool::TkWorkerPool(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl))
{
}

TkResult TkWorkerPool::Create(const std::size_t workerCount, const std::size_t readyCapacity,
                              std::unique_ptr<TkWorkerPool> *const outPool) noexcept
{
    if (outPool == nullptr || workerCount == 0)
    {
        return TK_ERROR_INVALID_ARGUMENT;
    }

    std::unique_ptr<Impl> impl;
    std::unique_ptr<TkBoundedMpmcQueue<TkWorkItem>> readyQueue;
    const TkResult queueResult = TkBoundedMpmcQueue<TkWorkItem>::Create(readyCapacity, &readyQueue);
    if (queueResult != TK_SUCCESS)
    {
        return queueResult;
    }

    try
    {
        impl = std::make_unique<Impl>(std::move(readyQueue), workerCount);
        const TkResult startResult = impl->Start();
        if (startResult != TK_SUCCESS)
        {
            return startResult;
        }

        std::unique_ptr<TkWorkerPool> pool(new TkWorkerPool(std::move(impl)));
        *outPool = std::move(pool);
        return TK_SUCCESS;
    }
    catch (const std::bad_alloc &)
    {
        if (impl != nullptr)
        {
            const TkResult stopResult = impl->Stop(TkWorkerPoolStopMode::Discard);
            assert(stopResult == TK_SUCCESS);
        }

        return TK_ERROR_OUT_OF_MEMORY;
    }
    catch (const std::length_error &)
    {
        if (impl != nullptr)
        {
            const TkResult stopResult = impl->Stop(TkWorkerPoolStopMode::Discard);
            assert(stopResult == TK_SUCCESS);
        }

        return TK_ERROR_INVALID_ARGUMENT;
    }
}

TkWorkerPool::~TkWorkerPool() noexcept
{
    const TkResult result = impl_->Stop(TkWorkerPoolStopMode::Discard);
    assert(result == TK_SUCCESS);
}

TkResult TkWorkerPool::TrySchedule(TkWorkItem &&item) noexcept
{
    return impl_->TrySchedule(std::move(item));
}

TkResult TkWorkerPool::Stop(const TkWorkerPoolStopMode mode) noexcept
{
    return impl_->Stop(mode);
}

} // namespace pstk::execution
