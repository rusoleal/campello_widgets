#include <campello_widgets/widgets/custom_multi_child_layout.hpp>
#include <campello_widgets/widgets/custom_multi_child_layout_element.hpp>
#include <campello_widgets/widgets/layout_id.hpp>
#include <campello_widgets/ui/render_custom_multi_child_layout.hpp>
#include <campello_widgets/ui/render_box.hpp>

namespace systems::leal::campello_widgets
{

    // ------------------------------------------------------------------
    // CustomMultiChildLayoutElement
    // ------------------------------------------------------------------

    CustomMultiChildLayoutElement::CustomMultiChildLayoutElement(
        std::shared_ptr<const CustomMultiChildLayout> widget)
        : MultiChildRenderObjectElement(std::move(widget))
    {}

    void CustomMultiChildLayoutElement::syncChildRenderObjects()
    {
        const auto& lw = static_cast<const CustomMultiChildLayout&>(*widget_);
        auto&       rl = static_cast<RenderCustomMultiChildLayout&>(*render_object_);

        rl.clearChildren();
        for (int i = 0; i < static_cast<int>(child_elements_.size()); ++i)
        {
            if (!child_elements_[i]) continue;

            auto* roe = child_elements_[i]->findDescendantRenderObjectElement();
            if (!roe) continue;

            auto box = std::dynamic_pointer_cast<RenderBox>(roe->sharedRenderObject());
            if (!box) continue;

            std::string id = std::to_string(i);
            if (i < static_cast<int>(lw.children.size()))
            {
                if (auto* lid = dynamic_cast<const LayoutId*>(lw.children[i].get()))
                    id = lid->id;
            }

            rl.insertChild(id, std::move(box));
        }
    }

    // ------------------------------------------------------------------
    // CustomMultiChildLayout
    // ------------------------------------------------------------------

    std::shared_ptr<Element> CustomMultiChildLayout::createElement() const
    {
        return std::make_shared<CustomMultiChildLayoutElement>(
            std::static_pointer_cast<const CustomMultiChildLayout>(shared_from_this()));
    }

    std::shared_ptr<RenderObject> CustomMultiChildLayout::createRenderObject() const
    {
        auto r      = std::make_shared<RenderCustomMultiChildLayout>();
        r->delegate = delegate;
        return r;
    }

    void CustomMultiChildLayout::updateRenderObject(RenderObject& ro) const
    {
        static_cast<RenderCustomMultiChildLayout&>(ro).delegate = delegate;
    }

    void CustomMultiChildLayout::insertRenderObjectChild(
        RenderObject& parent, std::shared_ptr<RenderBox> child_box, int index) const
    {
        // Dead in practice -- CustomMultiChildLayoutElement::syncChildRenderObjects()
        // always drives real inserts (it has the LayoutId ids); this only
        // satisfies the pure-virtual base contract, same as Flex::insertRenderObjectChild.
        static_cast<RenderCustomMultiChildLayout&>(parent).insertChild(std::to_string(index), std::move(child_box));
    }

    void CustomMultiChildLayout::clearRenderObjectChildren(RenderObject& parent) const
    {
        static_cast<RenderCustomMultiChildLayout&>(parent).clearChildren();
    }

} // namespace systems::leal::campello_widgets
