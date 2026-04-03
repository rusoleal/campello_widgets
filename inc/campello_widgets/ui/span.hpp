#pragma once

#include <cstddef>
#include <vector>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Configuration for a row or column span in a TableView.
     *
     * TableSpan defines the size and behavior of a single row or column
     * in a two-dimensional table. Spans can be pinned to remain visible
     * during scrolling.
     */
    struct TableSpan
    {
        /// Size of the span in logical pixels (height for rows, width for columns).
        float extent = 0.0f;

        /// Padding before this span in logical pixels.
        float padding = 0.0f;

        /// If true, this span remains fixed during scrolling.
        bool pinned = false;

        /// Background color for this span (optional, default = transparent).
        // TODO: Add Color background_color when Color type is available
        // campello_gpu::Color background_color = campello_gpu::Color::transparent();
    };

    /**
     * @brief Extents of a TableView (number of rows and columns).
     */
    struct TableSpanExtents
    {
        /// Number of rows in the table.
        int row_count = 0;

        /// Number of columns in the table.
        int column_count = 0;
    };

} // namespace systems::leal::campello_widgets
