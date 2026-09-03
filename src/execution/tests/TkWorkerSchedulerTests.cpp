#include <pstk/execution/TkWorkerScheduler.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <thread>
#include <utility>

namespace
{

using pstk::execution::TkWorkerPoolStopMode;
using pstk::execution::TkWorkerScheduler;
using pstk::execution::TkWorkerSchedulerCreateInfo;
using pstk::execution::TkWorkLaneCreateInfo;
using pstk::execution::TkWorkLaneHandle;

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

struct MoveOnlyValue
{
    explicit MoveOnlyValue(const int value) noexcept : value(value)
    {
    }

    MoveOnlyValue(const MoveOnlyValue &) = delete;
    MoveOnlyValue &operator=(const MoveOnlyValue &) = delete;

    MoveOnlyValue(MoveOnlyValue &&other) noexcept : value(other.value)
    {
        other.value = -1;
    }

    MoveOnlyValue &operator=(MoveOnlyValue &&other) noexcept
    {
        if (this != &other)
        {
            value = other.value;
            other.value = -1;
        }

        return *this;
    }

    ~MoveOnlyValue() noexcept = default;

    int value;
};

struct CounterContext
{
    std::atomic<int> count{0};
};

void CountInt(void *const rawContext, int &&) noexcept
{
    auto *const context = static_cast<CounterContext *>(rawContext);
    context->count.fetch_add(1, std::memory_order_release);
}

void CountMoveOnly(void *const rawContext, MoveOnlyValue &&value) noexcept
{
    auto *const context = static_cast<CounterContext *>(rawContext);
    context->count.store(value.value, std::memory_order_release);
}

struct BlockingGate
{
    std::atomic<bool> entered{false};
    std::atomic<bool> release{false};
};

struct BlockingContext
{
    BlockingGate *gate;
    std::atomic<int> *count;
};

void BlockInt(void *const rawContext, int &&) noexcept
{
    auto *const context = static_cast<BlockingContext *>(rawContext);
    context->count->fetch_add(1, std::memory_order_release);
    context->gate->entered.store(true, std::memory_order_release);
    while (!context->gate->release.load(std::memory_order_acquire))
    {
        std::this_thread::yield();
    }
}

struct SerialContext
{
    BlockingGate firstGate;
    std::atomic<int> active{0};
    std::atomic<int> maxActive{0};
    std::atomic<int> count{0};
};

void SerialInt(void *const rawContext, int &&) noexcept
{
    auto *const context = static_cast<SerialContext *>(rawContext);
    const int active = context->active.fetch_add(1, std::memory_order_acq_rel) + 1;
    int observedMax = context->maxActive.load(std::memory_order_relaxed);
    while (active > observedMax && !context->maxActive.compare_exchange_weak(
                                       observedMax, active, std::memory_order_relaxed, std::memory_order_relaxed))
    {
    }

    const int count = context->count.fetch_add(1, std::memory_order_release) + 1;
    if (count == 1)
    {
        context->firstGate.entered.store(true, std::memory_order_release);
        while (!context->firstGate.release.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }
    }

    context->active.fetch_sub(1, std::memory_order_release);
}

struct FairContext
{
    BlockingGate firstGate;
    std::atomic<int> firstCount{0};
    std::atomic<int> secondCount{0};
    std::atomic<bool> violation{false};
};

void FairFirst(void *const rawContext, int &&) noexcept
{
    auto *const context = static_cast<FairContext *>(rawContext);
    const int count = context->firstCount.fetch_add(1, std::memory_order_acq_rel) + 1;
    if (count == 1)
    {
        context->firstGate.entered.store(true, std::memory_order_release);
        while (!context->firstGate.release.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }
    }

    if (count >= 3 && context->secondCount.load(std::memory_order_acquire) == 0)
    {
        context->violation.store(true, std::memory_order_release);
    }
}

void FairSecond(void *const rawContext, int &&) noexcept
{
    auto *const context = static_cast<FairContext *>(rawContext);
    context->secondCount.fetch_add(1, std::memory_order_release);
}

struct DestructionProbe
{
    explicit DestructionProbe(std::atomic<int> *const destructionCount) noexcept
        : destructionCount(destructionCount), active(true)
    {
    }

    DestructionProbe(const DestructionProbe &) = delete;
    DestructionProbe &operator=(const DestructionProbe &) = delete;

    DestructionProbe(DestructionProbe &&other) noexcept : destructionCount(other.destructionCount), active(other.active)
    {
        other.active = false;
    }

    DestructionProbe &operator=(DestructionProbe &&other) noexcept
    {
        if (this != &other)
        {
            if (active)
            {
                destructionCount->fetch_add(1, std::memory_order_relaxed);
            }

            destructionCount = other.destructionCount;
            active = other.active;
            other.active = false;
        }

        return *this;
    }

    ~DestructionProbe() noexcept
    {
        if (active)
        {
            destructionCount->fetch_add(1, std::memory_order_relaxed);
        }
    }

    std::atomic<int> *destructionCount;
    bool active;
};

struct BlockingProbeContext
{
    BlockingGate *gate;
    std::atomic<int> *invokeCount;
};

void BlockProbe(void *const rawContext, DestructionProbe &&) noexcept
{
    auto *const context = static_cast<BlockingProbeContext *>(rawContext);
    context->invokeCount->fetch_add(1, std::memory_order_release);
    context->gate->entered.store(true, std::memory_order_release);
    while (!context->gate->release.load(std::memory_order_acquire))
    {
        std::this_thread::yield();
    }
}

struct StopCallbackContext
{
    TkWorkerScheduler<int> *scheduler;
    std::atomic<int> *result;
};

void StopFromCallback(void *const rawContext, int &&) noexcept
{
    auto *const context = static_cast<StopCallbackContext *>(rawContext);
    context->result->store(context->scheduler->Stop(TkWorkerPoolStopMode::Discard), std::memory_order_release);
}

} // namespace

TEST(TkWorkerSchedulerContract, RejectsInvalidFactoryArgumentsAndPreservesOutput)
{
    CounterContext context;
    const TkWorkLaneCreateInfo<int> laneInfo{&context, &CountInt};
    TkWorkerSchedulerCreateInfo<int> createInfo{1, 1, 2, 1, &laneInfo, 1};
    std::unique_ptr<TkWorkerScheduler<int>> scheduler;

    EXPECT_EQ(TkWorkerScheduler<int>::Create(createInfo, nullptr), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(TkWorkerScheduler<int>::Create(createInfo, &scheduler), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(scheduler.get(), nullptr);

    createInfo.readyCapacity = 2;
    createInfo.workerCount = 0;
    EXPECT_EQ(TkWorkerScheduler<int>::Create(createInfo, &scheduler), TK_ERROR_INVALID_ARGUMENT);
    createInfo.workerCount = 1;
    createInfo.workLaneCapacity = 1;
    EXPECT_EQ(TkWorkerScheduler<int>::Create(createInfo, &scheduler), TK_ERROR_INVALID_ARGUMENT);
    createInfo.workLaneCapacity = 2;
    createInfo.maxMessagesPerDrain = 0;
    EXPECT_EQ(TkWorkerScheduler<int>::Create(createInfo, &scheduler), TK_ERROR_INVALID_ARGUMENT);
    createInfo.maxMessagesPerDrain = 1;
    createInfo.workLanes = nullptr;
    EXPECT_EQ(TkWorkerScheduler<int>::Create(createInfo, &scheduler), TK_ERROR_INVALID_ARGUMENT);
    createInfo.workLanes = &laneInfo;
    const TkWorkLaneCreateInfo<int> invalidLaneInfo{&context, nullptr};
    createInfo.workLanes = &invalidLaneInfo;
    EXPECT_EQ(TkWorkerScheduler<int>::Create(createInfo, &scheduler), TK_ERROR_INVALID_ARGUMENT);
    createInfo.workLanes = &laneInfo;
    createInfo.workLaneCount = 4;
    EXPECT_EQ(TkWorkerScheduler<int>::Create(createInfo, &scheduler), TK_ERROR_INVALID_ARGUMENT);
    createInfo.workLaneCount = 1;

    ASSERT_EQ(TkWorkerScheduler<int>::Create(createInfo, &scheduler), TK_SUCCESS);
    TkWorkerScheduler<int> *const original = scheduler.get();
    createInfo.maxMessagesPerDrain = 0;
    EXPECT_EQ(TkWorkerScheduler<int>::Create(createInfo, &scheduler), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(scheduler.get(), original);
    ASSERT_EQ(scheduler->Stop(TkWorkerPoolStopMode::Drain), TK_SUCCESS);
}

TEST(TkWorkerSchedulerContract, RejectsInvalidAndForeignHandlesWithoutConsumingInput)
{
    CounterContext firstContext;
    CounterContext secondContext;
    const TkWorkLaneCreateInfo<MoveOnlyValue> laneInfo{&firstContext, &CountMoveOnly};
    TkWorkerSchedulerCreateInfo<MoveOnlyValue> createInfo{1, 2, 2, 1, &laneInfo, 1};
    std::unique_ptr<TkWorkerScheduler<MoveOnlyValue>> first;
    std::unique_ptr<TkWorkerScheduler<MoveOnlyValue>> second;
    ASSERT_EQ(TkWorkerScheduler<MoveOnlyValue>::Create(createInfo, &first), TK_SUCCESS);

    const TkWorkLaneCreateInfo<MoveOnlyValue> secondLaneInfo{&secondContext, &CountMoveOnly};
    createInfo.workLanes = &secondLaneInfo;
    ASSERT_EQ(TkWorkerScheduler<MoveOnlyValue>::Create(createInfo, &second), TK_SUCCESS);

    TkWorkLaneHandle<MoveOnlyValue> firstHandle;
    TkWorkLaneHandle<MoveOnlyValue> secondHandle;
    ASSERT_EQ(first->GetWorkLaneHandle(0, &firstHandle), TK_SUCCESS);
    ASSERT_EQ(second->GetWorkLaneHandle(0, &secondHandle), TK_SUCCESS);

    MoveOnlyValue invalid(1);
    EXPECT_EQ(first->TryPost(TkWorkLaneHandle<MoveOnlyValue>(), std::move(invalid)), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(invalid.value, 1);

    MoveOnlyValue foreign(2);
    EXPECT_EQ(first->TryPost(secondHandle, std::move(foreign)), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(foreign.value, 2);

    TkWorkLaneHandle<MoveOnlyValue> originalHandle = firstHandle;
    EXPECT_EQ(first->GetWorkLaneHandle(1, &firstHandle), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(first->TryPost(firstHandle, MoveOnlyValue(4)), TK_SUCCESS);
    ASSERT_TRUE(WaitFor([&]() { return firstContext.count.load(std::memory_order_acquire) == 4; }));

    ASSERT_EQ(first->Stop(TkWorkerPoolStopMode::Discard), TK_SUCCESS);
    MoveOnlyValue closed(3);
    EXPECT_EQ(first->TryPost(originalHandle, std::move(closed)), TK_ERROR_INVALID_STATE);
    EXPECT_EQ(closed.value, 3);
    EXPECT_EQ(second->Stop(TkWorkerPoolStopMode::Discard), TK_SUCCESS);
}

TEST(TkWorkerSchedulerContract, SerializesWorkOnTheSameLane)
{
    SerialContext context;
    const TkWorkLaneCreateInfo<int> laneInfo{&context, &SerialInt};
    const TkWorkerSchedulerCreateInfo<int> createInfo{2, 2, 4, 8, &laneInfo, 1};
    std::unique_ptr<TkWorkerScheduler<int>> scheduler;
    ASSERT_EQ(TkWorkerScheduler<int>::Create(createInfo, &scheduler), TK_SUCCESS);

    TkWorkLaneHandle<int> handle;
    ASSERT_EQ(scheduler->GetWorkLaneHandle(0, &handle), TK_SUCCESS);
    ASSERT_EQ(scheduler->TryPost(handle, 1), TK_SUCCESS);
    ASSERT_TRUE(WaitFor([&]() { return context.firstGate.entered.load(std::memory_order_acquire); }));
    ASSERT_EQ(scheduler->TryPost(handle, 2), TK_SUCCESS);

    std::this_thread::yield();
    EXPECT_EQ(context.count.load(std::memory_order_acquire), 1);
    EXPECT_EQ(context.maxActive.load(std::memory_order_acquire), 1);

    context.firstGate.release.store(true, std::memory_order_release);
    ASSERT_TRUE(WaitFor([&]() { return context.count.load(std::memory_order_acquire) == 2; }));
    EXPECT_EQ(context.maxActive.load(std::memory_order_acquire), 1);
    ASSERT_EQ(scheduler->Stop(TkWorkerPoolStopMode::Drain), TK_SUCCESS);
}

TEST(TkWorkerSchedulerContract, RunsDifferentLanesInParallel)
{
    BlockingGate firstGate;
    BlockingGate secondGate;
    std::atomic<int> firstCount{0};
    std::atomic<int> secondCount{0};
    BlockingContext firstContext{&firstGate, &firstCount};
    BlockingContext secondContext{&secondGate, &secondCount};
    const TkWorkLaneCreateInfo<int> firstLaneInfo{&firstContext, &BlockInt};
    const TkWorkLaneCreateInfo<int> secondLaneInfo{&secondContext, &BlockInt};
    const TkWorkLaneCreateInfo<int> infos[] = {firstLaneInfo, secondLaneInfo};
    const TkWorkerSchedulerCreateInfo<int> createInfo{2, 2, 2, 1, infos, 2};
    std::unique_ptr<TkWorkerScheduler<int>> scheduler;
    ASSERT_EQ(TkWorkerScheduler<int>::Create(createInfo, &scheduler), TK_SUCCESS);

    TkWorkLaneHandle<int> firstHandle;
    TkWorkLaneHandle<int> secondHandle;
    ASSERT_EQ(scheduler->GetWorkLaneHandle(0, &firstHandle), TK_SUCCESS);
    ASSERT_EQ(scheduler->GetWorkLaneHandle(1, &secondHandle), TK_SUCCESS);
    ASSERT_EQ(scheduler->TryPost(firstHandle, 1), TK_SUCCESS);
    ASSERT_EQ(scheduler->TryPost(secondHandle, 2), TK_SUCCESS);
    ASSERT_TRUE(WaitFor([&]() {
        return firstGate.entered.load(std::memory_order_acquire) && secondGate.entered.load(std::memory_order_acquire);
    }));

    firstGate.release.store(true, std::memory_order_release);
    secondGate.release.store(true, std::memory_order_release);
    ASSERT_EQ(scheduler->Stop(TkWorkerPoolStopMode::Drain), TK_SUCCESS);
    EXPECT_EQ(firstCount.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(secondCount.load(std::memory_order_relaxed), 1);
}

TEST(TkWorkerSchedulerContract, EnforcesDrainBudgetBetweenLanes)
{
    FairContext context;
    const TkWorkLaneCreateInfo<int> laneInfos[] = {
        {&context, &FairFirst},
        {&context, &FairSecond},
    };
    const TkWorkerSchedulerCreateInfo<int> createInfo{1, 2, 8, 2, laneInfos, 2};
    std::unique_ptr<TkWorkerScheduler<int>> scheduler;
    ASSERT_EQ(TkWorkerScheduler<int>::Create(createInfo, &scheduler), TK_SUCCESS);

    TkWorkLaneHandle<int> firstHandle;
    TkWorkLaneHandle<int> secondHandle;
    ASSERT_EQ(scheduler->GetWorkLaneHandle(0, &firstHandle), TK_SUCCESS);
    ASSERT_EQ(scheduler->GetWorkLaneHandle(1, &secondHandle), TK_SUCCESS);
    for (int value = 0; value < 5; ++value)
    {
        ASSERT_EQ(scheduler->TryPost(firstHandle, std::move(value)), TK_SUCCESS);
    }

    ASSERT_TRUE(WaitFor([&]() { return context.firstGate.entered.load(std::memory_order_acquire); }));
    ASSERT_EQ(scheduler->TryPost(secondHandle, 5), TK_SUCCESS);
    context.firstGate.release.store(true, std::memory_order_release);
    ASSERT_TRUE(WaitFor([&]() { return context.secondCount.load(std::memory_order_acquire) == 1; }));
    EXPECT_FALSE(context.violation.load(std::memory_order_acquire));
    ASSERT_EQ(scheduler->Stop(TkWorkerPoolStopMode::Drain), TK_SUCCESS);
    EXPECT_EQ(context.firstCount.load(std::memory_order_relaxed), 5);
}

TEST(TkWorkerSchedulerContract, ClosesPostsBeforeDrainAndRejectsConcurrentStop)
{
    BlockingGate gate;
    std::atomic<int> count{0};
    BlockingContext context{&gate, &count};
    const TkWorkLaneCreateInfo<int> laneInfo{&context, &BlockInt};
    const TkWorkerSchedulerCreateInfo<int> createInfo{1, 2, 4, 1, &laneInfo, 1};
    std::unique_ptr<TkWorkerScheduler<int>> scheduler;
    ASSERT_EQ(TkWorkerScheduler<int>::Create(createInfo, &scheduler), TK_SUCCESS);

    TkWorkLaneHandle<int> handle;
    ASSERT_EQ(scheduler->GetWorkLaneHandle(0, &handle), TK_SUCCESS);
    ASSERT_EQ(scheduler->TryPost(handle, 1), TK_SUCCESS);
    ASSERT_TRUE(WaitFor([&]() { return gate.entered.load(std::memory_order_acquire); }));

    std::atomic<int> stopResult{TK_ERROR_UNKNOWN};
    std::thread stopper(
        [&]() { stopResult.store(scheduler->Stop(TkWorkerPoolStopMode::Drain), std::memory_order_release); });
    ASSERT_TRUE(WaitFor([&]() {
        int value = 2;
        return scheduler->TryPost(handle, std::move(value)) == TK_ERROR_INVALID_STATE;
    }));
    EXPECT_EQ(scheduler->Stop(TkWorkerPoolStopMode::Discard), TK_ERROR_INVALID_STATE);

    gate.release.store(true, std::memory_order_release);
    stopper.join();
    EXPECT_EQ(stopResult.load(std::memory_order_acquire), TK_SUCCESS);
}

TEST(TkWorkerSchedulerContract, DrainExecutesAllAcceptedMessages)
{
    CounterContext context;
    const TkWorkLaneCreateInfo<int> laneInfo{&context, &CountInt};
    const TkWorkerSchedulerCreateInfo<int> createInfo{2, 2, 8, 2, &laneInfo, 1};
    std::unique_ptr<TkWorkerScheduler<int>> scheduler;
    ASSERT_EQ(TkWorkerScheduler<int>::Create(createInfo, &scheduler), TK_SUCCESS);

    TkWorkLaneHandle<int> handle;
    ASSERT_EQ(scheduler->GetWorkLaneHandle(0, &handle), TK_SUCCESS);
    for (int value = 0; value < 8; ++value)
    {
        ASSERT_EQ(scheduler->TryPost(handle, std::move(value)), TK_SUCCESS);
    }

    ASSERT_EQ(scheduler->Stop(TkWorkerPoolStopMode::Drain), TK_SUCCESS);
    EXPECT_EQ(context.count.load(std::memory_order_acquire), 8);
    EXPECT_EQ(scheduler->Stop(TkWorkerPoolStopMode::Drain), TK_SUCCESS);
    EXPECT_EQ(scheduler->Stop(TkWorkerPoolStopMode::Discard), TK_SUCCESS);
}

TEST(TkWorkerSchedulerContract, DiscardJoinsInFlightAndDestroysQueuedMessages)
{
    std::atomic<int> destructionCount{0};
    std::atomic<int> invokeCount{0};
    BlockingGate gate;
    BlockingProbeContext context{&gate, &invokeCount};
    const TkWorkLaneCreateInfo<DestructionProbe> laneInfo{&context, &BlockProbe};
    const TkWorkerSchedulerCreateInfo<DestructionProbe> createInfo{1, 2, 4, 1, &laneInfo, 1};
    std::unique_ptr<TkWorkerScheduler<DestructionProbe>> scheduler;
    ASSERT_EQ(TkWorkerScheduler<DestructionProbe>::Create(createInfo, &scheduler), TK_SUCCESS);

    TkWorkLaneHandle<DestructionProbe> handle;
    ASSERT_EQ(scheduler->GetWorkLaneHandle(0, &handle), TK_SUCCESS);
    ASSERT_EQ(scheduler->TryPost(handle, DestructionProbe(&destructionCount)), TK_SUCCESS);
    ASSERT_TRUE(WaitFor([&]() { return gate.entered.load(std::memory_order_acquire); }));
    ASSERT_EQ(scheduler->TryPost(handle, DestructionProbe(&destructionCount)), TK_SUCCESS);
    ASSERT_EQ(scheduler->TryPost(handle, DestructionProbe(&destructionCount)), TK_SUCCESS);

    std::atomic<int> stopResult{TK_ERROR_UNKNOWN};
    std::thread stopper(
        [&]() { stopResult.store(scheduler->Stop(TkWorkerPoolStopMode::Discard), std::memory_order_release); });
    ASSERT_TRUE(WaitFor([&]() {
        TkWorkLaneHandle<DestructionProbe> observedHandle;
        const TkResult result = scheduler->GetWorkLaneHandle(0, &observedHandle);
        return result == TK_ERROR_INVALID_STATE;
    }));
    gate.release.store(true, std::memory_order_release);
    stopper.join();

    EXPECT_EQ(stopResult.load(std::memory_order_acquire), TK_SUCCESS);
    EXPECT_EQ(invokeCount.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(destructionCount.load(std::memory_order_relaxed), 3);
}

TEST(TkWorkerSchedulerContract, RejectsStopFromItsDrainCallback)
{
    std::atomic<int> callbackResult{TK_ERROR_UNKNOWN};
    StopCallbackContext context{nullptr, &callbackResult};
    const TkWorkLaneCreateInfo<int> laneInfo{&context, &StopFromCallback};
    const TkWorkerSchedulerCreateInfo<int> createInfo{1, 2, 2, 1, &laneInfo, 1};
    std::unique_ptr<TkWorkerScheduler<int>> scheduler;
    ASSERT_EQ(TkWorkerScheduler<int>::Create(createInfo, &scheduler), TK_SUCCESS);
    context.scheduler = scheduler.get();

    TkWorkLaneHandle<int> handle;
    ASSERT_EQ(scheduler->GetWorkLaneHandle(0, &handle), TK_SUCCESS);
    ASSERT_EQ(scheduler->TryPost(handle, 1), TK_SUCCESS);
    ASSERT_TRUE(WaitFor([&]() { return callbackResult.load(std::memory_order_acquire) != TK_ERROR_UNKNOWN; }));
    EXPECT_EQ(callbackResult.load(std::memory_order_relaxed), TK_ERROR_INVALID_STATE);
    ASSERT_EQ(scheduler->Stop(TkWorkerPoolStopMode::Drain), TK_SUCCESS);
}
