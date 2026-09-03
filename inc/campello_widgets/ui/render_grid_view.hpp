#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/scroll_physics.hpp>
#include <campello_widgets/ui/gesture_arena_manager.hpp>
#include <campello_widgets/ui/gesture_constants.hpp>
#include <campello_widgets/ui/offset_layer.hpp>
#include <campello_widgets/ui/velocity_tracker.hpp>

namespace systems::leal::campello_widgets
{

    class ScrollController;

    /**
     * @brief RenderBox for a virtualised, fixed-extent 2D grid.
     *
     * Self-boundaries its own paint output via `OffsetLayer` — see
     * `RenderSingleChildScrollView`'s class doc for the rationale, which
     * applies identically here.
     *
     * Items are laid out in a row-major grid scrolling vertically.
     *   - crossAxisCount items per row.
     *   - Each row is item_extent logical pixels tall.
     *   - Item width = viewport_width / crossAxisCount.
     *
     * Total content height = ceil(item_count / crossAxisCount) × item_extent.
     *
     * Virtualisation: the `on_visible_range_changed` callback is fired whenever
     * the visible row range changes so GridViewElement can mount / unmount items.
     */
    class RenderGridView : public RenderBox, public GestureArenaMember
    {
    public:
        int   item_count      = 0;

        /// Height of each row in logical pixels. Must be > 0.
        float item_extent     = 0.0f;

        /// Number of columns.
        int   cross_axis_count = 2;

        /// Fired when the visible item range changes. Set by GridViewElement.
        std::function<void()> on_visible_range_changed;

        RenderGridView();
        ~RenderGridView();

        void attach() override;
        void detach() override;

        void setController(std::shared_ptr<ScrollController> controller);
        void setPhysics(std::shared_ptr<ScrollPhysics> physics);

        // ------------------------------------------------------------------
        // Child management — called by GridViewElement
        // ------------------------------------------------------------------

        /** @brief Attaches a render box for the given item index. */
        void setItemBox(int index, std::shared_ptr<RenderBox> box);

        /** @brief Detaches and discards the render box for the given index. */
        void removeItemBox(int index);

        // ------------------------------------------------------------------
        // Visible range — queried by GridViewElement in performBuild()
        // ------------------------------------------------------------------

        /** @brief Index of the first visible item. */
        int firstVisibleIndex() const noexcept;

        /** @brief Index of the last visible item (inclusive). */
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
        // the last row) to receive pan-to-scroll gestures there — see
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
        // See RenderListView::raw_offset_'s doc — same reasoning: the true,
        // unresisted cumulative scroll position, kept separate from the
        // displayed (possibly boundary-resisted) offset above.
        float raw_offset_      = 0.0f;
        float min_extent_      = 0.0f;
        float max_extent_      = 0.0f;
        float viewport_height_ = 0.0f;
        float item_width_      = 0.0f; // computed in performLayout

        struct ChildEntry
        {
            std::shared_ptr<RenderBox> box;
            Offset                     offset;
        };

        std::unordered_map<int, ChildEntry> item_boxes_;

        int cached_first_ = -1;
        int cached_last_  = -1;

        bool   pointer_down_  = false;
        bool   panning_       = false;
        bool   won_arena_     = false;
        bool   lost_arena_    = false;
        std::optional<GestureArenaEntry> arena_entry_;
        Offset pan_last_pos_;
        Offset pan_down_pos_; ///< Position at pointer-down — fixed; used for the slop check (cumulative distance), unlike pan_last_pos_ which advances every move.
        PointerDeviceKind device_kind_ = PointerDeviceKind::touch;
        // See RenderListView::velocity_tracker_'s doc — fits a line through
        // recent samples instead of using only the last sample-to-sample delta.
        VelocityTracker velocity_tracker_;
        float  velocity_px_s_ = 0.0f;
        uint64_t last_tick_ms_= 0;

        // See RenderListView::last_pointer_timestamp_ms_'s doc.
        uint64_t last_pointer_timestamp_ms_ = 0;

        // See RenderListView::wheel_velocity_tracker_'s doc — trackpad/
        // wheel scrolling never goes through the drag path above, so this
        // tracks its own recent velocity to hand off to onTick()'s momentum
        // the instant the OS stops sending scroll events.
        VelocityTracker wheel_velocity_tracker_;
        bool wheel_momentum_pending_ = false;

        // See RenderSingleChildScrollView::last_scroll_event_ms_'s doc —
        // lets onTick() defer spring-back while the OS is still actively
        // delivering scroll events (including its own momentum tail) for
        // this gesture, instead of fighting them every tick.
        uint64_t last_scroll_event_ms_ = 0;

        static constexpr float    kMinVelocity = 1.0f;
        // See RenderListView::kSpringCoeff's doc.
        static constexpr float    kSpringCoeff = 20.0f;
        // See RenderListView::kSignificantScrollDelta's doc.
        static constexpr float    kSignificantScrollDelta = 8.0f;
        // See RenderListView::kSpringSettleThreshold's doc.
        static constexpr float    kSpringSettleThreshold = 0.05f;
        // See RenderListView::kScrollActiveWindowMs's doc.
        static constexpr uint64_t kScrollActiveWindowMs = 40;
    };

} // namespace systems::leal::campello_widgets
