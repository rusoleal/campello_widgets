#pragma once

#include <campello_widgets/widgets/render_object_element.hpp>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Element for SingleChildRenderObjectWidget.
     *
     * Builds the child widget into a child element, finds the child's RenderBox
     * via `findDescendantRenderObjectElement()`, and wires it into the parent's
     * RenderObject using the widget's `insertRenderObjectChild()` hook.
     */
    class SingleChildRenderObjectElement : public RenderObjectElement
    {
    public:
        explicit SingleChildRenderObjectElement(
            std::shared_ptr<const SingleChildRenderObjectWidget> widget);

        void update(WidgetRef new_widget) override;
        void unmount() override;
        void onDescendantRenderObjectChanged() override;

        Element* firstChildElement() const noexcept override
        {
            return child_element_.get();
        }

    protected:
        void performBuild() override;

    private:
        void syncChildRenderObject();

        std::shared_ptr<Element> child_element_;
    };

} // namespace systems::leal::campello_widgets
