#include <campello_widgets/widgets/inherited_widget.hpp>
#include <campello_widgets/widgets/inherited_element.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<Element> InheritedWidget::createElement() const
    {
        return std::make_shared<InheritedElement>(
            std::static_pointer_cast<const InheritedWidget>(shared_from_this()));
    }

} // namespace systems::leal::campello_widgets
