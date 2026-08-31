#include <campello_widgets/diagnostics/diagnostic_property.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/ui/render_gesture_detector.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> GestureDetector::createRenderObject() const
    {
        auto ro           = std::make_shared<RenderGestureDetector>();
        ro->on_tap         = on_tap;
        ro->on_double_tap  = on_double_tap;
        ro->on_long_press  = on_long_press;

        ro->on_tap_down    = on_tap_down;
        ro->on_tap_up      = on_tap_up;
        ro->on_tap_cancel  = on_tap_cancel;

        ro->on_secondary_tap        = on_secondary_tap;
        ro->on_secondary_tap_down   = on_secondary_tap_down;
        ro->on_secondary_tap_up     = on_secondary_tap_up;
        ro->on_secondary_tap_cancel = on_secondary_tap_cancel;

        ro->on_tertiary_tap_down   = on_tertiary_tap_down;
        ro->on_tertiary_tap_up     = on_tertiary_tap_up;
        ro->on_tertiary_tap_cancel = on_tertiary_tap_cancel;

        ro->on_long_press_down        = on_long_press_down;
        ro->on_long_press_cancel      = on_long_press_cancel;
        ro->on_long_press_start       = on_long_press_start;
        ro->on_long_press_move_update = on_long_press_move_update;
        ro->on_long_press_end         = on_long_press_end;

        ro->on_pan_down    = on_pan_down;
        ro->on_pan_start   = on_pan_start;
        ro->on_pan_update  = on_pan_update;
        ro->on_pan_end     = on_pan_end;

        ro->on_horizontal_drag_down   = on_horizontal_drag_down;
        ro->on_horizontal_drag_start  = on_horizontal_drag_start;
        ro->on_horizontal_drag_update = on_horizontal_drag_update;
        ro->on_horizontal_drag_end    = on_horizontal_drag_end;

        ro->on_vertical_drag_down   = on_vertical_drag_down;
        ro->on_vertical_drag_start  = on_vertical_drag_start;
        ro->on_vertical_drag_update = on_vertical_drag_update;
        ro->on_vertical_drag_end    = on_vertical_drag_end;

        ro->drag_start_behavior = drag_start_behavior;

        ro->on_scale_start  = on_scale_start;
        ro->on_scale_update = on_scale_update;
        ro->on_scale_end    = on_scale_end;

        ro->force_press_start_pressure = force_press_start_pressure;
        ro->force_press_peak_pressure  = force_press_peak_pressure;
        ro->on_force_press_start       = on_force_press_start;
        ro->on_force_press_peak        = on_force_press_peak;
        ro->on_force_press_update      = on_force_press_update;
        ro->on_force_press_end         = on_force_press_end;

        ro->behavior            = behavior;

        ro->on_scroll      = on_scroll;
        ro->on_press_change = on_press_change;
        ro->on_focus_change = on_focus_change;
        ro->setFocusConfig(focus_node, autofocus, focusable);
        return ro;
    }

    void GestureDetector::updateRenderObject(RenderObject& render_object) const
    {
        auto& ro           = static_cast<RenderGestureDetector&>(render_object);
        ro.on_tap          = on_tap;
        ro.on_double_tap   = on_double_tap;
        ro.on_long_press   = on_long_press;

        ro.on_tap_down     = on_tap_down;
        ro.on_tap_up       = on_tap_up;
        ro.on_tap_cancel   = on_tap_cancel;

        ro.on_secondary_tap        = on_secondary_tap;
        ro.on_secondary_tap_down   = on_secondary_tap_down;
        ro.on_secondary_tap_up     = on_secondary_tap_up;
        ro.on_secondary_tap_cancel = on_secondary_tap_cancel;

        ro.on_tertiary_tap_down   = on_tertiary_tap_down;
        ro.on_tertiary_tap_up     = on_tertiary_tap_up;
        ro.on_tertiary_tap_cancel = on_tertiary_tap_cancel;

        ro.on_long_press_down        = on_long_press_down;
        ro.on_long_press_cancel      = on_long_press_cancel;
        ro.on_long_press_start       = on_long_press_start;
        ro.on_long_press_move_update = on_long_press_move_update;
        ro.on_long_press_end         = on_long_press_end;

        ro.on_pan_down     = on_pan_down;
        ro.on_pan_start    = on_pan_start;
        ro.on_pan_update   = on_pan_update;
        ro.on_pan_end      = on_pan_end;

        ro.on_horizontal_drag_down   = on_horizontal_drag_down;
        ro.on_horizontal_drag_start  = on_horizontal_drag_start;
        ro.on_horizontal_drag_update = on_horizontal_drag_update;
        ro.on_horizontal_drag_end    = on_horizontal_drag_end;

        ro.on_vertical_drag_down   = on_vertical_drag_down;
        ro.on_vertical_drag_start  = on_vertical_drag_start;
        ro.on_vertical_drag_update = on_vertical_drag_update;
        ro.on_vertical_drag_end    = on_vertical_drag_end;

        ro.drag_start_behavior = drag_start_behavior;

        ro.on_scale_start  = on_scale_start;
        ro.on_scale_update = on_scale_update;
        ro.on_scale_end    = on_scale_end;

        ro.force_press_start_pressure = force_press_start_pressure;
        ro.force_press_peak_pressure  = force_press_peak_pressure;
        ro.on_force_press_start       = on_force_press_start;
        ro.on_force_press_peak        = on_force_press_peak;
        ro.on_force_press_update      = on_force_press_update;
        ro.on_force_press_end         = on_force_press_end;

        ro.behavior            = behavior;

        ro.on_scroll       = on_scroll;
        ro.on_press_change = on_press_change;
        ro.on_focus_change = on_focus_change;
        ro.setFocusConfig(focus_node, autofocus, focusable);
    }


    void GestureDetector::debugFillProperties(DiagnosticsPropertyBuilder& properties) const
    {
        properties.add(std::make_unique<FlagProperty>("onTap", on_tap != nullptr, "tap enabled", "tap disabled"));
    }
} // namespace systems::leal::campello_widgets
