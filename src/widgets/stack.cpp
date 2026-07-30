#include <campello_widgets/diagnostics/diagnostic_property.hpp>
#include <campello_widgets/widgets/stack.hpp>
#include <campello_widgets/widgets/stack_element.hpp>
#include <campello_widgets/ui/render_stack.hpp>
#include <campello_widgets/widgets/positioned.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/widgets/render_object_element.hpp>

namespace systems::leal::campello_widgets
{

    namespace
    {
        // Looks for a Positioned widget between `start` and its nearest
        // RenderObjectElement descendant. A plain dynamic_cast on the
        // top-level Stack child widget only sees a Positioned that is the
        // *direct* child — but it's common for a Positioned to be produced
        // several StatefulWidget/StatelessWidget layers down (e.g. an
        // Overlay entry wrapping a ValueListenableBuilder that builds a
        // Positioned), which would otherwise be invisible to Stack.
        const Positioned* findPositionedBeforeRenderObject(Element* start)
        {
            for (Element* e = start; e; e = e->firstChildElement())
            {
                if (auto* p = dynamic_cast<const Positioned*>(&e->widget()))
                    return p;
                if (dynamic_cast<RenderObjectElement*>(e))
                    break;
            }
            return nullptr;
        }
    } // namespace

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

        // Re-insert every current child in place (RenderStack::insertChild()
        // reuses a slot without re-parenting when the same box is already
        // there) rather than clearChildren()-ing first — this can run once
        // per pointer-move (a Positioned several layers below an Overlay
        // entry re-notifying via onDescendantRenderObjectChanged(), see
        // PositionedElement's doc), and a blind clear would detach/reattach
        // every sibling — including, at the root Overlay's Stack, the
        // entire rest of the app — just to reposition the one child that
        // actually changed.
        int count = 0;
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
                if (const auto* p = findPositionedBeforeRenderObject(child_elements_[i].get()))
                {
                    left   = p->left;
                    top    = p->top;
                    right  = p->right;
                    bottom = p->bottom;
                    width  = p->width;
                    height = p->height;
                }
            }

            rs.insertChild(std::move(box), count, left, top, right, bottom, width, height);
            ++count;
        }

        rs.truncateChildren(static_cast<size_t>(count));
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
        if (rs.fit == fit) return;
        rs.fit = fit;
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


    void Stack::debugFillProperties(DiagnosticsPropertyBuilder& properties) const
    {
        properties.add(std::make_unique<StringProperty>("fit", fit == StackFit::loose ? "loose" : (fit == StackFit::expand ? "expand" : "passthrough")));
    }
} // namespace systems::leal::campello_widgets
