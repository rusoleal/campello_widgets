#include <campello_widgets/widgets/positioned.hpp>
#include <campello_widgets/widgets/positioned_element.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<Element> Positioned::createElement() const
    {
        return std::make_shared<PositionedElement>(
            std::static_pointer_cast<const StatelessWidget>(shared_from_this()));
    }

    void PositionedElement::performBuild()
    {
        StatelessElement::performBuild();
        if (parent_) parent_->onDescendantRenderObjectChanged();
    }

} // namespace systems::leal::campello_widgets
