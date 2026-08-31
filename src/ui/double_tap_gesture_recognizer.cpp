#include <cmath>
#include <campello_widgets/ui/double_tap_gesture_recognizer.hpp>
#include <campello_widgets/ui/gesture_constants.hpp>

namespace systems::leal::campello_widgets
{

    bool DoubleTapGestureRecognizer::checkAndConsume(bool has_handler, uint64_t tap_time_ms, Offset tap_pos) noexcept
    {
        if (has_handler && last_tap_valid_ && tap_time_ms != 0 &&
            (tap_time_ms - last_tap_time_ms_) <= kDoubleTapMs)
        {
            const float dx   = tap_pos.x - last_tap_pos_.x;
            const float dy   = tap_pos.y - last_tap_pos_.y;
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < kStationaryTolerance)
            {
                last_tap_valid_ = false;
                return true;
            }
        }

        // Not a double-tap (or nothing listening for one) -- record this
        // tap so a *subsequent* tap can be checked against it.
        last_tap_valid_   = true;
        last_tap_time_ms_ = tap_time_ms;
        last_tap_pos_     = tap_pos;
        return false;
    }

} // namespace systems::leal::campello_widgets
