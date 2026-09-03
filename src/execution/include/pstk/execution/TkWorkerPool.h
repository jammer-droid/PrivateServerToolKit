#pragma once

#include <pstk/TkResult.h>
#include <pstk/execution/TkWorkItem.h>

#include <cstddef>
#include <memory>

namespace pstk::execution
{

enum class TkWorkerPoolStopMode
{
    Drain,
    Discard
};

class TkWorkerPool final
{
  public:
    static TkResult Create(std::size_t workerCount, std::size_t readyCapacity,
                           std::unique_ptr<TkWorkerPool> *outPool) noexcept;

    ~TkWorkerPool() noexcept;

    TkWorkerPool(const TkWorkerPool &) = delete;
    TkWorkerPool &operator=(const TkWorkerPool &) = delete;

    TkWorkerPool(TkWorkerPool &&) = delete;
    TkWorkerPool &operator=(TkWorkerPool &&) = delete;

    TkResult TrySchedule(TkWorkItem &&item) noexcept;
    TkResult Stop(TkWorkerPoolStopMode mode) noexcept;

  private:
    class Impl;

    explicit TkWorkerPool(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;
};
} // namespace pstk::execution
