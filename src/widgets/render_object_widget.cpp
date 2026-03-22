#include <campello_widgets/widgets/render_object_widget.hpp>
#include <campello_widgets/widgets/render_object_element.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<Element> RenderObjectWidget::createElement() const
    {
        return std::make_shared<RenderObjectElement>(
            std::static_pointer_cast<const RenderObjectWidget>(shared_from_this()));
    }

} // namespace systems::leal::campello_widgets
