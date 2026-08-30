#pragma once

#include <vector>
#include <functional>
#include <campello_widgets/widgets/stateful_widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A vertical list whose items can be reordered by dragging.
     *
     * Simplifications versus Flutter's `ReorderableListView`, noted
     * explicitly (this is the "moderate scope" item of this batch -- see
     * the borrowed-pattern note below):
     *  - Assumes a uniform `item_extent` per item (drag-target math in
     *    `computeReorderTargetIndex()`, `ui/reorder_math.hpp`, needs it to
     *    convert a drag distance into an index shift).
     *  - The whole item is the drag handle (no separate handle affordance).
     *  - The dragged item follows the finger via a live `Transform`; other
     *    items do not animate out of the way mid-drag the way Flutter's
     *    does -- the list simply reflows once `on_reorder` fires and the
     *    caller supplies the new order.
     *  - `on_reorder(old_index, new_index)` gives the item's *final*
     *    resting index directly (move it there in your own data, e.g.
     *    `std::vector::erase` + `insert`) -- simpler than Flutter's
     *    remove-then-insert-adjusted convention.
     *
     * Borrows only `RenderDraggable`'s slop/arena gesture-disambiguation
     * *pattern* (via the same `GestureDetector` primitive every draggable
     * widget in this codebase already uses for `on_pan_*`), not the
     * `Draggable<T>`/`DragTarget<T>`/`DragManager` stack built for
     * cross-widget drag-and-drop -- unnecessary generality for same-list
     * reordering.
     *
     * @code
     * auto w = std::make_shared<ReorderableListView>();
     * w->children     = itemWidgets;
     * w->item_extent  = 56.0f;
     * w->on_reorder   = [&](int oldIndex, int newIndex) {
     *     auto item = std::move(items[oldIndex]);
     *     items.erase(items.begin() + oldIndex);
     *     items.insert(items.begin() + newIndex, std::move(item));
     * };
     * @endcode
     */
    class ReorderableListView : public StatefulWidget
    {
    public:
        std::vector<WidgetRef>              children;
        float                                item_extent = 56.0f;
        std::function<void(int, int)>       on_reorder;

        ReorderableListView() = default;

        std::unique_ptr<StateBase> createState() const override;
    };

} // namespace systems::leal::campello_widgets
