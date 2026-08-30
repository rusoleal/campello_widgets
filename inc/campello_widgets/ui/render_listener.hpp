#pragma once

#include <functional>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/pointer_event.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief RenderBox that forwards raw pointer events verbatim, with no
     * gesture disambiguation/recognition of any kind.
     *
     * Registers with `PointerDispatcher` like `RenderMouseRegion`, but
     * dispatches every `PointerEventKind` straight through to the matching
     * callback instead of synthesizing enter/exit/hover semantics.
     */
    class RenderListener : public RenderBox
    {
    public:
        std::function<void(const PointerEvent&)> on_pointer_down;
        std::function<void(const PointerEvent&)> on_pointer_move;
        std::function<void(const PointerEvent&)> on_pointer_up;
        std::function<void(const PointerEvent&)> on_pointer_cancel;
        std::function<void(const PointerEvent&)> on_pointer_signal;

        RenderListener();
        ~RenderListener() override;

        void attach() override;
        void detach() override;

        void performLayout() override;
        void performPaint(PaintContext& ctx, const Offset& offset) override;

        // Claims hits within its own bounds so raw events reach this widget
        // even over an area with no other interactive descendant -- see
        // RenderBox::hitTestSelf().
        bool hitTestSelf(const Offset&) const override { return true; }

    private:
        void onPointerEvent(const PointerEvent& event);
    };

} // namespace systems::leal::campello_widgets
