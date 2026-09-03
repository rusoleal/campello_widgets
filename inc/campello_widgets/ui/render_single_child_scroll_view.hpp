#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/axis.hpp>
#include <campello_widgets/ui/scroll_physics.hpp>
#include <campello_widgets/ui/gesture_arena_manager.hpp>
#include <campello_widgets/ui/gesture_constants.hpp>
#include <campello_widgets/ui/offset_layer.hpp>
#include <campello_widgets/ui/velocity_tracker.hpp>

namespace systems::leal::campello_widgets
{

    class ScrollController;

    /**
     * @brief RenderBox that clips and scrolls a single child along one axis.
     *
     * Layout: the child receives an unconstrained constraint on the scroll axis
     * so it can be as large as its content requires. The cross axis is tight to
     * the viewport size.
     *
     * Paint: the child is clipped to the viewport rectangle and translated by
     * the negative scroll offset so only the visible portion appears.
     *
     * Self-boundaries its own paint output via `OffsetLayer`, mirroring
     * Flutter's `RenderViewport.isRepaintBoundary` — an unrelated repaint
     * elsewhere in the tree doesn't force this (potentially large) scrolled
     * content to be re-walked, and vice versa. Scrolling itself still
     * re-records every frame the offset changes (it calls `markNeedsPaint()`
     * — see `applyScrollDelta()`), so this doesn't make scrolling itself
     * cheaper; it isolates everything *else* from scrolling's cost.
     *
     * Input: registers with the active PointerDispatcher to receive pan and
     * scroll-wheel events. Pan releases initiate momentum that is decayed each
     * tick by the active ScrollPhysics.
     */
    class RenderSingleChildScrollView : public RenderBox, public GestureArenaMember
    {
    public:
        Axis scroll_axis = Axis::vertical;

        RenderSingleChildScrollView();
        ~RenderSingleChildScrollView();

        void attach() override;
        void detach() override;

        /** @brief Wires an optional ScrollController for programmatic control. */
        void setController(std::shared_ptr<ScrollController> controller);

        /** @brief Replaces the active scroll physics (defaults to ClampingScrollPhysics). */
        void setPhysics(std::shared_ptr<ScrollPhysics> physics);

        /**
         * @brief Applies a scroll delta from an external coordinator (a future
         * NestedScrollView), through this view's own physics, exactly as if it
         * came from this view's own pointer/wheel input -- but without touching
         * this view's own gesture/velocity state, since the delta already came
         * from somewhere else's gesture.
         *
         * @return The amount actually absorbed (post-physics) -- may be less than
         * `delta` at a hard clamping boundary, or the delta minus rubber-band
         * resistance under bouncing physics. Never assume it equals `delta`; a
         * coordinator uses the difference to redistribute the remainder to
         * another position.
         */
        float applyExternalScrollDelta(float delta);

        /**
         * @brief When set, a genuinely user-driven scroll delta (drag or
         * wheel -- never onTick()'s own spring-back/momentum, which stays
         * local to this view even when coordinated) is handed to this
         * callback instead of being applied directly via this view's own
         * applyScrollDelta(). Set by a NestedScrollCoordinator on both the
         * outer and inner participants it coordinates; unset (nullptr)
         * preserves this view's exact standalone behavior.
         */
        std::function<void(float)> external_delta_redirect;

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
        void paint(PaintContext& context, const Offset& offset) override;
        bool isRepaintBoundary() const noexcept override { return true; }
        bool hitTestChildren(HitTestResult& result, const Offset& position) override;

        // Must claim hits within its own viewport (e.g. empty space below
        // short content) to receive pan-to-scroll gestures there — see
        // RenderBox::hitTestSelf().
        bool hitTestSelf(const Offset&) const override { return true; }

        // ------------------------------------------------------------------
        // GestureArenaMember
        // ------------------------------------------------------------------

        void acceptGesture(int32_t pointer_id) override;
        void rejectGesture(int32_t pointer_id) override;

    private:
        void onPointerEvent(const PointerEvent& event);
        void onTick(uint64_t now_ms);

        float scrollOffset() const noexcept;
        void  applyScrollDelta(float delta, const char* source = "drag");

        std::shared_ptr<ScrollController> controller_;
        std::shared_ptr<ScrollPhysics>    physics_;
        OffsetLayer                       offset_layer_;

        // Internal offset used when no controller is attached.
        float internal_offset_ = 0.0f;

        // See RenderListView::raw_offset_'s doc — same reasoning: the true,
        // unresisted cumulative scroll position, kept separate from the
        // displayed (possibly boundary-resisted) offset above.
        float raw_offset_ = 0.0f;

        // Scroll extents (updated after each layout).
        float min_extent_ = 0.0f;
        float max_extent_ = 0.0f;

        // Pan gesture state.
        bool   pointer_down_ = false;
        bool   panning_      = false;
        bool   won_arena_    = false;
        bool   lost_arena_   = false;
        std::optional<GestureArenaEntry> arena_entry_;
        Offset pan_last_pos_;
        Offset pan_down_pos_; ///< Position at pointer-down — fixed; used for the slop check (cumulative distance), unlike pan_last_pos_ which advances every move.
        PointerDeviceKind device_kind_ = PointerDeviceKind::touch;
        // See RenderListView::velocity_tracker_'s doc — fits a line through
        // recent samples instead of using only the last sample-to-sample delta.
        VelocityTracker velocity_tracker_;

        // See RenderListView::last_pointer_timestamp_ms_'s doc.
        uint64_t last_pointer_timestamp_ms_ = 0;

        // See RenderListView::wheel_velocity_tracker_'s doc — trackpad/
        // wheel scrolling never goes through the drag path above, so this
        // tracks its own recent velocity to hand off to onTick()'s momentum
        // the instant the OS stops sending scroll events.
        VelocityTracker wheel_velocity_tracker_;
        bool wheel_momentum_pending_ = false;

        // Momentum simulation.
        float    velocity_px_s_ = 0.0f;
        uint64_t last_tick_ms_  = 0;

        // Wall-clock time (steady_clock epoch ms) of the last
        // PointerEventKind::scroll event — lets onTick() tell "the OS is
        // still actively delivering trackpad/wheel scroll events for this
        // gesture" apart from "input has stopped, settle it ourselves."
        // Without this, spring-back would start easing an overscroll back
        // toward the edge on the very next tick, fighting the OS's own
        // momentum tail (which keeps calling applyScrollDelta() directly,
        // pushing further out) — felt as sustained vibration rather than
        // a clean settle once the user's input actually stops. Mirrors
        // how `panning_` already gates spring-back during an active
        // click-drag; scroll events have no equivalent "still active"
        // flag of their own, so a recency check stands in for one.
        uint64_t last_scroll_event_ms_ = 0;

        static constexpr float    kMinVelocity = 1.0f;  ///< px/s below which momentum stops.
        /// See RenderListView::kSpringCoeff's doc.
        static constexpr float    kSpringCoeff = 20.0f;
        /// See RenderListView::kSignificantScrollDelta's doc.
        static constexpr float    kSignificantScrollDelta = 8.0f;
        /// See RenderListView::kSpringSettleThreshold's doc.
        static constexpr float    kSpringSettleThreshold = 0.05f;
        /// See RenderListView::kScrollActiveWindowMs's doc.
        static constexpr uint64_t kScrollActiveWindowMs = 40; ///< Recency window for "scroll input still active".
    };

} // namespace systems::leal::campello_widgets
