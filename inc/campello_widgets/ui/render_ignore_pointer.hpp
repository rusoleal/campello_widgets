#pragma once

#include <campello_widgets/ui/render_box.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A RenderBox that, while ignoring, removes itself and its whole
     * subtree from hit-testing entirely.
     *
     * Layout and paint are unaffected -- only `hitTestChildren()` is
     * disabled; `hitTestSelf()` stays false (the default), so pointer events
     * simply fall through to whatever is behind this box in the tree.
     *
     * Matches Flutter's `IgnorePointer` widget.
     */
    class RenderIgnorePointer : public RenderBox
    {
    public:
        explicit RenderIgnorePointer(bool ignoring = true) : ignoring_(ignoring) {}

        void setIgnoring(bool ignoring) noexcept { ignoring_ = ignoring; }
        bool ignoring() const noexcept { return ignoring_; }

        bool hitTestChildren(HitTestResult& result, const Offset& position) override
        {
            if (ignoring_) return false;
            return RenderBox::hitTestChildren(result, position);
        }

    private:
        bool ignoring_;
    };

} // namespace systems::leal::campello_widgets
