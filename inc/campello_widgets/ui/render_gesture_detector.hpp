#pragma once

#include <cstdint>
#include <functional>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/pointer_event.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief RenderBox that recognises tap, double-tap, long-press, pan, and scroll.
     *
     * On construction, registers itself with PointerDispatcher::activeDispatcher()
     * for both pointer events and per-frame ticks (used for long-press timing).
     * On destruction, deregisters both. The active dispatcher must be set before
     * any GestureDetector widget is mounted.
     *
     * Recognised gestures:
     *  - on_tap        — down + up within kTapSlop (18 px)
     *  - on_double_tap — two qualifying taps within kDoubleTapMs (300 ms) and kTapSlop
     *  - on_long_press — finger held ≥ kLongPressMs (500 ms) without moving past kTapSlop
     *  - on_pan_update — move beyond kTapSlop while down; called with Offset delta
     *  - on_pan_end    — up/cancel after a pan
     *  - on_scroll     — scroll-wheel / trackpad scroll; called with Offset{dx, dy}
     *
     * Layout and paint pass through to the single child (inherited from RenderBox).
     */
    class RenderGestureDetector : public RenderBox
    {
    public:
        std::function<void()>           on_tap;
        std::function<void()>           on_double_tap;
        std::function<void()>           on_long_press;
        std::function<void(Offset)>     on_pan_update;
        std::function<void()>           on_pan_end;
        std::function<void(Offset)>     on_scroll;

        RenderGestureDetector();
        ~RenderGestureDetector();

    private:
        void onPointerEvent(const PointerEvent& event);
        void onTick(uint64_t now_ms);

        // Tap / double-tap state
        bool     has_down_         = false;
        bool     long_press_fired_ = false;
        bool     panning_          = false;
        Offset   down_pos_;
        Offset   last_pos_;
        uint64_t down_time_ms_     = 0;

        // Double-tap tracking
        bool     last_tap_valid_   = false;
        uint64_t last_tap_time_ms_ = 0;
        Offset   last_tap_pos_;

        static constexpr float    kTapSlop      = 18.0f;  // logical pixels
        static constexpr uint64_t kDoubleTapMs  = 300;    // milliseconds
        static constexpr uint64_t kLongPressMs  = 500;    // milliseconds
    };

} // namespace systems::leal::campello_widgets
