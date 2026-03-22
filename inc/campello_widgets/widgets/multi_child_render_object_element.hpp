#pragma once

#include <vector>
#include <campello_widgets/widgets/render_object_element.hpp>
#include <campello_widgets/widgets/multi_child_render_object_widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Element for MultiChildRenderObjectWidget.
     *
     * Reconciles the ordered child list against the widget's `children` vector,
     * then syncs all child RenderBoxes into the parent RenderObject via the
     * widget's `insertRenderObjectChild()` / `clearRenderObjectChildren()` hooks.
     */
    class MultiChildRenderObjectElement : public RenderObjectElement
    {
    public:
        explicit MultiChildRenderObjectElement(
            std::shared_ptr<const MultiChildRenderObjectWidget> widget);

        void update(WidgetRef new_widget) override;
        void unmount() override;

        Element* firstChildElement() const noexcept override
        {
            return child_elements_.empty() ? nullptr : child_elements_.front().get();
        }

    protected:
        void performBuild() override;

        /**
         * @brief Syncs child RenderBoxes into the parent RenderObject.
         *
         * Called after `child_elements_` is up-to-date. Subclasses (e.g. FlexElement)
         * may override to attach additional per-child data (e.g. flex factors).
         */
        virtual void syncChildRenderObjects();

        std::vector<std::shared_ptr<Element>> child_elements_;
    };

} // namespace systems::leal::campello_widgets
