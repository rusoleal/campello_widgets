#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/widgets/single_child_render_object_element.hpp>
#include <campello_widgets/ui/render_box.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<Element> SingleChildRenderObjectWidget::createElement() const
    {
        return std::make_shared<SingleChildRenderObjectElement>(
            std::static_pointer_cast<const SingleChildRenderObjectWidget>(shared_from_this()));
    }

    void SingleChildRenderObjectWidget::insertRenderObjectChild(
        RenderObject&              parent,
        std::shared_ptr<RenderBox> child_box) const
    {
        static_cast<RenderBox&>(parent).setChild(std::move(child_box));
    }

    void SingleChildRenderObjectWidget::removeRenderObjectChild(
        RenderObject& parent) const
    {
        static_cast<RenderBox&>(parent).setChild(nullptr);
    }

} // namespace systems::leal::campello_widgets
