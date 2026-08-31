#include <campello_widgets/ui/force_press_gesture_recognizer.hpp>
#include <campello_widgets/ui/render_gesture_detector.hpp>

namespace systems::leal::campello_widgets
{

    ForcePressGestureRecognizer::ForcePressGestureRecognizer(RenderGestureDetector& owner)
        : owner_(owner)
    {
    }

    void ForcePressGestureRecognizer::addPointer(const PointerEvent& down)
    {
        // See the class doc comment -- deliberately does not join the
        // gesture arena, and only ever arms for a real staged-pressure
        // device.
        if (down.device_kind != PointerDeviceKind::stylus)
        {
            tracking_ = false;
            return;
        }

        tracking_        = true;
        started_         = false;
        peaked_          = false;
        global_position_ = down.position;
        local_position_  = down.local_position;

        updatePressure(down);
    }

    void ForcePressGestureRecognizer::handlePointerEvent(const PointerEvent& event)
    {
        if (!tracking_) return;

        switch (event.kind)
        {
        case PointerEventKind::move:
            global_position_ = event.position;
            local_position_  = event.local_position;
            updatePressure(event);
            break;
        case PointerEventKind::up:
        case PointerEventKind::cancel:
            if (started_ && owner_.on_force_press_end)
                owner_.on_force_press_end(ForcePressDetails{global_position_, local_position_, event.pressure});
            tracking_ = false;
            started_  = false;
            peaked_   = false;
            break;
        default:
            break;
        }
    }

    void ForcePressGestureRecognizer::updatePressure(const PointerEvent& event)
    {
        const ForcePressDetails details{global_position_, local_position_, event.pressure};
        bool just_started = false;

        if (!started_ && event.pressure >= owner_.force_press_start_pressure)
        {
            started_     = true;
            just_started = true;
            if (owner_.on_force_press_start) owner_.on_force_press_start(details);
        }

        if (started_ && !peaked_ && event.pressure >= owner_.force_press_peak_pressure)
        {
            peaked_ = true;
            if (owner_.on_force_press_peak) owner_.on_force_press_peak(details);
        }

        // Deliberately excludes the sample that just fired on_start -- an
        // update reports a *subsequent* pressure change, not a restatement
        // of the value that just triggered start.
        if (started_ && !just_started && owner_.on_force_press_update)
            owner_.on_force_press_update(details);
    }

    void ForcePressGestureRecognizer::acceptGesture(int32_t /*pointer_id*/)
    {
        // Never called -- this recognizer intentionally never joins the
        // arena. See the class doc comment.
    }

    void ForcePressGestureRecognizer::rejectGesture(int32_t /*pointer_id*/)
    {
        // Never called -- see acceptGesture().
    }

} // namespace systems::leal::campello_widgets
