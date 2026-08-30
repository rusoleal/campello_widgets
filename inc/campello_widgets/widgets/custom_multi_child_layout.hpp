#pragma once

#include <memory>
#include <campello_widgets/widgets/multi_child_render_object_widget.hpp>
#include <campello_widgets/ui/multi_child_layout_delegate.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Lays out its (id-tagged, via `LayoutId`) children according to
     * an arbitrary `MultiChildLayoutDelegate`.
     *
     * Matches Flutter's `CustomMultiChildLayout` widget.
     *
     * @code
     * auto w = std::make_shared<CustomMultiChildLayout>();
     * w->delegate = myDelegate;
     * w->children = {
     *     std::make_shared<LayoutId>("title", titleWidget),
     *     std::make_shared<LayoutId>("body",  bodyWidget),
     * };
     * @endcode
     */
    class CustomMultiChildLayout : public MultiChildRenderObjectWidget
    {
    public:
        std::shared_ptr<MultiChildLayoutDelegate> delegate;

        CustomMultiChildLayout() = default;
        explicit CustomMultiChildLayout(std::shared_ptr<MultiChildLayoutDelegate> d) : delegate(std::move(d)) {}

        std::shared_ptr<Element> createElement() const override;

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;

        void insertRenderObjectChild(
            RenderObject& parent, std::shared_ptr<RenderBox> child_box, int index) const override;
        void clearRenderObjectChildren(RenderObject& parent) const override;
    };

} // namespace systems::leal::campello_widgets
