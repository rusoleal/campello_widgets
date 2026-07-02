#include <campello_widgets/widgets/render_object_element.hpp>
#include <campello_widgets/ui/render_object.hpp>

namespace systems::leal::campello_widgets
{

    RenderObjectElement::RenderObjectElement(
        std::shared_ptr<const RenderObjectWidget> widget)
        : Element(std::move(widget))
        , render_object_(
              static_cast<const RenderObjectWidget&>(*widget_).createRenderObject())
    {
    }

    void RenderObjectElement::mount(Element* parent)
    {
        Element::mount(parent);
    }

    void RenderObjectElement::unmount()
    {
        render_object_.reset();
        Element::unmount();
    }

    void RenderObjectElement::update(WidgetRef new_widget)
    {
        Element::update(std::move(new_widget));
        // updateRenderObject() is responsible for calling markNeedsLayout()/
        // markNeedsPaint() itself, guarded by comparing old vs new property
        // values — every RenderObjectWidget subclass does this (see e.g.
        // Transform, SizedBox, Flex). An unconditional markNeedsLayout()
        // here would defeat every one of those guards on every single
        // rebuild, regardless of whether anything actually changed —
        // exactly the bug this comment replaces (found via a
        // continuously-animating widget forcing full-tree relayout even
        // though only one leaf's property was genuinely changing).
        static_cast<const RenderObjectWidget&>(*widget_)
            .updateRenderObject(*render_object_);
    }

} // namespace systems::leal::campello_widgets
