#pragma once

#include <campello_widgets/widgets/multi_child_render_object_element.hpp>

namespace systems::leal::campello_widgets
{

    class CustomMultiChildLayout;

    /**
     * @brief Element for the CustomMultiChildLayout widget.
     *
     * Overrides `syncChildRenderObjects()` to inspect each child widget for
     * a `LayoutId` wrapper and pass its id to
     * `RenderCustomMultiChildLayout::insertChild()` -- mirrors
     * `FlexElement`'s identical role for `Flexible`'s flex factor, minus its
     * last-synced-diff skip-optimization (kept simple: always re-syncs).
     */
    class CustomMultiChildLayoutElement : public MultiChildRenderObjectElement
    {
    public:
        explicit CustomMultiChildLayoutElement(std::shared_ptr<const CustomMultiChildLayout> widget);

    protected:
        void syncChildRenderObjects() override;
    };

} // namespace systems::leal::campello_widgets
