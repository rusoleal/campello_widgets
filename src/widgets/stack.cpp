#include <campello_widgets/widgets/stack.hpp>
#include <campello_widgets/widgets/stack_element.hpp>
#include <campello_widgets/ui/render_stack.hpp>
#include <campello_widgets/widgets/positioned.hpp>
#include <campello_widgets/ui/render_box.hpp>

namespace systems::leal::campello_widgets
{

    // ------------------------------------------------------------------
    // StackElement
    // ------------------------------------------------------------------

    StackElement::StackElement(std::shared_ptr<const Stack> widget)
        : MultiChildRenderObjectElement(std::move(widget))
    {}

    void StackElement::syncChildRenderObjects()
    {
        const auto& sw = static_cast<const Stack&>(*widget_);
        auto&       rs = static_cast<RenderStack&>(*render_object_);

        rs.clearChildren();

        for (int i = 0; i < static_cast<int>(child_elements_.size()); ++i)
        {
            if (!child_elements_[i]) continue;

            auto* roe = child_elements_[i]->findDescendantRenderObjectElement();
            if (!roe) continue;

            auto box = std::dynamic_pointer_cast<RenderBox>(roe->sharedRenderObject());
            if (!box) continue;

            std::optional<float> left, top, right, bottom, width, height;
            if (i < static_cast<int>(sw.children.size()))
            {
                if (auto* p = dynamic_cast<const Positioned*>(sw.children[i].get()))
                {
                    left   = p->left;
                    top    = p->top;
                    right  = p->right;
                    bottom = p->bottom;
                    width  = p->width;
                    height = p->height;
                }
            }

            rs.insertChild(std::move(box), i, left, top, right, bottom, width, height);
        }
    }

    // ------------------------------------------------------------------
    // Stack
    // ------------------------------------------------------------------

    std::shared_ptr<Element> Stack::createElement() const
    {
        return std::make_shared<StackElement>(
            std::static_pointer_cast<const Stack>(shared_from_this()));
    }

    std::shared_ptr<RenderObject> Stack::createRenderObject() const
    {
        auto ro  = std::make_shared<RenderStack>();
        ro->fit  = fit;
        return ro;
    }

    void Stack::updateRenderObject(RenderObject& ro) const
    {
        auto& rs = static_cast<RenderStack&>(ro);
        rs.fit   = fit;
        rs.markNeedsLayout();
    }

    void Stack::insertRenderObjectChild(
        RenderObject& parent, std::shared_ptr<RenderBox> child_box, int index) const
    {
        static_cast<RenderStack&>(parent).insertChild(
            std::move(child_box), index,
            std::nullopt, std::nullopt, std::nullopt, std::nullopt,
            std::nullopt, std::nullopt);
    }

    void Stack::clearRenderObjectChildren(RenderObject& parent) const
    {
        static_cast<RenderStack&>(parent).clearChildren();
    }

} // namespace systems::leal::campello_widgets
