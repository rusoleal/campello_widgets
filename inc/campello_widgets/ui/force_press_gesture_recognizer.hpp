#pragma once

#include <campello_widgets/ui/gesture_recognizer.hpp>
#include <campello_widgets/ui/gesture_details.hpp>

namespace systems::leal::campello_widgets
{

    class RenderGestureDetector;

    /**
     * @brief Recognizes staged force-press for a stylus pointer (start/peak/update/end).
     *
     * Deliberately never joins the gesture arena. Flutter's real
     * ForcePressGestureRecognizer self-accepts immediately once armed, since
     * force-press isn't meant to be arbitrated against tap/drag -- but this
     * codebase's GestureArenaManager treats an explicit accept as winner-
     * take-all (see gesture_arena_manager.hpp's resolve()/
     * resolveInFavorOf() doc comments): it rejects every other member of
     * that pointer's arena. Self-accepting here would silently steal the
     * win from a co-configured TapGestureRecognizer or DragGestureRecognizer
     * on every stylus press, breaking the "force-press coexists with tap/
     * drag" contract. Observing the raw pointer stream passively achieves
     * the same "doesn't need arbitration" outcome without that side effect.
     *
     * Only ever activates for PointerDeviceKind::stylus -- every other
     * device kind in this codebase reports a constant pressure = 1.0
     * (mouse) with no staging, which would fire start+peak instantly on
     * every down and produce meaningless callbacks. See ForcePressDetails's
     * doc comment (gesture_details.hpp).
     *
     * Thresholds are read live from the owner's force_press_start_pressure/
     * force_press_peak_pressure fields (same "read the owner live" pattern
     * as every other recognizer's callbacks), rather than cached at
     * construction, so they can be reconfigured on a widget rebuild.
     */
    class ForcePressGestureRecognizer : public GestureRecognizer
    {
    public:
        explicit ForcePressGestureRecognizer(RenderGestureDetector& owner);

        void addPointer(const PointerEvent& down) override;
        void handlePointerEvent(const PointerEvent& event) override;

        void acceptGesture(int32_t pointer_id) override;
        void rejectGesture(int32_t pointer_id) override;

    private:
        void updatePressure(const PointerEvent& event);

        RenderGestureDetector& owner_;

        bool   tracking_ = false; // a stylus pointer is currently down
        bool   started_  = false;
        bool   peaked_   = false;
        Offset global_position_;
        Offset local_position_;
    };

} // namespace systems::leal::campello_widgets
