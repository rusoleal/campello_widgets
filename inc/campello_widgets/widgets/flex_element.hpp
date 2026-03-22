#pragma once

#include <campello_widgets/widgets/multi_child_render_object_element.hpp>

namespace systems::leal::campello_widgets
{

    class Flex;

    /**
     * @brief Element for the Flex widget.
     *
     * Overrides `syncChildRenderObjects()` to inspect each child widget for a
     * `Flexible` wrapper, extract its `flex` factor, and pass that factor to
     * `RenderFlex::insertChild()`.
     */
    class FlexElement : public MultiChildRenderObjectElement
    {
    public:
        explicit FlexElement(std::shared_ptr<const Flex> widget);

    protected:
        void syncChildRenderObjects() override;
    };

} // namespace systems::leal::campello_widgets
