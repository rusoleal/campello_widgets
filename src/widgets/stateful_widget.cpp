#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/widgets/stateful_element.hpp>

namespace systems::leal::campello_widgets
{

    void StateBase::setState(std::function<void()> fn)
    {
        fn();
        if (element_)
            element_->scheduleBuild();
    }

    std::shared_ptr<Element> StatefulWidget::createElement() const
    {
        return std::make_shared<StatefulElement>(
            std::static_pointer_cast<const StatefulWidget>(shared_from_this()));
    }

} // namespace systems::leal::campello_widgets
