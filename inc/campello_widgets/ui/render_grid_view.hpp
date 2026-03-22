#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/scroll_physics.hpp>

namespace systems::leal::campello_widgets
{

    class ScrollController;

    /**
     * @brief RenderBox for a virtualised, fixed-extent 2D grid.
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
    class RenderGridView : public RenderBox
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
