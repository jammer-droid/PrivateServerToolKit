#include <pstk/execution/TkWorkerPool.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <thread>
#include <utility>

namespace
{

using pstk::execution::TkWorkerPool;
using pstk::execution::TkWorkerPoolStopMode;
using pstk::execution::TkWorkItem;

struct CallbackState
{
    std::atomic<int> invokeCount{0};
    std::atomic<int> destroyCount{0};
    std::atomic<int> nestedResult{TK_ERROR_UNKNOWN};
    std::atomic<int> workerStopResult{TK_ERROR_UNKNOWN};
};

struct CallbackContext
{
    CallbackState *state;
    int id;
};

void CountInvoke(void *const rawContext) noexcept
{
    auto *const context = static_cast<CallbackContext *>(rawContext);
    context->state->invokeCount.fetch_add(1, std::memory_order_relaxed);
}

void CountDestroy(void *const rawContext) noexcept
{
    auto *const context = static_cast<CallbackContext *>(rawContext);
    context->state->destroyCount.fetch_add(1, std::memory_order_relaxed);
    delete context;
}

TkWorkItem MakeItem(CallbackState *const state, const int id)
{
    return TkWorkItem(new CallbackContext{state, id}, &CountInvoke, &CountDestroy);
}

template <typename Predicate> bool WaitFor(Predicate &&predicate)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!predicate())
    {
        if (std::chrono::steady_clock::now() >= deadline)
        {
            return false;
        }

        std::this_thread::yield();
    }

    return true;
}

struct BlockingGate
{
    std::atomic<bool> started{false};
    std::atomic<bool> release{false};
};

struct BlockingContext
{
    CallbackState *state;
    BlockingGate *gate;
};

void BlockInvoke(void *const rawContext) noexcept
{
    auto *const context = static_cast<BlockingContext *>(rawContext);
    context->state->invokeCount.fetch_add(1, std::memory_order_relaxed);
    context->gate->started.store(true, std::memory_order_release);

    while (!context->gate->release.load(std::memory_order_acquire))
    {
        std::this_thread::yield();
    }
}

void BlockDestroy(void *const rawContext) noexcept
{
    auto *const context = static_cast<BlockingContext *>(rawContext);
    context->state->destroyCount.fetch_add(1, std::memory_order_relaxed);
    delete context;
}

TkWorkItem MakeBlockingItem(CallbackState *const state, BlockingGate *const gate)
{
    return TkWorkItem(new BlockingContext{state, gate}, &BlockInvoke, &BlockDestroy);
}

struct ExactlyOnceState
{
    explicit ExactlyOnceState(const int valueCount)
        : invokeCounts(new std::atomic<int>[valueCount]), valueCount(valueCount)
    {
        for (int index = 0; index < valueCount; ++index)
        {
            invokeCounts[index].store(0, std::memory_order_relaxed);
        }
    }

    std::unique_ptr<std::atomic<int>[]> invokeCounts;
    const int valueCount;
    std::atomic<int> destroyCount{0};
    std::atomic<int> invalidIdCount{0};
    std::atomic<int> duplicateCount{0};
};

struct ExactlyOnceContext
{
    ExactlyOnceState *state;
    int id;
};

void ExactlyOnceInvoke(void *const rawContext) noexcept
{
    auto *const context = static_cast<ExactlyOnceContext *>(rawContext);
    if (context->id < 0 || context->id >= context->state->valueCount)
    {
        context->state->invalidIdCount.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    const int previous = context->state->invokeCounts[context->id].fetch_add(1, std::memory_order_relaxed);
    if (previous != 0)
    {
        context->state->duplicateCount.fetch_add(1, std::memory_order_relaxed);
    }
}

void ExactlyOnceDestroy(void *const rawContext) noexcept
{
    auto *const context = static_cast<ExactlyOnceContext *>(rawContext);
    context->state->destroyCount.fetch_add(1, std::memory_order_relaxed);
    delete context;
}

TkWorkItem MakeExactlyOnceItem(ExactlyOnceState *const state, const int id)
{
    return TkWorkItem(new ExactlyOnceContext{state, id}, &ExactlyOnceInvoke, &ExactlyOnceDestroy);
}

} // namespace

TEST(TkWorkerPoolContract, RejectsInvalidFactoryArgumentsAndPreservesOutput)
{
    std::unique_ptr<TkWorkerPool> pool;

    EXPECT_EQ(TkWorkerPool::Create(0, 2, &pool), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(TkWorkerPool::Create(1, 0, &pool), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(TkWorkerPool::Create(1, 1, &pool), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(TkWorkerPool::Create(1, 3, &pool), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(TkWorkerPool::Create(1, 2, nullptr), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(pool.get(), nullptr);

    ASSERT_EQ(TkWorkerPool::Create(1, 2, &pool), TK_SUCCESS);
    TkWorkerPool *const original = pool.get();

    EXPECT_EQ(TkWorkerPool::Create(0, 2, &pool), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(pool.get(), original);
    EXPECT_EQ(TkWorkerPool::Create(1, 3, &pool), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(pool.get(), original);

    ASSERT_EQ(pool->Stop(TkWorkerPoolStopMode::Drain), TK_SUCCESS);
}

TEST(TkWorkerPoolContract, WakesSleepingWorkerAndDestroysAcceptedItemsExactlyOnce)
{
    CallbackState state;
    std::unique_ptr<TkWorkerPool> pool;
    ASSERT_EQ(TkWorkerPool::Create(1, 2, &pool), TK_SUCCESS);

    TkWorkItem first = MakeItem(&state, 1);
    ASSERT_EQ(pool->TrySchedule(std::move(first)), TK_SUCCESS);
    ASSERT_TRUE(WaitFor([&]() { return state.invokeCount.load(std::memory_order_acquire) == 1; }));

    TkWorkItem second = MakeItem(&state, 2);
    ASSERT_EQ(pool->TrySchedule(std::move(second)), TK_SUCCESS);
    ASSERT_TRUE(WaitFor([&]() { return state.invokeCount.load(std::memory_order_acquire) == 2; }));

    ASSERT_EQ(pool->Stop(TkWorkerPoolStopMode::Drain), TK_SUCCESS);
    EXPECT_EQ(state.invokeCount.load(std::memory_order_relaxed), 2);
    EXPECT_EQ(state.destroyCount.load(std::memory_order_relaxed), 2);
}

TEST(TkWorkerPoolContract, PreservesInputWhenReadyQueueIsFull)
{
    CallbackState state;
    BlockingGate gate;
    std::unique_ptr<TkWorkerPool> pool;
    ASSERT_EQ(TkWorkerPool::Create(1, 2, &pool), TK_SUCCESS);

    TkWorkItem blocker = MakeBlockingItem(&state, &gate);
    ASSERT_EQ(pool->TrySchedule(std::move(blocker)), TK_SUCCESS);
    EXPECT_TRUE(WaitFor([&]() { return gate.started.load(std::memory_order_acquire); }));

    TkWorkItem queuedFirst = MakeItem(&state, 1);
    TkWorkItem queuedSecond = MakeItem(&state, 2);
    ASSERT_EQ(pool->TrySchedule(std::move(queuedFirst)), TK_SUCCESS);
    ASSERT_EQ(pool->TrySchedule(std::move(queuedSecond)), TK_SUCCESS);

    TkWorkItem rejected = MakeItem(&state, 3);
    EXPECT_EQ(pool->TrySchedule(std::move(rejected)), TK_ERROR_CAPACITY_EXCEEDED);
    EXPECT_EQ(state.destroyCount.load(std::memory_order_relaxed), 0);

    gate.release.store(true, std::memory_order_release);
    ASSERT_EQ(pool->Stop(TkWorkerPoolStopMode::Drain), TK_SUCCESS);

    EXPECT_EQ(state.invokeCount.load(std::memory_order_relaxed), 3);
    EXPECT_EQ(state.destroyCount.load(std::memory_order_relaxed), 3);
}

TEST(TkWorkerPoolContract, ExecutesMultipleProducersAndWorkersExactlyOnce)
{
    constexpr int producerCount = 4;
    constexpr int valuesPerProducer = 500;
    constexpr int totalValues = producerCount * valuesPerProducer;

    ExactlyOnceState state(totalValues);
    std::unique_ptr<TkWorkerPool> pool;
    ASSERT_EQ(TkWorkerPool::Create(4, 64, &pool), TK_SUCCESS);

    std::atomic<int> readyProducers{0};
    std::atomic<bool> start{false};
    std::atomic<bool> producerFailure{false};
    std::thread producers[producerCount];

    for (int producerIndex = 0; producerIndex < producerCount; ++producerIndex)
    {
        producers[producerIndex] = std::thread([&, producerIndex]() {
            readyProducers.fetch_add(1, std::memory_order_release);
            while (!start.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }

            for (int valueIndex = 0; valueIndex < valuesPerProducer; ++valueIndex)
            {
                const int id = producerIndex * valuesPerProducer + valueIndex;
                TkWorkItem item = MakeExactlyOnceItem(&state, id);
                bool accepted = false;
                for (int attempt = 0; attempt < 1000000; ++attempt)
                {
                    const TkResult result = pool->TrySchedule(std::move(item));
                    if (result == TK_SUCCESS)
                    {
                        accepted = true;
                        break;
                    }

                    if (result != TK_ERROR_CAPACITY_EXCEEDED)
                    {
                        producerFailure.store(true, std::memory_order_release);
                        break;
                    }

                    std::this_thread::yield();
                }

                if (!accepted)
                {
                    producerFailure.store(true, std::memory_order_release);
                    return;
                }
            }
        });
    }

    ASSERT_TRUE(WaitFor([&]() { return readyProducers.load(std::memory_order_acquire) == producerCount; }));
    start.store(true, std::memory_order_release);

    for (std::thread &producer : producers)
    {
        producer.join();
    }

    ASSERT_EQ(pool->Stop(TkWorkerPoolStopMode::Drain), TK_SUCCESS);
    EXPECT_FALSE(producerFailure.load(std::memory_order_acquire));
    EXPECT_EQ(state.destroyCount.load(std::memory_order_relaxed), totalValues);
    EXPECT_EQ(state.invalidIdCount.load(std::memory_order_relaxed), 0);
    EXPECT_EQ(state.duplicateCount.load(std::memory_order_relaxed), 0);

    for (int id = 0; id < totalValues; ++id)
    {
        EXPECT_EQ(state.invokeCounts[id].load(std::memory_order_relaxed), 1);
    }
}

struct NestedContext
{
    NestedContext(TkWorkerPool *const pool, CallbackState *const state, TkWorkItem &&nestedItem) noexcept
        : pool(pool), state(state), nestedItem(std::move(nestedItem))
    {
    }

    TkWorkerPool *pool;
    CallbackState *state;
    TkWorkItem nestedItem;
};

void NestedInvoke(void *const rawContext) noexcept
{
    auto *const context = static_cast<NestedContext *>(rawContext);
    context->state->invokeCount.fetch_add(1, std::memory_order_relaxed);
    TkWorkItem nestedItem(std::move(context->nestedItem));
    const TkResult result = context->pool->TrySchedule(std::move(nestedItem));
    context->state->nestedResult.store(result, std::memory_order_release);
}

void NestedDestroy(void *const rawContext) noexcept
{
    auto *const context = static_cast<NestedContext *>(rawContext);
    context->state->destroyCount.fetch_add(1, std::memory_order_relaxed);
    delete context;
}

TEST(TkWorkerPoolContract, InvokesOutsideQueueMutexAndAllowsNestedScheduling)
{
    CallbackState state;
    std::unique_ptr<TkWorkerPool> pool;
    ASSERT_EQ(TkWorkerPool::Create(1, 4, &pool), TK_SUCCESS);

    TkWorkItem nestedItem = MakeItem(&state, 2);
    auto *const rootContext = new NestedContext(pool.get(), &state, std::move(nestedItem));
    TkWorkItem root(rootContext, &NestedInvoke, &NestedDestroy);
    ASSERT_EQ(pool->TrySchedule(std::move(root)), TK_SUCCESS);

    ASSERT_TRUE(WaitFor([&]() { return state.nestedResult.load(std::memory_order_acquire) == TK_SUCCESS; }));
    ASSERT_EQ(pool->Stop(TkWorkerPoolStopMode::Drain), TK_SUCCESS);

    EXPECT_EQ(state.invokeCount.load(std::memory_order_relaxed), 2);
    EXPECT_EQ(state.destroyCount.load(std::memory_order_relaxed), 2);
}

TEST(TkWorkerPoolContract, DrainExecutesAllQueuedWork)
{
    constexpr int itemCount = 32;
    CallbackState state;
    std::unique_ptr<TkWorkerPool> pool;
    ASSERT_EQ(TkWorkerPool::Create(4, 64, &pool), TK_SUCCESS);

    for (int id = 0; id < itemCount; ++id)
    {
        TkWorkItem item = MakeItem(&state, id);
        ASSERT_EQ(pool->TrySchedule(std::move(item)), TK_SUCCESS);
    }

    ASSERT_EQ(pool->Stop(TkWorkerPoolStopMode::Drain), TK_SUCCESS);
    EXPECT_EQ(state.invokeCount.load(std::memory_order_relaxed), itemCount);
    EXPECT_EQ(state.destroyCount.load(std::memory_order_relaxed), itemCount);
}

TEST(TkWorkerPoolContract, DiscardFinishesInFlightAndDestroysQueuedWork)
{
    CallbackState state;
    BlockingGate gate;
    std::unique_ptr<TkWorkerPool> pool;
    ASSERT_EQ(TkWorkerPool::Create(1, 2, &pool), TK_SUCCESS);

    TkWorkItem blocker = MakeBlockingItem(&state, &gate);
    ASSERT_EQ(pool->TrySchedule(std::move(blocker)), TK_SUCCESS);
    EXPECT_TRUE(WaitFor([&]() { return gate.started.load(std::memory_order_acquire); }));

    TkWorkItem queuedFirst = MakeItem(&state, 1);
    TkWorkItem queuedSecond = MakeItem(&state, 2);
    ASSERT_EQ(pool->TrySchedule(std::move(queuedFirst)), TK_SUCCESS);
    ASSERT_EQ(pool->TrySchedule(std::move(queuedSecond)), TK_SUCCESS);

    std::atomic<int> stopResult{TK_ERROR_UNKNOWN};
    std::thread stopper(
        [&]() { stopResult.store(pool->Stop(TkWorkerPoolStopMode::Discard), std::memory_order_release); });

    {
        TkWorkItem probe = MakeItem(&state, 3);
        bool admissionClosed = false;
        for (int attempt = 0; attempt < 1000000 && !admissionClosed; ++attempt)
        {
            const TkResult result = pool->TrySchedule(std::move(probe));
            if (result == TK_ERROR_INVALID_STATE)
            {
                admissionClosed = true;
                break;
            }

            if (result != TK_ERROR_CAPACITY_EXCEEDED)
            {
                break;
            }

            std::this_thread::yield();
        }

        EXPECT_TRUE(admissionClosed);
    }
    gate.release.store(true, std::memory_order_release);
    stopper.join();

    EXPECT_EQ(stopResult.load(std::memory_order_acquire), TK_SUCCESS);
    EXPECT_EQ(state.invokeCount.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(state.destroyCount.load(std::memory_order_relaxed), 4);
}

struct WorkerStopContext
{
    TkWorkerPool *pool;
    CallbackState *state;
};

void WorkerStopInvoke(void *const rawContext) noexcept
{
    auto *const context = static_cast<WorkerStopContext *>(rawContext);
    context->state->invokeCount.fetch_add(1, std::memory_order_relaxed);
    context->state->workerStopResult.store(context->pool->Stop(TkWorkerPoolStopMode::Discard),
                                           std::memory_order_release);
}

void WorkerStopDestroy(void *const rawContext) noexcept
{
    auto *const context = static_cast<WorkerStopContext *>(rawContext);
    context->state->destroyCount.fetch_add(1, std::memory_order_relaxed);
    delete context;
}

TEST(TkWorkerPoolContract, RejectsStopFromOwnWorker)
{
    CallbackState state;
    std::unique_ptr<TkWorkerPool> pool;
    ASSERT_EQ(TkWorkerPool::Create(1, 2, &pool), TK_SUCCESS);

    auto *const context = new WorkerStopContext{pool.get(), &state};
    TkWorkItem item(context, &WorkerStopInvoke, &WorkerStopDestroy);
    ASSERT_EQ(pool->TrySchedule(std::move(item)), TK_SUCCESS);
    ASSERT_TRUE(WaitFor([&]() { return state.workerStopResult.load(std::memory_order_acquire) != TK_ERROR_UNKNOWN; }));

    EXPECT_EQ(state.workerStopResult.load(std::memory_order_relaxed), TK_ERROR_INVALID_STATE);
    ASSERT_EQ(pool->Stop(TkWorkerPoolStopMode::Drain), TK_SUCCESS);
    EXPECT_EQ(state.invokeCount.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(state.destroyCount.load(std::memory_order_relaxed), 1);
}

TEST(TkWorkerPoolContract, SequentialStopIsAnIdempotentSuccess)
{
    std::unique_ptr<TkWorkerPool> pool;
    ASSERT_EQ(TkWorkerPool::Create(2, 2, &pool), TK_SUCCESS);

    ASSERT_EQ(pool->Stop(TkWorkerPoolStopMode::Drain), TK_SUCCESS);
    EXPECT_EQ(pool->Stop(TkWorkerPoolStopMode::Drain), TK_SUCCESS);
    EXPECT_EQ(pool->Stop(TkWorkerPoolStopMode::Discard), TK_SUCCESS);
}

TEST(TkWorkerPoolContract, DestructorDiscardsOrJoinsAcceptedWorkSafely)
{
    CallbackState state;
    {
        std::unique_ptr<TkWorkerPool> pool;
        ASSERT_EQ(TkWorkerPool::Create(1, 2, &pool), TK_SUCCESS);

        TkWorkItem item = MakeItem(&state, 1);
        ASSERT_EQ(pool->TrySchedule(std::move(item)), TK_SUCCESS);
    }

    EXPECT_GE(state.destroyCount.load(std::memory_order_relaxed), 1);
    EXPECT_LE(state.invokeCount.load(std::memory_order_relaxed), 1);
}
