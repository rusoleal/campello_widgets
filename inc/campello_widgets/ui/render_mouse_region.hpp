#pragma once

#include <functional>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/system_mouse_cursor.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief RenderBox that fires enter/hover/exit callbacks as the cursor
     *        moves over and away from the widget.
     *
     * Registers with `PointerDispatcher`. Exit is detected when the dispatcher
     * sends a synthetic cancel event as the cursor moves to a different region.
     */
    class RenderMouseRegion : public RenderBox
    {
    public:
        std::function<void()>        on_enter;
        std::function<void()>        on_exit;
        std::function<void(Offset)>  on_hover;
        SystemMouseCursor            cursor = SystemMouseCursor::arrow;

        RenderMouseRegion();
        ~RenderMouseRegion() override;

        void performLayout() override;
        void performPaint(PaintContext& ctx, const Offset& offset) override;

    private:
        void onPointerEvent(const PointerEvent& event);
        void onTick(uint64_t now_ms);

        bool hovered_ = false;
    };

} // namespace systems::leal::campello_widgets
