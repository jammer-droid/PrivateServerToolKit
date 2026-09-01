#include <pstk/execution/TkBoundedMpscQueue.hpp>
#include <pstk/execution/TkWorkItem.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <memory>
#include <thread>
#include <type_traits>
#include <vector>

namespace
{

struct MoveOnlyValue
{
    explicit MoveOnlyValue(int value) noexcept : value(value)
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

using MoveOnlyQueue = pstk::execution::TkBoundedMpscQueue<MoveOnlyValue>;

static_assert(!std::is_default_constructible<pstk::execution::TkWorkItem>::value,
              "TkWorkItem requires explicit callbacks");

struct DestructionProbe
{
    explicit DestructionProbe(std::atomic<int> *destructionCount) noexcept
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

struct WorkContext
{
    std::atomic<int> *invokeCount;
    std::atomic<int> *destroyCount;
};

void CountInvoke(void *context) noexcept
{
    auto *workContext = static_cast<WorkContext *>(context);
    workContext->invokeCount->fetch_add(1, std::memory_order_relaxed);
}

void CountDestroy(void *context) noexcept
{
    auto *workContext = static_cast<WorkContext *>(context);
    workContext->destroyCount->fetch_add(1, std::memory_order_relaxed);
    delete workContext;
}

void NoopDestroy(void *) noexcept
{
}

} // namespace

TEST(TkBoundedMpscQueueContract, RejectsInvalidCapacityAndPreservesOutput)
{
    std::unique_ptr<MoveOnlyQueue> queue;

    EXPECT_EQ(MoveOnlyQueue::Create(2, nullptr), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(MoveOnlyQueue::Create(0, &queue), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(queue.get(), nullptr);

    EXPECT_EQ(MoveOnlyQueue::Create(1, &queue), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(queue.get(), nullptr);

    EXPECT_EQ(MoveOnlyQueue::Create(3, &queue), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(queue.get(), nullptr);

    ASSERT_EQ(MoveOnlyQueue::Create(2, &queue), TK_SUCCESS);
    MoveOnlyQueue *original = queue.get();

    EXPECT_EQ(MoveOnlyQueue::Create(3, &queue), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(queue.get(), original);
}

TEST(TkBoundedMpscQueueContract, PreservesInputAndOutputOnFullOrEmpty)
{
    std::unique_ptr<MoveOnlyQueue> queue;
    ASSERT_EQ(MoveOnlyQueue::Create(2, &queue), TK_SUCCESS);

    MoveOnlyValue first(1);
    MoveOnlyValue second(2);
    ASSERT_TRUE(queue->TryPush(std::move(first)));
    ASSERT_TRUE(queue->TryPush(std::move(second)));

    MoveOnlyValue rejected(3);
    EXPECT_FALSE(queue->TryPush(std::move(rejected)));
    EXPECT_EQ(rejected.value, 3);

    MoveOnlyValue output(100);
    ASSERT_TRUE(queue->TryPop(output));
    EXPECT_EQ(output.value, 1);
    ASSERT_TRUE(queue->TryPop(output));
    EXPECT_EQ(output.value, 2);

    output.value = 100;
    EXPECT_FALSE(queue->TryPop(output));
    EXPECT_EQ(output.value, 100);
}

TEST(TkBoundedMpscQueueContract, SupportsMoveOnlyNonDefaultConstructibleValuesAndWrapAround)
{
    std::unique_ptr<MoveOnlyQueue> queue;
    ASSERT_EQ(MoveOnlyQueue::Create(2, &queue), TK_SUCCESS);

    for (int value = 0; value < 64; ++value)
    {
        MoveOnlyValue input(value);
        ASSERT_TRUE(queue->TryPush(std::move(input)));

        MoveOnlyValue output(-1);
        ASSERT_TRUE(queue->TryPop(output));
        EXPECT_EQ(output.value, value);
    }
}

TEST(TkBoundedMpscQueueContract, SupportsMultipleProducersAndOneConsumer)
{
    constexpr int producerCount = 4;
    constexpr int valuesPerProducer = 2000;
    constexpr int totalValues = producerCount * valuesPerProducer;

    std::unique_ptr<MoveOnlyQueue> queue;
    ASSERT_EQ(MoveOnlyQueue::Create(64, &queue), TK_SUCCESS);

    std::vector<std::thread> producers;
    producers.reserve(producerCount);

    for (int producerIndex = 0; producerIndex < producerCount; ++producerIndex)
    {
        producers.emplace_back([&, producerIndex]() {
            for (int valueIndex = 0; valueIndex < valuesPerProducer; ++valueIndex)
            {
                MoveOnlyValue input(producerIndex * valuesPerProducer + valueIndex);
                while (!queue->TryPush(std::move(input)))
                {
                    std::this_thread::yield();
                }
            }
        });
    }

    std::vector<bool> seen(totalValues, false);
    MoveOnlyValue output(-1);
    int consumed = 0;
    while (consumed < totalValues)
    {
        if (!queue->TryPop(output))
        {
            std::this_thread::yield();
            continue;
        }

        ++consumed;

        if (output.value < 0 || output.value >= totalValues)
        {
            ADD_FAILURE() << "consumer received an out-of-range value: " << output.value;
            continue;
        }

        if (seen[output.value])
        {
            ADD_FAILURE() << "consumer received duplicate value: " << output.value;
            continue;
        }

        seen[output.value] = true;
    }

    for (std::thread &producer : producers)
    {
        producer.join();
    }

    EXPECT_EQ(consumed, totalValues);
    EXPECT_TRUE(std::all_of(seen.begin(), seen.end(), [](bool value) { return value; }));
}

TEST(TkBoundedMpscQueueContract, DestroysQueuedValuesWhenQueueIsDestroyed)
{
    std::atomic<int> destructionCount(0);

    {
        std::unique_ptr<pstk::execution::TkBoundedMpscQueue<DestructionProbe>> queue;
        ASSERT_EQ(pstk::execution::TkBoundedMpscQueue<DestructionProbe>::Create(4, &queue), TK_SUCCESS);

        ASSERT_TRUE(queue->TryPush(DestructionProbe(&destructionCount)));
        ASSERT_TRUE(queue->TryPush(DestructionProbe(&destructionCount)));
        EXPECT_EQ(destructionCount.load(std::memory_order_relaxed), 0);
    }

    EXPECT_EQ(destructionCount.load(std::memory_order_relaxed), 2);
}

TEST(TkWorkItemContract, InvokesThenDestroysExactlyOnce)
{
    std::atomic<int> invokeCount(0);
    std::atomic<int> destroyCount(0);

    {
        auto *context = new WorkContext{&invokeCount, &destroyCount};
        pstk::execution::TkWorkItem item(context, &CountInvoke, &CountDestroy);
        item.Invoke();

        EXPECT_EQ(invokeCount.load(std::memory_order_relaxed), 1);
        EXPECT_EQ(destroyCount.load(std::memory_order_relaxed), 0);
    }

    EXPECT_EQ(invokeCount.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(destroyCount.load(std::memory_order_relaxed), 1);
}

TEST(TkWorkItemContract, DiscardDestroysWithoutInvoking)
{
    std::atomic<int> invokeCount(0);
    std::atomic<int> destroyCount(0);

    {
        auto *context = new WorkContext{&invokeCount, &destroyCount};
        pstk::execution::TkWorkItem item(context, &CountInvoke, &CountDestroy);
    }

    EXPECT_EQ(invokeCount.load(std::memory_order_relaxed), 0);
    EXPECT_EQ(destroyCount.load(std::memory_order_relaxed), 1);
}

TEST(TkWorkItemContract, MoveTransfersLifecycleOwnership)
{
    std::atomic<int> invokeCount(0);
    std::atomic<int> destroyCount(0);

    {
        auto *firstContext = new WorkContext{&invokeCount, &destroyCount};
        auto *secondContext = new WorkContext{&invokeCount, &destroyCount};
        pstk::execution::TkWorkItem first(firstContext, &CountInvoke, &CountDestroy);
        pstk::execution::TkWorkItem second(secondContext, &CountInvoke, &CountDestroy);

        second = std::move(first);
        second.Invoke();
    }

    EXPECT_EQ(invokeCount.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(destroyCount.load(std::memory_order_relaxed), 2);
}

TEST(TkWorkItemContract, MoveConstructionTransfersLifecycleOwnership)
{
    std::atomic<int> invokeCount(0);
    std::atomic<int> destroyCount(0);

    {
        auto *context = new WorkContext{&invokeCount, &destroyCount};
        pstk::execution::TkWorkItem source(context, &CountInvoke, &CountDestroy);
        pstk::execution::TkWorkItem destination(std::move(source));
        destination.Invoke();
    }

    EXPECT_EQ(invokeCount.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(destroyCount.load(std::memory_order_relaxed), 1);
}

TEST(TkWorkItemContract, AcceptsExplicitNoopDestroyCallback)
{
    std::atomic<int> invokeCount(0);
    auto *context = new WorkContext{&invokeCount, nullptr};

    {
        pstk::execution::TkWorkItem item(context, &CountInvoke, &NoopDestroy);
        item.Invoke();
    }

    delete context;
    EXPECT_EQ(invokeCount.load(std::memory_order_relaxed), 1);
}
