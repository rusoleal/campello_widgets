#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/span.hpp>
#include <campello_widgets/ui/scroll_physics.hpp>

namespace systems::leal::campello_widgets
{

    class ScrollController;

    /**
     * @brief RenderBox for a two-dimensional scrollable table.
     *
     * RenderTableView manages a grid of cells that can scroll both horizontally
     * and vertically. Cells are virtualized - only visible cells are mounted.
     * Supports pinned rows and columns that remain fixed during scroll.
     *
     * The table uses a sparse cell storage model where cells are keyed by
     * their (row, column) position.
     */
    class RenderTableView : public RenderBox
    {
    public:
        /// Table dimensions.
        TableSpanExtents extents;

        /// Configuration for each row.
        std::vector<TableSpan> row_spans;

        /// Configuration for each column.
        std::vector<TableSpan> column_spans;

        RenderTableView();
        ~RenderTableView();

        void attach() override;
        void detach() override;

        /// Sets the horizontal scroll controller.
        void setHorizontalController(std::shared_ptr<ScrollController> controller);

        /// Sets the vertical scroll controller.
        void setVerticalController(std::shared_ptr<ScrollController> controller);

        /// Sets the scroll physics for both axes.
        void setPhysics(std::shared_ptr<ScrollPhysics> physics);

        // ------------------------------------------------------------------
        // Cell management - called by TableViewElement
        // ------------------------------------------------------------------

        /** @brief Attaches a render box for the given cell position. */
        void setCell(int row, int column, std::shared_ptr<RenderBox> box);

        /** @brief Detaches and discards the render box for the given cell. */
        void removeCell(int row, int column);

        // ------------------------------------------------------------------
        // Visible range queries
        // ------------------------------------------------------------------

        /// Range of visible rows and columns.
        struct VisibleRange
        {
            int first_row = 0, last_row = 0;
            int first_col = 0, last_col = 0;
        };

        /** @brief Returns the current visible range of cells. */
        VisibleRange visibleRange() const noexcept;

        /// Fired when the visible range changes. Set by TableViewElement.
        std::function<void()> on_visible_range_changed;

        // ------------------------------------------------------------------
        // RenderBox overrides
        // ------------------------------------------------------------------

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
        bool hitTestChildren(HitTestResult& result, const Offset& position) override;
        void visitRenderChildren(const std::function<void(RenderBox*)>& visitor) const override;

    private:
        void onPointerEvent(const PointerEvent& event);
        void onTick(uint64_t now_ms);

        float scrollX() const noexcept;
        float scrollY() const noexcept;
        void applyScrollDelta(float dx, float dy);
        void checkVisibleRangeChanged();

        // Layout helpers
        float computeRowOffset(int row) const;
        float computeColumnOffset(int col) const;
        float computeTotalHeight() const;
        float computeTotalWidth() const;
        int computePinnedRowCount() const;
        int computePinnedColumnCount() const;
        float computePinnedRowsHeight() const;
        float computePinnedColumnsWidth() const;

        // Cell key encoding: (row << 32) | col
        static uint64_t cellKey(int row, int col);
        static void decodeCellKey(uint64_t key, int& row, int& col);

        std::shared_ptr<ScrollController> horizontal_controller_;
        std::shared_ptr<ScrollController> vertical_controller_;
        std::shared_ptr<ScrollPhysics> physics_;

        // Internal scroll offsets when no controller is attached
        float internal_scroll_x_ = 0.0f;
        float internal_scroll_y_ = 0.0f;

        // Scroll extents
        float min_scroll_x_ = 0.0f, max_scroll_x_ = 0.0f;
        float min_scroll_y_ = 0.0f, max_scroll_y_ = 0.0f;
        float viewport_width_ = 0.0f, viewport_height_ = 0.0f;

        // Sparse cell storage
        struct CellEntry
        {
            std::shared_ptr<RenderBox> box;
            Offset offset;
        };
        std::unordered_map<uint64_t, CellEntry> cells_;

        // Cached visible range
        VisibleRange cached_range_{-1, -1, -1, -1};

        // Pan / momentum state
        bool pointer_down_ = false;
        bool panning_ = false;
        Offset pan_last_pos_{0.0f, 0.0f};
        std::chrono::steady_clock::time_point last_pan_time_;
        float pan_velocity_x_ = 0.0f, pan_velocity_y_ = 0.0f;
        float velocity_x_ = 0.0f, velocity_y_ = 0.0f;
        uint64_t last_tick_ms_ = 0;

        static constexpr float kTapSlop = 8.0f;
        static constexpr float kMinVelocity = 1.0f;
        static constexpr float kSpringCoeff = 12.0f;
    };

} // namespace systems::leal::campello_widgets
