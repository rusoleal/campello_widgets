#pragma once

#include <cstdint>
#include <campello_widgets/ui/offset.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Double-tap detection helper, owned internally by
     * TapGestureRecognizer.
     *
     * Deliberately NOT an independent GestureArenaMember/GestureRecognizer:
     * this codebase's double-tap semantics fire a single tap immediately on
     * every qualifying up (no delay holding the arena open to see if a
     * second tap follows, unlike Flutter's real DoubleTapGestureRecognizer)
     * and retroactively upgrade the *current* tap to a double-tap when it
     * lands within kDoubleTapMs and kStationaryTolerance of the *previous*
     * completed tap. TapGestureRecognizer calls checkAndConsume() every
     * time it resolves a tap.
     */
    class DoubleTapGestureRecognizer
    {
    public:
        /**
         * @brief Records/evaluates one completed tap.
         *
         * @param has_handler True if a double-tap callback is actually
         *                    configured -- with none set, every tap is
         *                    just a single tap (mirrors Flutter never
         *                    arming a DoubleTapGestureRecognizer at all
         *                    when onDoubleTap isn't supplied).
         * @param tap_time_ms Timestamp of this tap (pointer-down time, ms).
         * @param tap_pos     Position of this tap (pointer-down position).
         * @return true if this tap completed a double-tap -- caller should
         *         fire its double-tap callback and NOT its single-tap one;
         *         false for an ordinary single tap.
         */
        bool checkAndConsume(bool has_handler, uint64_t tap_time_ms, Offset tap_pos) noexcept;

    private:
        bool     last_tap_valid_   = false;
        uint64_t last_tap_time_ms_ = 0;
        Offset   last_tap_pos_;
    };

} // namespace systems::leal::campello_widgets
