#include <pstk/execution/TkWorkItem.h>

#include <cassert>

namespace pstk::execution
{

void NoOpInvoke(void *) noexcept {};
void NoOpDestroy(void *) noexcept {};

TkWorkItem::TkWorkItem(void *const context, const TkWorkInvoke invoke, const TkWorkDestroy destroy) noexcept
    : context_(context), invoke_(invoke), destroy_(destroy)
{
    assert(invoke_ != nullptr);
    assert(destroy_ != nullptr);
}

TkWorkItem::~TkWorkItem() noexcept
{
    Reset();
}

TkWorkItem::TkWorkItem(TkWorkItem &&other) noexcept
    : context_(other.context_), invoke_(other.invoke_), destroy_(other.destroy_)
{
    other.Clear();
}

TkWorkItem &TkWorkItem::operator=(TkWorkItem &&other) noexcept
{
    if (this != &other)
    {
        Reset();
        this->context_ = other.context_;
        this->invoke_ = other.invoke_;
        this->destroy_ = other.destroy_;
        other.Clear();
    }

    return *this;
}

void TkWorkItem::Invoke() noexcept
{
    assert(invoke_ != nullptr);

    const TkWorkInvoke invoke = invoke_;
    invoke_ = nullptr;
    invoke(context_);
}

void TkWorkItem::Reset() noexcept
{
    if (destroy_ == nullptr)
    {
        return;
    }

    const TkWorkDestroy destroy = destroy_;
    void *const context = context_;

    Clear();
    destroy(context);
}

void TkWorkItem::Clear() noexcept
{
    context_ = nullptr;
    invoke_ = nullptr;
    destroy_ = nullptr;
}

} // namespace pstk::execution
