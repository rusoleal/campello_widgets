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
        std::chrono::steady_clock::time_point last_pan_time_;
        float  pan_velocity_ = 0.0f; ///< Instantaneous velocity sampled during pan (px/s).

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
        static constexpr uint64_t kScrollActiveWindowMs = 80; ///< Recency window for "scroll input still active".
    };

} // namespace systems::leal::campello_widgets
