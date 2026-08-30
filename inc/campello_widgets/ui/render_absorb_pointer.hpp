#pragma once

#include <campello_widgets/ui/render_box.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A RenderBox that, while absorbing, prevents its subtree from
     * receiving pointer events but still claims the hit itself.
     *
     * Unlike `RenderIgnorePointer` (which lets events pass through to
     * whatever is behind it), this box swallows the event: `hitTestChildren()`
     * is disabled and `hitTestSelf()` claims any point within this box's own
     * bounds, so nothing painted behind it (e.g. a sibling lower in a Stack)
     * receives the event either.
     *
     * Matches Flutter's `AbsorbPointer` widget.
     */
    class RenderAbsorbPointer : public RenderBox
    {
    public:
        explicit RenderAbsorbPointer(bool absorbing = true) : absorbing_(absorbing) {}

        void setAbsorbing(bool absorbing) noexcept { absorbing_ = absorbing; }
        bool absorbing() const noexcept { return absorbing_; }

        bool hitTestChildren(HitTestResult& result, const Offset& position) override
        {
            if (absorbing_) return false;
            return RenderBox::hitTestChildren(result, position);
        }

        bool hitTestSelf(const Offset& position) const override
        {
            if (absorbing_) return true;
            return RenderBox::hitTestSelf(position);
        }

    private:
        bool absorbing_;
    };

} // namespace systems::leal::campello_widgets
