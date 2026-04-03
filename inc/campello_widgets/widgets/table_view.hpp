#pragma once

#include <functional>
#include <memory>
#include <vector>
#include <campello_widgets/widgets/render_object_widget.hpp>
#include <campello_widgets/ui/span.hpp>
#include <campello_widgets/ui/scroll_physics.hpp>

namespace systems::leal::campello_widgets
{

    class BuildContext;
    class ScrollController;

    /**
     * @brief Builder function type for table cells.
     *
     * Called by TableViewElement to build the widget for cell at (row, column).
     */
    using TableCellBuilder = std::function<WidgetRef(BuildContext&, int row, int col)>;

    /**
     * @brief A scrollable table with rows and columns supporting two-dimensional
     * scrolling, pinned rows/columns, and lazy cell building.
     *
     * TableView only mounts elements for cells currently visible in the viewport
     * (plus a small buffer). Cells that scroll off screen are unmounted and
     * their resources released.
     *
     * Features:
     * - Bidirectional scrolling (horizontal + vertical)
     * - Pinned rows/columns that remain visible during scroll
     * - Configurable row heights and column widths
     * - Lazy building via cell_builder callback
     *
     * Usage:
     * @code
     * auto table = std::make_shared<TableView>();
     * table->extents = {100, 26};  // 100 rows, 26 columns
     *
     * // Configure rows
     * for (int i = 0; i < 100; i++) {
     *     table->row_spans.push_back({.extent = 48.0f, .pinned = (i == 0)});
     * }
     *
     * // Configure columns
     * for (int i = 0; i < 26; i++) {
     *     table->column_spans.push_back({.extent = 120.0f, .pinned = (i == 0)});
     * }
     *
     * table->cell_builder = [](BuildContext& ctx, int row, int col) {
     *     return Text::create(fmt::format("Cell {}-{}", row, col));
     * };
     * @endcode
     */
    class TableView : public RenderObjectWidget
    {
    public:
        /// Table dimensions (number of rows and columns).
        TableSpanExtents extents;

        /// Configuration for each row. Size should match extents.row_count.
        std::vector<TableSpan> row_spans;

        /// Configuration for each column. Size should match extents.column_count.
        std::vector<TableSpan> column_spans;

        /// Builder function for cells. Called lazily for visible cells only.
        TableCellBuilder cell_builder;

        /// Optional controller for horizontal scrolling. Creates internal if null.
        std::shared_ptr<ScrollController> horizontal_controller;

        /// Optional controller for vertical scrolling. Creates internal if null.
        std::shared_ptr<ScrollController> vertical_controller;

        /// Scroll physics for momentum and boundary behavior.
        std::shared_ptr<ScrollPhysics> physics;

        std::shared_ptr<Element> createElement() const override;
        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& render_object) const override;
    };

} // namespace systems::leal::campello_widgets
