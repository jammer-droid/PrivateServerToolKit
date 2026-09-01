#pragma once

namespace pstk::execution
{

using TkWorkInvoke = void (*)(void *) noexcept;
using TkWorkDestroy = void (*)(void *) noexcept;

class TkWorkItem final
{
  public:
    TkWorkItem(void *const context, const TkWorkInvoke invoke, const TkWorkDestroy destroy) noexcept;

    ~TkWorkItem() noexcept;

    TkWorkItem(const TkWorkItem &) = delete;
    TkWorkItem &operator=(const TkWorkItem &) = delete;

    TkWorkItem(TkWorkItem &&other) noexcept;
    TkWorkItem &operator=(TkWorkItem &&other) noexcept;

    void Invoke() noexcept;

  private:
    void Reset() noexcept;
    void Clear() noexcept;

    void *context_ = nullptr;
    TkWorkInvoke invoke_ = nullptr;
    TkWorkDestroy destroy_ = nullptr;
};
} // namespace pstk::execution
