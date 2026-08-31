#pragma once

#include <optional>
#include <campello_widgets/ui/gesture_recognizer.hpp>
#include <campello_widgets/ui/velocity_tracker.hpp>

namespace systems::leal::campello_widgets
{

    class RenderGestureDetector;

    /**
     * @brief Recognizes a long press: pointer held >= kLongPressMs without
     * moving past kStationaryTolerance.
     *
     * Timer-driven via handleTick() (fed by RenderGestureDetector's
     * per-frame tick registration). Explicitly claims the arena the moment
     * its deadline fires, mirroring Flutter's LongPressGestureRecognizer --
     * so a competing ancestor pan/scroll can no longer steal the gesture
     * after that point.
     *
     * Granular callbacks (owner_.on_long_press_down/cancel/start/
     * move_update/end) mirror Flutter's LongPressGestureRecognizer family:
     * on_long_press_down fires eagerly on every qualifying pointer-down;
     * on_long_press_cancel fires if a pending press is abandoned (moved
     * past tolerance, or lost the arena) before its deadline; once the
     * deadline fires (on_long_press_start, the same moment as the existing
     * on_long_press), further movement no longer threatens the gesture --
     * a long press, once recognised, isn't undone by subsequent movement --
     * it's tracked and reported via on_long_press_move_update instead; and
     * on_long_press_end fires on release, carrying the drag velocity
     * accumulated since the press started (sampled from that moment, not
     * from the original pointer-down, since movement before the press
     * starts is by definition bounded by the stationary tolerance).
     */
    class LongPressGestureRecognizer : public GestureRecognizer
    {
    public:
        explicit LongPressGestureRecognizer(RenderGestureDetector& owner);

        void addPointer(const PointerEvent& down) override;
        void handlePointerEvent(const PointerEvent& event) override;
        void handleTick(uint64_t now_ms) override;

        void acceptGesture(int32_t pointer_id) override;
        void rejectGesture(int32_t pointer_id) override;

    private:
        bool exceedsStationaryTolerance() const noexcept;

        RenderGestureDetector& owner_;

        std::optional<GestureArenaEntry> arena_entry_;
        bool     has_down_ = false;
        bool     rejected_ = false;
        bool     fired_    = false;
        Offset   down_pos_;
        Offset   last_pos_;
        Offset   down_local_pos_;
        Offset   last_local_pos_;
        uint64_t down_time_ms_ = 0;

        VelocityTracker vx_;
        VelocityTracker vy_;
    };

} // namespace systems::leal::campello_widgets
