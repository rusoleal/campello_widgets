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
        static_cast<const RenderObjectWidget&>(*widget_)
            .updateRenderObject(*render_object_);
        render_object_->markNeedsLayout();
        markNeedsBuild();
        rebuild();
    }

} // namespace systems::leal::campello_widgets
