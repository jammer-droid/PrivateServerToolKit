#include <pstk/execution/TkSerialWorkLane.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <memory>
#include <thread>
#include <utility>

namespace
{

struct MoveOnlyValue
{
    explicit MoveOnlyValue(const int value) noexcept : value(value), gate(nullptr)
    {
    }

    MoveOnlyValue(const int value, struct PublishGate *const gate) noexcept : value(value), gate(gate)
    {
    }

    MoveOnlyValue(const MoveOnlyValue &) = delete;
    MoveOnlyValue &operator=(const MoveOnlyValue &) = delete;

    MoveOnlyValue(MoveOnlyValue &&other) noexcept;

    MoveOnlyValue &operator=(MoveOnlyValue &&other) noexcept
    {
        if (this != &other)
        {
            value = other.value;
            gate = other.gate;
            other.value = -1;
            other.gate = nullptr;
        }

        return *this;
    }

    ~MoveOnlyValue() noexcept = default;

    int value;
    struct PublishGate *gate;
};

struct PublishGate
{
    std::atomic<bool> moveStarted{false};
    std::atomic<bool> allowMove{false};
};

MoveOnlyValue::MoveOnlyValue(MoveOnlyValue &&other) noexcept : value(other.value), gate(other.gate)
{
    if (gate != nullptr)
    {
        gate->moveStarted.store(true, std::memory_order_release);
        while (!gate->allowMove.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }
    }

    other.value = -1;
    other.gate = nullptr;
}

using WorkLane = pstk::execution::TkSerialWorkLane<MoveOnlyValue>;

struct RecordedValues
{
    int values[4]{};
    std::size_t count = 0;
};

void RecordValue(void *const rawContext, MoveOnlyValue &&value) noexcept
{
    RecordedValues *const context = static_cast<RecordedValues *>(rawContext);
    if (context->count >= 4)
    {
        ADD_FAILURE() << "Drain invoked more callbacks than expected";
        return;
    }

    context->values[context->count++] = value.value;
}

void IgnoreValue(void *, MoveOnlyValue &&) noexcept
{
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

using ProbeWorkLane = pstk::execution::TkSerialWorkLane<DestructionProbe>;

template <typename Predicate> bool WaitFor(Predicate &&predicate)
{
    for (int attempt = 0; attempt < 1000000; ++attempt)
    {
        if (predicate())
        {
            return true;
        }

        std::this_thread::yield();
    }

    return false;
}

} // namespace

TEST(TkSerialWorkLaneContract, RejectsInvalidCapacityAndPreservesOutput)
{
    std::unique_ptr<WorkLane> workLane;

    EXPECT_EQ(WorkLane::Create(2, nullptr), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(WorkLane::Create(0, &workLane), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(workLane.get(), nullptr);
    EXPECT_EQ(WorkLane::Create(1, &workLane), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(WorkLane::Create(3, &workLane), TK_ERROR_INVALID_ARGUMENT);

    ASSERT_EQ(WorkLane::Create(2, &workLane), TK_SUCCESS);
    WorkLane *const original = workLane.get();
    EXPECT_EQ(WorkLane::Create(3, &workLane), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(workLane.get(), original);
}

TEST(TkSerialWorkLaneContract, PublishesWithSingleScheduleSignalAndPreservesFullInput)
{
    std::unique_ptr<WorkLane> workLane;
    ASSERT_EQ(WorkLane::Create(2, &workLane), TK_SUCCESS);

    MoveOnlyValue first(1);
    bool shouldSchedule = false;
    ASSERT_EQ(workLane->TryPublish(std::move(first), &shouldSchedule), TK_SUCCESS);
    EXPECT_TRUE(shouldSchedule);

    MoveOnlyValue second(2);
    shouldSchedule = true;
    ASSERT_EQ(workLane->TryPublish(std::move(second), &shouldSchedule), TK_SUCCESS);
    EXPECT_FALSE(shouldSchedule);

    MoveOnlyValue rejected(3);
    shouldSchedule = true;
    EXPECT_EQ(workLane->TryPublish(std::move(rejected), &shouldSchedule), TK_ERROR_CAPACITY_EXCEEDED);
    EXPECT_EQ(rejected.value, 3);
    EXPECT_TRUE(shouldSchedule);

    MoveOnlyValue nullOutput(4);
    EXPECT_EQ(workLane->TryPublish(std::move(nullOutput), nullptr), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(nullOutput.value, 4);
}

TEST(TkSerialWorkLaneContract, DrainsFifoUpToBudgetAndReturnsReschedule)
{
    std::unique_ptr<WorkLane> workLane;
    ASSERT_EQ(WorkLane::Create(4, &workLane), TK_SUCCESS);
    EXPECT_TRUE(workLane->IsQuiescent());

    bool shouldSchedule = false;
    ASSERT_EQ(workLane->TryPublish(MoveOnlyValue(1), &shouldSchedule), TK_SUCCESS);
    EXPECT_FALSE(workLane->IsQuiescent());
    ASSERT_EQ(workLane->TryPublish(MoveOnlyValue(2), &shouldSchedule), TK_SUCCESS);
    ASSERT_EQ(workLane->TryPublish(MoveOnlyValue(3), &shouldSchedule), TK_SUCCESS);

    RecordedValues values;
    ASSERT_EQ(workLane->Drain(2, &values, RecordValue, &shouldSchedule), TK_SUCCESS);
    ASSERT_EQ(values.count, 2U);
    EXPECT_EQ(values.values[0], 1);
    EXPECT_EQ(values.values[1], 2);
    EXPECT_TRUE(shouldSchedule);
    EXPECT_FALSE(workLane->IsQuiescent());

    ASSERT_EQ(workLane->Drain(2, &values, RecordValue, &shouldSchedule), TK_SUCCESS);
    ASSERT_EQ(values.count, 3U);
    EXPECT_EQ(values.values[2], 3);
    EXPECT_FALSE(shouldSchedule);
    EXPECT_TRUE(workLane->IsQuiescent());
}

TEST(TkSerialWorkLaneContract, RejectsInvalidDrainRequestsAndPreservesOutput)
{
    std::unique_ptr<WorkLane> workLane;
    ASSERT_EQ(WorkLane::Create(2, &workLane), TK_SUCCESS);

    bool shouldSchedule = true;
    EXPECT_EQ(workLane->Drain(1, nullptr, IgnoreValue, &shouldSchedule), TK_ERROR_INVALID_STATE);
    EXPECT_TRUE(shouldSchedule);

    ASSERT_EQ(workLane->TryPublish(MoveOnlyValue(1), &shouldSchedule), TK_SUCCESS);
    EXPECT_EQ(workLane->Drain(0, nullptr, IgnoreValue, &shouldSchedule), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_TRUE(shouldSchedule);
    EXPECT_EQ(workLane->Drain(1, nullptr, nullptr, &shouldSchedule), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_TRUE(shouldSchedule);
    EXPECT_EQ(workLane->Drain(1, nullptr, IgnoreValue, nullptr), TK_ERROR_INVALID_ARGUMENT);

    struct DrainContext
    {
        WorkLane *workLane;
        int count = 0;
    };
    DrainContext context{workLane.get()};

    ASSERT_EQ(workLane->Drain(
                  1, &context,
                  [](void *const rawContext, MoveOnlyValue &&value) noexcept {
                      DrainContext *const drainContext = static_cast<DrainContext *>(rawContext);
                      ++drainContext->count;
                      EXPECT_EQ(value.value, 1);
                      EXPECT_FALSE(drainContext->workLane->IsQuiescent());

                      bool nestedShouldSchedule = true;
                      EXPECT_EQ(drainContext->workLane->Drain(1, nullptr, IgnoreValue, &nestedShouldSchedule),
                                TK_ERROR_INVALID_STATE);
                      EXPECT_TRUE(nestedShouldSchedule);
                  },
                  &shouldSchedule),
              TK_SUCCESS);
    EXPECT_EQ(context.count, 1);
    EXPECT_FALSE(shouldSchedule);
    EXPECT_EQ(workLane->Drain(1, nullptr, IgnoreValue, &shouldSchedule), TK_ERROR_INVALID_STATE);
    EXPECT_FALSE(shouldSchedule);
}

TEST(TkSerialWorkLaneContract, RechecksMessagesPublishedDuringDrainCallback)
{
    std::unique_ptr<WorkLane> workLane;
    ASSERT_EQ(WorkLane::Create(2, &workLane), TK_SUCCESS);

    bool shouldSchedule = false;
    ASSERT_EQ(workLane->TryPublish(MoveOnlyValue(1), &shouldSchedule), TK_SUCCESS);
    ASSERT_TRUE(shouldSchedule);

    struct PublishContext
    {
        WorkLane *workLane;
        TkResult result = TK_ERROR_UNKNOWN;
        bool shouldSchedule = true;
    };
    PublishContext context{workLane.get()};

    ASSERT_EQ(workLane->Drain(
                  1, &context,
                  [](void *const rawContext, MoveOnlyValue &&value) noexcept {
                      PublishContext *const publishContext = static_cast<PublishContext *>(rawContext);
                      EXPECT_EQ(value.value, 1);
                      publishContext->result =
                          publishContext->workLane->TryPublish(MoveOnlyValue(2), &publishContext->shouldSchedule);
                  },
                  &shouldSchedule),
              TK_SUCCESS);
    EXPECT_EQ(context.result, TK_SUCCESS);
    EXPECT_FALSE(context.shouldSchedule);
    EXPECT_TRUE(shouldSchedule);

    RecordedValues values;
    ASSERT_EQ(workLane->Drain(1, &values, RecordValue, &shouldSchedule), TK_SUCCESS);
    ASSERT_EQ(values.count, 1U);
    EXPECT_EQ(values.values[0], 2);
    EXPECT_FALSE(shouldSchedule);
}

TEST(TkSerialWorkLaneContract, ProducerSchedulesAfterDrainObservesUnpublishedSlot)
{
    std::unique_ptr<WorkLane> workLane;
    ASSERT_EQ(WorkLane::Create(2, &workLane), TK_SUCCESS);

    bool shouldSchedule = false;
    ASSERT_EQ(workLane->TryPublish(MoveOnlyValue(1), &shouldSchedule), TK_SUCCESS);

    PublishGate gate;
    std::atomic<bool> startProducer{false};
    MoveOnlyValue item(2, &gate);
    std::atomic<int> producerResult{TK_ERROR_UNKNOWN};
    std::atomic<bool> producerShouldSchedule{false};
    std::thread producer([&]() {
        if (!WaitFor([&]() { return startProducer.load(std::memory_order_acquire); }))
        {
            return;
        }

        bool producerSignal = false;
        producerResult.store(workLane->TryPublish(std::move(item), &producerSignal), std::memory_order_release);
        producerShouldSchedule.store(producerSignal, std::memory_order_release);
    });

    struct DrainContext
    {
        PublishGate *gate;
        std::atomic<bool> *startProducer;
        bool moveStarted = false;
    };
    DrainContext context{&gate, &startProducer};
    shouldSchedule = true;
    const TkResult drainResult = workLane->Drain(
        2, &context,
        [](void *const rawContext, MoveOnlyValue &&value) noexcept {
            DrainContext *const drainContext = static_cast<DrainContext *>(rawContext);
            EXPECT_EQ(value.value, 1);
            drainContext->startProducer->store(true, std::memory_order_release);
            drainContext->moveStarted =
                WaitFor([&]() { return drainContext->gate->moveStarted.load(std::memory_order_acquire); });
        },
        &shouldSchedule);

    startProducer.store(true, std::memory_order_release);
    gate.allowMove.store(true, std::memory_order_release);
    producer.join();

    ASSERT_TRUE(context.moveStarted);
    ASSERT_EQ(drainResult, TK_SUCCESS);
    EXPECT_FALSE(shouldSchedule);
    EXPECT_EQ(producerResult.load(std::memory_order_acquire), TK_SUCCESS);
    EXPECT_TRUE(producerShouldSchedule.load(std::memory_order_acquire));

    RecordedValues values;
    ASSERT_EQ(workLane->Drain(1, &values, RecordValue, &shouldSchedule), TK_SUCCESS);
    ASSERT_EQ(values.count, 1U);
    EXPECT_EQ(values.values[0], 2);
    EXPECT_FALSE(shouldSchedule);
}

TEST(TkSerialWorkLaneContract, SupportsMultipleProducersWithOneDrainOwner)
{
    constexpr int producerCount = 4;
    constexpr int valuesPerProducer = 250;
    constexpr int totalValues = producerCount * valuesPerProducer + 1;

    std::unique_ptr<WorkLane> workLane;
    ASSERT_EQ(WorkLane::Create(64, &workLane), TK_SUCCESS);

    bool shouldSchedule = false;
    ASSERT_EQ(workLane->TryPublish(MoveOnlyValue(0), &shouldSchedule), TK_SUCCESS);
    ASSERT_TRUE(shouldSchedule);
    std::atomic<bool> drainReady{true};

    std::unique_ptr<std::atomic<int>[]> seen(new std::atomic<int>[totalValues]);
    for (int index = 0; index < totalValues; ++index)
    {
        seen[index].store(0, std::memory_order_relaxed);
    }

    std::atomic<int> producerDone{0};
    std::atomic<int> producerFailure{0};
    struct DrainContext
    {
        std::atomic<int> *seen;
        std::atomic<int> *failure;
        int valueCount;
        int consumedCount = 0;
    };
    DrainContext context{seen.get(), &producerFailure, totalValues};

    std::thread producers[producerCount];
    for (int producerIndex = 0; producerIndex < producerCount; ++producerIndex)
    {
        producers[producerIndex] = std::thread([&, producerIndex]() {
            for (int valueIndex = 0; valueIndex < valuesPerProducer; ++valueIndex)
            {
                const int id = producerIndex * valuesPerProducer + valueIndex + 1;
                MoveOnlyValue item(id);
                bool accepted = false;
                for (int attempt = 0; attempt < 1000000; ++attempt)
                {
                    bool producerSignal = true;
                    const TkResult result = workLane->TryPublish(std::move(item), &producerSignal);
                    if (result == TK_SUCCESS)
                    {
                        if (producerSignal)
                        {
                            drainReady.store(true, std::memory_order_release);
                        }
                        accepted = true;
                        break;
                    }

                    if (result != TK_ERROR_CAPACITY_EXCEEDED)
                    {
                        producerFailure.fetch_add(1, std::memory_order_relaxed);
                        break;
                    }

                    std::this_thread::yield();
                }

                if (!accepted)
                {
                    producerFailure.fetch_add(1, std::memory_order_relaxed);
                    producerDone.fetch_add(1, std::memory_order_release);
                    return;
                }
            }

            producerDone.fetch_add(1, std::memory_order_release);
        });
    }

    const bool completed = WaitFor([&]() {
        if (drainReady.exchange(false, std::memory_order_acq_rel))
        {
            bool reschedule = false;
            const TkResult result = workLane->Drain(
                64, &context,
                [](void *const rawContext, MoveOnlyValue &&value) noexcept {
                    DrainContext *const drainContext = static_cast<DrainContext *>(rawContext);
                    ++drainContext->consumedCount;
                    if (value.value < 0 || value.value >= drainContext->valueCount ||
                        drainContext->seen[value.value].fetch_add(1, std::memory_order_relaxed) != 0)
                    {
                        drainContext->failure->fetch_add(1, std::memory_order_relaxed);
                    }
                },
                &reschedule);

            if (result != TK_SUCCESS)
            {
                producerFailure.fetch_add(1, std::memory_order_relaxed);
            }

            if (reschedule)
            {
                drainReady.store(true, std::memory_order_release);
            }
        }

        return producerDone.load(std::memory_order_acquire) == producerCount && context.consumedCount == totalValues;
    });

    for (std::thread &producer : producers)
    {
        producer.join();
    }

    EXPECT_TRUE(completed);
    EXPECT_TRUE(workLane->IsQuiescent());
    EXPECT_EQ(producerFailure.load(std::memory_order_relaxed), 0);
    for (int id = 0; id < totalValues; ++id)
    {
        EXPECT_EQ(seen[id].load(std::memory_order_relaxed), 1);
    }
}

TEST(TkSerialWorkLaneContract, DestroysQueuedItemsWhenWorkLaneIsDestroyed)
{
    std::atomic<int> destructionCount{0};
    {
        std::unique_ptr<ProbeWorkLane> workLane;
        ASSERT_EQ(ProbeWorkLane::Create(4, &workLane), TK_SUCCESS);

        bool shouldSchedule = false;
        ASSERT_EQ(workLane->TryPublish(DestructionProbe(&destructionCount), &shouldSchedule), TK_SUCCESS);
        ASSERT_EQ(workLane->TryPublish(DestructionProbe(&destructionCount), &shouldSchedule), TK_SUCCESS);
        EXPECT_EQ(destructionCount.load(std::memory_order_relaxed), 0);
    }

    EXPECT_EQ(destructionCount.load(std::memory_order_relaxed), 2);
}
