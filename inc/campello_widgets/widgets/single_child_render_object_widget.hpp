#pragma once

#include <memory>
#include <campello_widgets/widgets/render_object_widget.hpp>

namespace systems::leal::campello_widgets
{

    class RenderBox;

    /**
     * @brief A RenderObjectWidget that has exactly one optional child widget.
     *
     * Subclasses describe their own RenderObject (via `createRenderObject()` /
     * `updateRenderObject()`) and supply a `child` widget. The framework wires
     * the child's RenderBox into the parent's RenderObject automatically via
     * `SingleChildRenderObjectElement`.
     *
     * Default `insertRenderObjectChild` / `removeRenderObjectChild` implementations
     * call `RenderBox::setChild()` on the parent. Override them if your RenderObject
     * attaches the child differently.
     */
    class SingleChildRenderObjectWidget : public RenderObjectWidget
    {
    public:
        /** @brief The single child widget (may be null for leaf render objects). */
        WidgetRef child;

        std::shared_ptr<Element> createElement() const override;

        /**
         * @brief Attaches `child_box` as the child of `parent`.
         *
         * Default: `static_cast<RenderBox&>(parent).setChild(child_box)`.
         */
        virtual void insertRenderObjectChild(
            RenderObject&             parent,
            std::shared_ptr<RenderBox> child_box) const;

        /**
         * @brief Detaches the child from `parent`.
         *
         * Default: `static_cast<RenderBox&>(parent).setChild(nullptr)`.
         */
        virtual void removeRenderObjectChild(RenderObject& parent) const;
    };

} // namespace systems::leal::campello_widgets
