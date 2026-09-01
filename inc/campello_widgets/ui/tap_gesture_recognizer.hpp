#pragma once

#include <functional>
#include <optional>
#include <campello_widgets/ui/gesture_recognizer.hpp>
#include <campello_widgets/ui/gesture_details.hpp>
#include <campello_widgets/ui/double_tap_gesture_recognizer.hpp>

namespace systems::leal::campello_widgets
{

    class RenderGestureDetector;

    /**
     * @brief Recognizes tap and double-tap for one PointerButton.
     *
     * Reads its owning RenderGestureDetector's callback fields live (not a
     * cached copy) at resolution time, so a widget rebuild that changes
     * those callbacks mid-gesture takes effect immediately, same as the
     * pre-decomposition monolithic implementation did.
     *
     * Which owner fields a given instance reads/fires is bound at
     * construction via pointer-to-member, not hardcoded to on_tap/
     * on_tap_down/on_tap_up/on_tap_cancel/on_double_tap -- this lets
     * RenderGestureDetector construct a second instance for
     * PointerButton::secondary (bound to on_secondary_tap*) and a third for
     * PointerButton::tertiary (bound to on_tertiary_tap*, with a null
     * on_tap_field_ since Flutter has no plain "on_tertiary_tap", and a
     * null on_double_tap_field_ since neither secondary nor tertiary
     * support double-tap) without duplicating this class. `button` events
     * for a different button are ignored by an instance entirely.
     *
     * Granular callbacks mirror Flutter's TapGestureRecognizer: the *_down
     * field fires eagerly on every qualifying pointer-down; the *_up field
     * fires alongside the plain-tap field when a pending tap resolves as a
     * *single* tap (not when it upgrades to a double-tap -- consistent with
     * on_tap's own existing double-tap suppression, see
     * DoubleTapGestureRecognizer's doc comment); the *_cancel field fires
     * whenever a pending tap is abandoned instead (moved past tolerance,
     * lost the arena, or the pointer was cancelled).
     */
    class TapGestureRecognizer : public GestureRecognizer
    {
    public:
        using VoidField    = std::function<void()> RenderGestureDetector::*;
        using TapDownField = std::function<void(TapDownDetails)> RenderGestureDetector::*;
        using TapUpField   = std::function<void(TapUpDetails)> RenderGestureDetector::*;

        /**
         * @param on_tap_field       Nullable -- null means this button has
         *                           no plain "tap" callback (tertiary).
         * @param on_double_tap_field Nullable -- null means this button
         *                           doesn't support double-tap
         *                           (secondary/tertiary); a pending tap
         *                           always resolves as single in that case.
         */
        TapGestureRecognizer(RenderGestureDetector& owner,
                              PointerButton          button,
                              VoidField               on_tap_field,
                              TapDownField            on_tap_down_field,
                              TapUpField              on_tap_up_field,
                              VoidField               on_tap_cancel_field,
                              VoidField               on_double_tap_field);

        void addPointer(const PointerEvent& down) override;
        void handlePointerEvent(const PointerEvent& event) override;

        void acceptGesture(int32_t pointer_id) override;
        void rejectGesture(int32_t pointer_id) override;

    private:
        void resolveTap();

        RenderGestureDetector& owner_;
        PointerButton           button_;

        VoidField    on_tap_field_;
        TapDownField on_tap_down_field_;
        TapUpField   on_tap_up_field_;
        VoidField    on_tap_cancel_field_;
        VoidField    on_double_tap_field_;

        std::optional<GestureArenaEntry> arena_entry_;
        bool     has_down_ = false;
        bool     won_      = false;
        bool     rejected_ = false;
        bool     pending_  = false;
        Offset   down_pos_;
        Offset   down_local_pos_;
        Offset   up_pos_;
        Offset   up_local_pos_;
        uint64_t down_time_ms_ = 0;

        DoubleTapGestureRecognizer double_tap_;
    };

} // namespace systems::leal::campello_widgets
