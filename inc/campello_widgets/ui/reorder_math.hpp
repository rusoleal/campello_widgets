#pragma once

#include <cmath>
#include <algorithm>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Given a dragged list item's source index and cumulative drag
     * delta, computes which index it should land at.
     *
     * Assumes a uniform `item_extent` (see `ReorderableListView`'s doc
     * comment for the simplification this implies) -- rounds the drag
     * delta to the nearest whole item-slot shift and clamps to the valid
     * index range.
     */
    inline int computeReorderTargetIndex(
        int source_index, float drag_delta, float item_extent, int item_count)
    {
        if (item_extent <= 0.0f || item_count <= 0) return source_index;
        const int shift  = static_cast<int>(std::lround(drag_delta / item_extent));
        const int target = source_index + shift;
        return std::clamp(target, 0, item_count - 1);
    }

} // namespace systems::leal::campello_widgets
