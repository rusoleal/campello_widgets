#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/gesture_arena_manager.hpp>
#include <campello_widgets/ui/gesture_constants.hpp>

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
     * Slop thresholds are device-aware (see gesture_constants.hpp), mirroring
     * Flutter: touch input tolerates more incidental movement than a mouse.
     * Tap/double-tap/long-press use the smaller hit slop; pan-start
     * disambiguation (exceedsSlop()) uses the larger pan slop when this
     * detector actually has pan handlers registered — mirroring Flutter's
     * GestureDetector internally choosing a PanGestureRecognizer (pan slop)
     * vs Tap/LongPressGestureRecognizer (hit slop) depending on which
     * callbacks are set.
     *
     * Recognised gestures:
     *  - on_tap        — down + up within hit slop
     *  - on_double_tap — two qualifying taps within kDoubleTapMs (300 ms) and hit slop
     *  - on_long_press — finger held ≥ kLongPressMs (500 ms) without moving past hit slop
     *  - on_pan_down   — pointer down, fired immediately (before the slop gate);
     *                    called with the box-local position, akin to Flutter's
     *                    GestureDetector.onPanDown(DragDownDetails)
     *  - on_pan_update — move beyond pan slop while down; called with Offset delta
     *  - on_pan_end    — up/cancel after a pan
     *  - on_scroll     — scroll-wheel / trackpad scroll; called with Offset{dx, dy}
     *
     * Layout and paint pass through to the single child (inherited from RenderBox).
     */
    class RenderGestureDetector : public RenderBox, public GestureArenaMember
    {
    public:
        std::function<void()>           on_tap;
        std::function<void()>           on_double_tap;
        std::function<void()>           on_long_press;
        std::function<void(Offset)>     on_pan_down;
        std::function<void(Offset)>     on_pan_update;
        std::function<void()>           on_pan_end;
        std::function<void(Offset)>     on_scroll;

        RenderGestureDetector();
        ~RenderGestureDetector();

        void attach() override;
        void detach() override;

        // ------------------------------------------------------------------
        // GestureArenaMember
        // ------------------------------------------------------------------

        void acceptGesture(int32_t pointer_id) override;
        void rejectGesture(int32_t pointer_id) override;

        // Transparent layout: size to child without loosening constraints.
        void performLayout() override;
        void performPaint(PaintContext& ctx, const Offset& offset) override;

        // Claims hits within its own bounds even where no descendant did
        // (e.g. a "tap the whole row" button whose visible content is
        // smaller than the tappable area) — see RenderBox::hitTestSelf().
        bool hitTestSelf(const Offset&) const override { return true; }

        /**
         * @brief This box's on-screen position as of the last paint.
         *
         * Lets a tap callback (e.g. DropdownButton opening an anchored
         * overlay menu) find out where it is on screen without a general
         * localToGlobal() facility — mirrors the same pattern used by
         * RenderDraggable for its feedback-anchor offset.
         */
        Offset globalOffset() const noexcept { return global_offset_; }

    private:
        void onPointerEvent(const PointerEvent& event);
        void onTick(uint64_t now_ms);

        bool exceedsSlop() const noexcept;
        bool exceedsStationaryTolerance() const noexcept;
        void resolveTapOutcome();

        Offset   global_offset_;

        // Tap / double-tap state
        bool     has_down_         = false;
        bool     long_press_fired_ = false;
        bool     panning_          = false;
        bool     won_arena_        = false;
        bool     lost_arena_       = false;
        bool     pending_tap_      = false;
        std::optional<GestureArenaEntry> arena_entry_;
        Offset   down_pos_;
        Offset   last_pos_;
        uint64_t down_time_ms_     = 0;
        PointerDeviceKind device_kind_ = PointerDeviceKind::touch;

        // Double-tap tracking
        bool     last_tap_valid_   = false;
        uint64_t last_tap_time_ms_ = 0;
        Offset   last_tap_pos_;

        static constexpr uint64_t kDoubleTapMs  = 300;    // milliseconds
        static constexpr uint64_t kLongPressMs  = 500;    // milliseconds
    };

} // namespace systems::leal::campello_widgets
