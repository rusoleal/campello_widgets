#pragma once

#include <functional>
#include <memory>
#include <campello_widgets/diagnostics/diagnostic_property.hpp>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/focus_node.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A widget that detects pointer gestures on its subtree.
     *
     * Wraps a RenderGestureDetector that registers with the active
     * PointerDispatcher. Supply whichever callbacks you need — unset
     * callbacks (nullptr) are simply not called.
     *
     * Supported gestures:
     *  - on_tap        — pointer down + up with travel < 18 px
     *  - on_pan_down   — pointer down, fired immediately with the box-local
     *                    position (before the slop gate); akin to Flutter's
     *                    GestureDetector.onPanDown(DragDownDetails)
     *  - on_pan_update — pointer moved while down, after exceeding 18 px slop;
     *                    called with the Offset delta since the last move
     *  - on_pan_end    — pointer lifted after a pan
     *
     * Usage:
     * @code
     * auto btn = std::make_shared<GestureDetector>();
     * btn->child      = someWidget;
     * btn->on_tap     = [] { NSLog(@"tapped!"); };
     * @endcode
     */
    class GestureDetector : public SingleChildRenderObjectWidget
    {
    public:
        std::function<void()>          on_tap;
        std::function<void()>          on_double_tap;
        std::function<void()>          on_long_press;
        std::function<void(Offset)>    on_pan_down;
        std::function<void(Offset)>    on_pan_update;
        std::function<void()>          on_pan_end;
        std::function<void(Offset)>    on_scroll;
        std::function<void(bool)>      on_press_change;

        // Keyboard focus — opt-in via `focusable` (defaults false, since
        // most GestureDetector usages wrap non-control content). See
        // RenderGestureDetector's class doc for the full contract.
        std::shared_ptr<FocusNode>     focus_node;
        bool                           autofocus = false;
        bool                           focusable = false;
        std::function<void(bool)>      on_focus_change;

        GestureDetector() = default;
        explicit GestureDetector(WidgetRef c) { child = std::move(c); }
        explicit GestureDetector(WidgetRef c, std::function<void()> tap)
            : on_tap(std::move(tap))
        {
            child = std::move(c);
        }
        explicit GestureDetector(
            WidgetRef c,
            std::function<void()> tap,
            std::function<void()> double_tap,
            std::function<void()> long_press = nullptr)
            : on_tap(std::move(tap))
            , on_double_tap(std::move(double_tap))
            , on_long_press(std::move(long_press))
        {
            child = std::move(c);
        }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& render_object) const override;
        void debugFillProperties(DiagnosticsPropertyBuilder& properties) const override;

    };

} // namespace systems::leal::campello_widgets
