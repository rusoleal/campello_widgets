#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/axis.hpp>
#include <campello_widgets/ui/scroll_physics.hpp>

namespace systems::leal::campello_widgets
{

    class ScrollController;

    /**
     * @brief RenderBox for a virtualised, fixed-extent list.
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
    class RenderListView : public RenderBox
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

        void setController(std::shared_ptr<ScrollController> controller);
        void setPhysics(std::shared_ptr<ScrollPhysics> physics);

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
        bool hitTestChildren(HitTestResult& result, const Offset& position) override;

    private:
        void onPointerEvent(const PointerEvent& event);
        void onTick(uint64_t now_ms);

        float scrollOffset() const noexcept;
        void  applyScrollDelta(float delta);
        void  checkVisibleRangeChanged();

        std::shared_ptr<ScrollController> controller_;
        std::shared_ptr<ScrollPhysics>    physics_;

        float internal_offset_ = 0.0f;
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
        bool   panning_       = false;
        Offset pan_last_pos_;
        std::chrono::steady_clock::time_point last_pan_time_;
        float  pan_velocity_  = 0.0f;
        float  velocity_px_s_ = 0.0f;
        uint64_t last_tick_ms_= 0;

        static constexpr float kTapSlop     = 8.0f;
        static constexpr float kMinVelocity = 1.0f;
        static constexpr float kSpringCoeff = 12.0f;
    };

} // namespace systems::leal::campello_widgets
