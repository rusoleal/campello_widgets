#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/render_sliver.hpp>
#include <campello_widgets/ui/axis.hpp>
#include <campello_widgets/ui/scroll_physics.hpp>
#include <campello_widgets/ui/gesture_arena_manager.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/velocity_tracker.hpp>

namespace systems::leal::campello_widgets
{

    class ScrollController;

    /**
     * @brief RenderBox that coordinates multiple RenderSliver children sharing
     * one scroll position — the sliver-protocol analog of RenderListView, but
     * delegating each child's own layout to the sliver protocol
     * (SliverConstraints in, SliverGeometry out) instead of BoxConstraints/Size.
     *
     * Stage 2 of the sliver-scrolling initiative: nothing concrete produces
     * real sliver content yet (RenderSliverToBoxAdapter is Stage 3), so this
     * class is verified against synthetic RenderSliver test doubles, mirroring
     * RenderSliver's own Stage 1 TestRenderSliver pattern. Hit-testing is a
     * deliberate, documented stub (see hitTestChildren()) for the same reason
     * RenderSliver itself has none yet — there is nothing to hit.
     *
     * The drag/momentum/spring-back state machine below is a deliberate 4th
     * copy of the one in RenderListView/RenderGridView/RenderSingleChildScrollView,
     * not an extraction — those three already have small, real behavioral
     * divergences between them, and reconciling those is out of scope for
     * introducing slivers. Revisit consolidation once slivers are proven in
     * production.
     */
    class RenderViewport : public RenderBox, public GestureArenaMember
    {
    public:
        Axis                            axis    = Axis::vertical;
        std::shared_ptr<ScrollPhysics>  physics = std::make_shared<ClampingScrollPhysics>();

        RenderViewport();
        ~RenderViewport();

        void attach() override;
        void detach() override;

        void setController(std::shared_ptr<ScrollController> controller);

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

        // ------------------------------------------------------------------
        // Child management — mirrors RenderStack::insertChild/clearChildren/
        // truncateChildren field-for-field, over RenderSliver instead of
        // RenderBox, including the same-box-in-place-reuse optimization (see
        // render_stack.cpp's insertChild() doc for why that distinction
        // matters).
        // ------------------------------------------------------------------

        void insertChild(std::shared_ptr<RenderSliver> sliver, int index);
        void clearChildren();
        void truncateChildren(size_t count);

        size_t        sliverCount() const noexcept { return children_.size(); }
        RenderSliver* sliverAt(size_t index) const noexcept
        {
            return index < children_.size() ? children_[index].sliver.get() : nullptr;
        }
        /** @brief This child's most recent viewport-relative layout offset (see performLayout()). */
        float layoutOffsetAt(size_t index) const noexcept
        {
            return index < children_.size() ? children_[index].layout_offset : 0.0f;
        }

        /**
         * @brief The accumulated pinned-obstruction floor (see performLayout())
         * in effect *before* this child was laid out — 0 unless an earlier
         * sibling is a pinned persistent header. Non-obstructing children are
         * clipped to this floor during paint; test/introspection accessor for
         * that mechanism, mirroring layoutOffsetAt().
         */
        float clipFloorAt(size_t index) const noexcept
        {
            return index < children_.size() ? children_[index].clip_floor : 0.0f;
        }

        /**
         * @brief Whether this child reported itself as a pinned obstruction
         * (SliverGeometry::max_scroll_obstruction_extent > 0) on its most
         * recent layout — test/introspection accessor mirroring layoutOffsetAt().
         */
        bool isPinnedObstructionAt(size_t index) const noexcept
        {
            return index < children_.size() ? children_[index].is_pinned_obstruction : false;
        }

        // ------------------------------------------------------------------
        // RenderBox overrides
        // ------------------------------------------------------------------

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;

        // RenderSliver has no hit-test contract yet (deliberately deferred in
        // Stage 1 — see render_sliver.hpp's class doc), and nothing yet
        // produces real sliver content to hit regardless (RenderSliverToBoxAdapter
        // is Stage 3). Always false: a known, documented gap for a later
        // stage, not a bug.
        bool hitTestChildren(HitTestResult&, const Offset&) override { return false; }

        // No RenderBox children exist to report through the inherited,
        // RenderBox*-typed diagnostic visitor.
        void visitRenderChildren(const std::function<void(RenderBox*)>&) const override {}

        /** @brief The real, sliver-typed child visitor. */
        void visitSliverChildren(const std::function<void(RenderSliver*)>& visitor) const;

        // Must claim hits within its own viewport (empty space past the last
        // sliver) to receive pan-to-scroll gestures there — mirrors
        // RenderListView::hitTestSelf().
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

        struct ViewportChild
        {
            std::shared_ptr<RenderSliver> sliver;
            // Viewport-relative layout offset along the scroll axis, set
            // fresh every performLayout() pass — see that method's doc.
            float layout_offset = 0.0f;
            // Pinning support (Stage 5) — both set fresh every
            // performLayout() pass, both default-inert (false/0) so every
            // pre-Stage-5 sliver is completely unaffected. See
            // performLayout()'s pin_floor bookkeeping and performPaint()'s
            // two-pass clip.
            bool  is_pinned_obstruction = false;
            float clip_floor            = 0.0f;
        };
        std::vector<ViewportChild> children_;

        std::shared_ptr<ScrollController> controller_;

        // Mirrors RenderListView's no-controller fallback exactly.
        float internal_offset_ = 0.0f;
        // See RenderListView::raw_offset_'s doc — the true, unresisted
        // cumulative scroll position; always the base for the next
        // applyScrollDelta() call, never fed a re-resisted value back in.
        float raw_offset_      = 0.0f;
        float min_extent_      = 0.0f;
        float max_extent_      = 0.0f;
        float viewport_extent_ = 0.0f;

        // Own (4th) copy of the drag/momentum/spring-back state — field-for-
        // field identical to RenderListView's own private state block (see
        // render_list_view.hpp), same constants below.
        bool   pointer_down_  = false;
        bool   panning_       = false;
        bool   won_arena_     = false;
        bool   lost_arena_    = false;
        std::optional<GestureArenaEntry> arena_entry_;
        Offset pan_last_pos_;
        Offset pan_down_pos_;
        PointerDeviceKind device_kind_ = PointerDeviceKind::touch;
        VelocityTracker velocity_tracker_;
        float  velocity_px_s_ = 0.0f;
        uint64_t last_tick_ms_= 0;

        VelocityTracker wheel_velocity_tracker_;
        bool wheel_momentum_pending_ = false;
        uint64_t last_scroll_event_ms_ = 0;

        static constexpr float    kMinVelocity              = 1.0f;
        static constexpr float    kSpringCoeff              = 20.0f;
        static constexpr float    kSignificantScrollDelta   = 8.0f;
        static constexpr float    kSpringSettleThreshold    = 0.05f;
        static constexpr uint64_t kScrollActiveWindowMs      = 40;
    };

} // namespace systems::leal::campello_widgets
