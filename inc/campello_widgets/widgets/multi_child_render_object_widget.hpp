#pragma once

#include <memory>
#include <vector>
#include <campello_widgets/widgets/render_object_widget.hpp>

namespace systems::leal::campello_widgets
{

    class RenderBox;

    /**
     * @brief A RenderObjectWidget that manages an ordered list of child widgets.
     *
     * Subclasses implement `createRenderObject()` / `updateRenderObject()` as usual,
     * plus `insertRenderObjectChild()` and `clearRenderObjectChildren()` to keep the
     * RenderObject's child list in sync with the element tree.
     */
    class MultiChildRenderObjectWidget : public RenderObjectWidget
    {
    public:
        /** @brief Ordered list of child widgets. */
        std::vector<WidgetRef> children;

        std::shared_ptr<Element> createElement() const override;

        /**
         * @brief Inserts `child_box` at position `index` in the parent's child list.
         */
        virtual void insertRenderObjectChild(
            RenderObject&             parent,
            std::shared_ptr<RenderBox> child_box,
            int                        index) const = 0;

        /**
         * @brief Removes all children from the parent's child list.
         *
         * Called before re-inserting children after a rebuild.
         */
        virtual void clearRenderObjectChildren(RenderObject& parent) const = 0;
    };

} // namespace systems::leal::campello_widgets
