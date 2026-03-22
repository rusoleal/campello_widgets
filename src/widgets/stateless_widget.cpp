#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/widgets/stateless_element.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<Element> StatelessWidget::createElement() const
    {
        return std::make_shared<StatelessElement>(
            std::static_pointer_cast<const StatelessWidget>(shared_from_this()));
    }

} // namespace systems::leal::campello_widgets
