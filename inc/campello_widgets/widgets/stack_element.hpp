#pragma once

#include <campello_widgets/widgets/multi_child_render_object_element.hpp>

namespace systems::leal::campello_widgets
{

    class Stack;

    /**
     * @brief Element for the Stack widget.
     *
     * Overrides `syncChildRenderObjects()` to inspect each child widget for a
     * `Positioned` wrapper, extract its position constraints, and pass them to
     * `RenderStack::insertChild()`.
     */
    class StackElement : public MultiChildRenderObjectElement
    {
    public:
        explicit StackElement(std::shared_ptr<const Stack> widget);

    protected:
        void syncChildRenderObjects() override;
    };

} // namespace systems::leal::campello_widgets
