#pragma once

#include <string>
#include <campello_widgets/ui/size.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/offset.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Layout-time callback surface a `MultiChildLayoutDelegate` uses
     * to query/position its children by id.
     *
     * Implemented by `RenderCustomMultiChildLayout`; set on the delegate
     * only for the duration of one `performLayout()` call.
     */
    class MultiChildLayoutContext
    {
    public:
        virtual ~MultiChildLayoutContext() = default;

        virtual bool hasChild(const std::string& id) const = 0;
        virtual Size layoutChild(const std::string& id, const BoxConstraints& constraints) = 0;
        virtual void positionChild(const std::string& id, const Offset& offset) = 0;
    };

    /**
     * @brief Delegate that lays out an arbitrary set of id-tagged children
     * (see `LayoutId`) via explicit per-child `layoutChild()`/
     * `positionChild()` calls, rather than a fixed layout algorithm.
     *
     * Matches Flutter's `MultiChildLayoutDelegate`.
     */
    class MultiChildLayoutDelegate
    {
    public:
        virtual ~MultiChildLayoutDelegate() = default;

        /** @brief This layout's own size. Default: fills the incoming constraints. */
        virtual Size getSize(const BoxConstraints& constraints) const
        {
            return {constraints.max_width, constraints.max_height};
        }

        /** @brief Lays out and positions every child via the protected helpers below. */
        virtual void performLayout(const Size& size) = 0;

        /** @brief Whether a change from `old_delegate` requires relayout. */
        virtual bool shouldRelayout(const MultiChildLayoutDelegate& old_delegate) const = 0;

    protected:
        bool hasChild(const std::string& id) const
        {
            return ctx_ && ctx_->hasChild(id);
        }

        Size layoutChild(const std::string& id, const BoxConstraints& constraints)
        {
            return ctx_ ? ctx_->layoutChild(id, constraints) : Size::zero();
        }

        void positionChild(const std::string& id, const Offset& offset)
        {
            if (ctx_) ctx_->positionChild(id, offset);
        }

    private:
        friend class RenderCustomMultiChildLayout;
        MultiChildLayoutContext* ctx_ = nullptr;
    };

} // namespace systems::leal::campello_widgets
