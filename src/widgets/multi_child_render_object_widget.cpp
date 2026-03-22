#include <campello_widgets/widgets/multi_child_render_object_widget.hpp>
#include <campello_widgets/widgets/multi_child_render_object_element.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<Element> MultiChildRenderObjectWidget::createElement() const
    {
        return std::make_shared<MultiChildRenderObjectElement>(
            std::static_pointer_cast<const MultiChildRenderObjectWidget>(shared_from_this()));
    }

} // namespace systems::leal::campello_widgets
