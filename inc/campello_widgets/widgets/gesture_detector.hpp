#pragma once

#include <functional>
#include <memory>
#include <campello_widgets/diagnostics/diagnostic_property.hpp>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/focus_node.hpp>
#include <campello_widgets/ui/gesture_details.hpp>
#include <campello_widgets/ui/hit_test.hpp>

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
     *  - on_secondary_tap / on_tertiary_tap_* — same shape for a right-click
     *                    / middle-click. No secondary double-tap and no
     *                    plain on_tertiary_tap, matching Flutter.
     *  - on_pan_*, on_horizontal_drag_*, on_vertical_drag_*, on_scale_* —
     *                    four mutually exclusive gesture families (set
     *                    callbacks from at most one on a given detector —
     *                    debug-asserted, mirroring Flutter's own
     *                    GestureDetector). The three drag families have
     *                    down/start/update/end callbacks taking
     *                    DragDownDetails/DragStartDetails/DragUpdateDetails/
     *                    DragEndDetails (see gesture_details.hpp);
     *                    DragEndDetails carries release velocity. Pan is
     *                    omnidirectional; horizontal/vertical only count
     *                    movement along their own axis toward the slop
     *                    that starts the drag. on_scale_* tracks any
     *                    number of concurrent pointers for pinch/rotate
     *                    (ScaleStartDetails/ScaleUpdateDetails/
     *                    ScaleEndDetails), degrading to single-finger pan-
     *                    like behavior (scale pinned at 1.0) with only one
     *                    pointer down.
     *  - on_force_press_start/peak/update/end — staged pressure for a
     *                    stylus pointer only (ForcePressDetails); other
     *                    device kinds never fire these. Thresholds are
     *                    force_press_start_pressure/force_press_peak_pressure
     *                    (defaults 0.4/0.85). Coexists with tap/pan rather
     *                    than competing with them.
     *  - behavior      — HitTestBehavior (opaque/translucent/deferToChild);
     *                    defaults to opaque. See ui/hit_test.hpp.
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

        std::function<void(TapDownDetails)>    on_tap_down;
        std::function<void(TapUpDetails)>      on_tap_up;
        std::function<void()>                  on_tap_cancel;

        /// Right-click. No secondary double-tap -- Flutter has none either.
        std::function<void()>                  on_secondary_tap;
        std::function<void(TapDownDetails)>    on_secondary_tap_down;
        std::function<void(TapUpDetails)>      on_secondary_tap_up;
        std::function<void()>                  on_secondary_tap_cancel;

        /// Middle-click. No plain on_tertiary_tap -- Flutter has none
        /// either, only the down/up/cancel granular callbacks.
        std::function<void(TapDownDetails)>    on_tertiary_tap_down;
        std::function<void(TapUpDetails)>      on_tertiary_tap_up;
        std::function<void()>                  on_tertiary_tap_cancel;

        std::function<void(LongPressDownDetails)>       on_long_press_down;
        std::function<void()>                           on_long_press_cancel;
        std::function<void(LongPressStartDetails)>      on_long_press_start;
        std::function<void(LongPressMoveUpdateDetails)> on_long_press_move_update;
        std::function<void(LongPressEndDetails)>        on_long_press_end;

        std::function<void(DragDownDetails)>   on_pan_down;
        std::function<void(DragStartDetails)>  on_pan_start;
        std::function<void(DragUpdateDetails)> on_pan_update;
        std::function<void(DragEndDetails)>    on_pan_end;

        std::function<void(DragDownDetails)>   on_horizontal_drag_down;
        std::function<void(DragStartDetails)>  on_horizontal_drag_start;
        std::function<void(DragUpdateDetails)> on_horizontal_drag_update;
        std::function<void(DragEndDetails)>    on_horizontal_drag_end;

        std::function<void(DragDownDetails)>   on_vertical_drag_down;
        std::function<void(DragStartDetails)>  on_vertical_drag_start;
        std::function<void(DragUpdateDetails)> on_vertical_drag_update;
        std::function<void(DragEndDetails)>    on_vertical_drag_end;

        DragStartBehavior              drag_start_behavior = DragStartBehavior::down;

        std::function<void(ScaleStartDetails)>  on_scale_start;
        std::function<void(ScaleUpdateDetails)> on_scale_update;
        std::function<void(ScaleEndDetails)>    on_scale_end;

        /// Force-press thresholds and callbacks -- stylus only. See
        /// ForcePressGestureRecognizer's doc comment for why other device
        /// kinds can't support this and why it coexists with tap/pan rather
        /// than competing with them for the gesture arena.
        float force_press_start_pressure = 0.4f;
        float force_press_peak_pressure  = 0.85f;

        std::function<void(ForcePressDetails)> on_force_press_start;
        std::function<void(ForcePressDetails)> on_force_press_peak;
        std::function<void(ForcePressDetails)> on_force_press_update;
        std::function<void(ForcePressDetails)> on_force_press_end;

        /// See HitTestBehavior's doc comment (ui/hit_test.hpp). Defaults to
        /// `opaque`, matching this widget's pre-existing (only) behavior.
        HitTestBehavior                behavior = HitTestBehavior::opaque;

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
