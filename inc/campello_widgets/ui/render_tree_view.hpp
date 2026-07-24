#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/scroll_physics.hpp>
#include <campello_widgets/ui/tree_node.hpp>
#include <campello_widgets/ui/gesture_arena_manager.hpp>
#include <campello_widgets/ui/gesture_constants.hpp>

namespace systems::leal::campello_widgets
{

    class ScrollController;

    /**
     * @brief RenderBox for a TreeView with two-dimensional scrolling.
     *
     * RenderTreeView displays a tree structure where each node occupies a row.
     * The tree scrolls vertically through rows and horizontally to reveal
     * deeply nested content (based on indentation level).
     *
     * Features:
     * - Two-dimensional scrolling (vertical through rows, horizontal for depth)
     * - Lazy row building - only visible rows are mounted
     * - Configurable indentation and row height
     * - Integration with TreeController for expand/collapse state
     */
    class RenderTreeView : public RenderBox, public GestureArenaMember
    {
    public:
        /// Root node of the tree.
        std::shared_ptr<TreeNode> root;

        /// Controller managing expansion state.
        std::shared_ptr<TreeController> controller;

        /// Horizontal indentation per depth level in logical pixels.
        float indent_width = 24.0f;

        /// Fixed height for each row in logical pixels.
        float row_height = 48.0f;

        RenderTreeView();
        ~RenderTreeView();

        void attach() override;
        void detach() override;

        /// Sets the horizontal scroll controller.
        void setHorizontalController(std::shared_ptr<ScrollController> controller);

        /// Sets the vertical scroll controller.
        void setVerticalController(std::shared_ptr<ScrollController> controller);

        /// Sets the scroll physics.
        void setPhysics(std::shared_ptr<ScrollPhysics> physics);

        // ------------------------------------------------------------------
        // Row management - called by TreeViewElement
        // ------------------------------------------------------------------

        /** @brief Attaches a render box for the given row index. */
        void setRowBox(int row_index, std::shared_ptr<RenderBox> box);

        /** @brief Detaches and discards the render box for the given row. */
        void removeRowBox(int row_index);

        // ------------------------------------------------------------------
        // Visible range queries
        // ------------------------------------------------------------------

        /// Range of visible rows.
        struct VisibleRange
        {
            int first_row = 0, last_row = 0;
        };

        /** @brief Returns the current visible row range. */
        VisibleRange visibleRange() const noexcept;

        /// Information about a row needed for layout and building.
        struct RowInfo
        {
            const TreeNode* node = nullptr;
            int depth = 0;
            bool has_children = false;
            bool is_expanded = false;
        };

        /**
         * @brief Returns info about the row at the given index.
         *
         * Rebuilds the row cache if necessary.
         */
        RowInfo getRowInfo(int row_index) const;

        /** @brief Returns the total number of visible rows. */
        int computeTotalRows() const;

        /** @brief Invalidates the row cache, forcing a rebuild on next access. */
        void invalidateRowCache();

        /// Fired when the visible range changes. Set by TreeViewElement.
        std::function<void()> on_visible_range_changed;

        // ------------------------------------------------------------------
        // RenderBox overrides
        // ------------------------------------------------------------------

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
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

        float scrollX() const noexcept;
        float scrollY() const noexcept;
        void applyScrollDelta(float dx, float dy);
        void checkVisibleRangeChanged();

        // Row cache management
        void rebuildRowCache() const;
        int countVisibleRows(const TreeNode* node, int depth) const;
        void fillRowCache(const TreeNode* node, int depth) const;

        // Layout helpers
        float computeTotalWidth() const;
        float computeMaxDepth() const;

        std::shared_ptr<ScrollController> horizontal_controller_;
        std::shared_ptr<ScrollController> vertical_controller_;
        std::shared_ptr<ScrollPhysics> physics_;

        // Internal scroll offsets
        float internal_scroll_x_ = 0.0f;
        float internal_scroll_y_ = 0.0f;

        // Scroll extents
        float min_scroll_x_ = 0.0f, max_scroll_x_ = 0.0f;
        float min_scroll_y_ = 0.0f, max_scroll_y_ = 0.0f;
        float viewport_width_ = 0.0f, viewport_height_ = 0.0f;

        // Sparse row storage
        struct RowEntry
        {
            std::shared_ptr<RenderBox> box;
            Offset offset;
        };
        std::unordered_map<int, RowEntry> rows_;

        // Cached visible range
        VisibleRange cached_range_{-1, -1};

        // Flattened row cache - recomputed when tree changes
        mutable std::vector<RowInfo> row_cache_;
        mutable bool row_cache_dirty_ = true;
        mutable int cached_total_rows_ = 0;

        // Pan / momentum state
        bool pointer_down_ = false;
        bool panning_ = false;
        bool won_arena_ = false;
        bool lost_arena_ = false;
        std::optional<GestureArenaEntry> arena_entry_;
        Offset pan_last_pos_{0.0f, 0.0f};
        Offset pan_down_pos_{0.0f, 0.0f}; ///< Position at pointer-down — fixed; used for the slop check (cumulative distance), unlike pan_last_pos_ which advances every move.
        PointerDeviceKind device_kind_ = PointerDeviceKind::touch;
        std::chrono::steady_clock::time_point last_pan_time_;
        float pan_velocity_x_ = 0.0f, pan_velocity_y_ = 0.0f;
        float velocity_x_ = 0.0f, velocity_y_ = 0.0f;
        uint64_t last_tick_ms_ = 0;

        // See RenderSingleChildScrollView::last_scroll_event_ms_'s doc —
        // lets onTick() defer spring-back while the OS is still actively
        // delivering scroll events (including its own momentum tail) for
        // this gesture, instead of fighting them every tick.
        uint64_t last_scroll_event_ms_ = 0;

        static constexpr float    kMinVelocity = 1.0f;
        // See RenderListView::kSpringCoeff's doc.
        static constexpr float    kSpringCoeff = 20.0f;
        static constexpr uint64_t kScrollActiveWindowMs = 120;
    };

} // namespace systems::leal::campello_widgets
