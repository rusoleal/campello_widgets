#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
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
     * @brief RenderBox for a virtualised, fixed-extent list.
     *
     * Self-boundaries its own paint output via `OffsetLayer` — see
     * `RenderSingleChildScrollView`'s class doc for the rationale, which
     * applies identically here.
     *
     * Content model: item_count items each occupying item_extent logical pixels
     * on the scroll axis (height for vertical, width for horizontal). Total
     * content extent = item_count × item_extent.
     *
     * Virtualisation: only items currently in the visible viewport (plus one
     * buffer item on each side) are stored as child RenderBoxes. The
     * `on_visible_range_changed` callback is fired whenever scrolling causes the
     * set of visible items to change, so the owning ListViewElement can
     * mount / unmount items accordingly.
     *
     * Input: registers with the active PointerDispatcher for pan and scroll-wheel
     * events. Momentum is tick-driven, same as RenderSingleChildScrollView.
     */
    class RenderListView : public RenderBox, public GestureArenaMember
    {
    public:
        Axis  scroll_axis = Axis::vertical;
        int   item_count  = 0;

        /// Fixed size on the scroll axis per item (height for vertical lists,
        /// width for horizontal lists). Must be > 0 for virtualisation to work.
        float item_extent = 0.0f;

        /// Fired when the visible item range changes. Set by ListViewElement.
        std::function<void()> on_visible_range_changed;

        RenderListView();
        ~RenderListView();

        void attach() override;
        void detach() override;

        void setController(std::shared_ptr<ScrollController> controller);
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

        // ------------------------------------------------------------------
        // Child management — called by ListViewElement
        // ------------------------------------------------------------------

        /** @brief Attaches a render box for the given item index. */
        void setItemBox(int index, std::shared_ptr<RenderBox> box);

        /** @brief Detaches and discards the render box for the given index. */
        void removeItemBox(int index);

        // ------------------------------------------------------------------
        // Visible range — queried by ListViewElement in performBuild()
        // ------------------------------------------------------------------

        /** @brief Index of the first (partially) visible item. */
        int firstVisibleIndex() const noexcept;

        /** @brief Index of the last (partially) visible item (inclusive). */
        int lastVisibleIndex() const noexcept;

        // ------------------------------------------------------------------
        // RenderBox overrides
        // ------------------------------------------------------------------

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
        void paint(PaintContext& context, const Offset& offset) override;
        bool isRepaintBoundary() const noexcept override { return true; }
        bool hitTestChildren(HitTestResult& result, const Offset& position) override;
        void visitRenderChildren(const std::function<void(RenderBox*)>& visitor) const override;

        // Must claim hits within its own viewport (e.g. empty space below
        // the last item) to receive pan-to-scroll gestures there — see
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
        void  checkVisibleRangeChanged();

        std::shared_ptr<ScrollController> controller_;
        std::shared_ptr<ScrollPhysics>    physics_;
        OffsetLayer                       offset_layer_;

        float internal_offset_ = 0.0f;
        // True, unresisted cumulative scroll position — always the base
        // for the next applyScrollDelta() call. Kept separate from the
        // displayed (possibly boundary-resisted) offset above: feeding an
        // already-resisted value back through
        // ScrollPhysics::applyBoundaryConditions() repeatedly compresses
        // it further each call rather than reflecting the true drag
        // distance past the boundary, which snaps the displayed position
        // back toward the boundary whenever the delta shrinks — even
        // mid-drag, same direction. See applyScrollDelta()'s doc.
        float raw_offset_      = 0.0f;
        float min_extent_      = 0.0f;
        float max_extent_      = 0.0f;
        float viewport_extent_ = 0.0f;

        struct ChildEntry
        {
            std::shared_ptr<RenderBox> box;
            Offset                     offset;
        };

        // Sparse: only visible items are present.
        std::unordered_map<int, ChildEntry> item_boxes_;

        // Cached visible range — used to detect changes.
        int cached_first_ = -1;
        int cached_last_  = -1;

        // Pan / momentum state.
        bool   pointer_down_  = false;
        bool   panning_       = false;
        bool   won_arena_     = false;
        bool   lost_arena_    = false;
        std::optional<GestureArenaEntry> arena_entry_;
        Offset pan_last_pos_;
        Offset pan_down_pos_; ///< Position at pointer-down — fixed; used for the slop check (cumulative distance), unlike pan_last_pos_ which advances every move.
        PointerDeviceKind device_kind_ = PointerDeviceKind::touch;
        // Fits a line through recent (time, position) samples instead of
        // using only the delta between the last two — see its doc comment
        // for why a single-sample estimate is unreliable for release
        // velocity specifically.
        VelocityTracker velocity_tracker_;
        float  velocity_px_s_ = 0.0f;
        uint64_t last_tick_ms_= 0;

        // Trackpad/wheel scrolling never goes through onPointerEvent's
        // down/move/up drag path above, so velocity_tracker_ never sees it.
        // This tracks the wheel's own recent delivered velocity instead, so
        // onTick() can hand off to our own momentum simulation the instant
        // the OS stops sending scroll events — seeing this hand-off through
        // is what actually produces a felt "coast," rather than the list's
        // motion being entirely at the mercy of however long (or short) a
        // kinetic tail the platform's own trackpad driver happens to send.
        VelocityTracker wheel_velocity_tracker_;
        // Set on each "significant" wheel event (see onPointerEvent's
        // scroll case); consumed exactly once by onTick() at the tick where
        // the active-scroll-window gate first closes, to seed velocity_px_s_
        // from wheel_velocity_tracker_ before momentum decay takes over.
        bool wheel_momentum_pending_ = false;

        // See RenderSingleChildScrollView::last_scroll_event_ms_'s doc —
        // lets onTick() defer spring-back while the OS is still actively
        // delivering scroll events (including its own momentum tail) for
        // this gesture, instead of fighting them every tick.
        uint64_t last_scroll_event_ms_ = 0;

        static constexpr float    kMinVelocity = 1.0f;
        // 20.0 matches the snappier feel used elsewhere in this codebase
        // (RenderPageView's page-settle spring) and is closer to iOS
        // UIScrollView's bounce timing than the original 12.0, which felt
        // noticeably slower to settle.
        static constexpr float    kSpringCoeff = 20.0f;
        // See onPointerEvent()'s scroll case doc — only a delta at least
        // this large (px) refreshes last_scroll_event_ms_'s "still
        // actively scrolling" gate. Raised from 2.0 — a firm trackpad
        // swipe's OS-generated momentum tail decays gradually over many
        // events, and a lot of that tail was still >2px per event, so
        // spring-back kept waiting on it far longer than felt natural.
        static constexpr float    kSignificantScrollDelta = 8.0f;
        // Below this many px of remaining overscroll, snap to the boundary
        // exactly instead of continuing to ease — exponential decay
        // asymptotically approaches but never exactly reaches zero, so
        // without a settle threshold the spring branch (and the frame it
        // requests) would keep re-triggering forever, imperceptibly, after
        // every bounce.
        static constexpr float    kSpringSettleThreshold = 0.05f;
        // How long to wait, after the last "significant" wheel event,
        // before treating the OS as done delivering scroll for this
        // gesture (both for letting spring-back take over, and for handing
        // off to our own momentum — see onTick()'s doc). During genuinely
        // active scrolling, events arrive roughly once per tick (~16-20ms
        // at 60Hz) — 40ms comfortably outlasts that cadence's jitter
        // without (as an earlier, more conservative 80ms did) leaving a
        // visible ~5-frame dead zone where the list sits frozen before our
        // momentum handoff engages.
        static constexpr uint64_t kScrollActiveWindowMs = 40;
    };

} // namespace systems::leal::campello_widgets
