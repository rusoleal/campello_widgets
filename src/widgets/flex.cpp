#include <campello_widgets/widgets/flex.hpp>
#include <campello_widgets/widgets/flex_element.hpp>
#include <campello_widgets/ui/render_flex.hpp>
#include <campello_widgets/widgets/flexible.hpp>
#include <campello_widgets/ui/render_box.hpp>

namespace systems::leal::campello_widgets
{

    // ------------------------------------------------------------------
    // FlexElement
    // ------------------------------------------------------------------

    FlexElement::FlexElement(std::shared_ptr<const Flex> widget)
        : MultiChildRenderObjectElement(std::move(widget))
    {}

    void FlexElement::syncChildRenderObjects()
    {
        const auto& fw = static_cast<const Flex&>(*widget_);
        auto&       rf = static_cast<RenderFlex&>(*render_object_);

        rf.clearChildren();

        for (int i = 0; i < static_cast<int>(child_elements_.size()); ++i)
        {
            if (!child_elements_[i]) continue;

            auto* roe = child_elements_[i]->findDescendantRenderObjectElement();
            if (!roe) continue;

            auto box = std::dynamic_pointer_cast<RenderBox>(roe->sharedRenderObject());
            if (!box) continue;

            int flex = 0;
            if (i < static_cast<int>(fw.children.size()))
            {
                if (auto* f = dynamic_cast<const Flexible*>(fw.children[i].get()))
                    flex = f->flex;
            }

            rf.insertChild(std::move(box), i, flex);
        }
    }

    // ------------------------------------------------------------------
    // Flex
    // ------------------------------------------------------------------

    std::shared_ptr<Element> Flex::createElement() const
    {
        return std::make_shared<FlexElement>(
            std::static_pointer_cast<const Flex>(shared_from_this()));
    }

    std::shared_ptr<RenderObject> Flex::createRenderObject() const
    {
        auto ro               = std::make_shared<RenderFlex>();
        ro->axis              = axis;
        ro->main_axis_alignment  = main_axis_alignment;
        ro->cross_axis_alignment = cross_axis_alignment;
        ro->main_axis_size    = main_axis_size;
        return ro;
    }

    void Flex::updateRenderObject(RenderObject& ro) const
    {
        auto& rf                = static_cast<RenderFlex&>(ro);
        rf.axis                 = axis;
        rf.main_axis_alignment  = main_axis_alignment;
        rf.cross_axis_alignment = cross_axis_alignment;
        rf.main_axis_size       = main_axis_size;
        rf.markNeedsLayout();
    }

    void Flex::insertRenderObjectChild(
        RenderObject& parent, std::shared_ptr<RenderBox> child_box, int index) const
    {
        static_cast<RenderFlex&>(parent).insertChild(std::move(child_box), index, 0);
    }

    void Flex::clearRenderObjectChildren(RenderObject& parent) const
    {
        static_cast<RenderFlex&>(parent).clearChildren();
    }

} // namespace systems::leal::campello_widgets
