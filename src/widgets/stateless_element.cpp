#include <campello_widgets/widgets/stateless_element.hpp>

namespace systems::leal::campello_widgets
{

    StatelessElement::StatelessElement(std::shared_ptr<const StatelessWidget> widget)
        : Element(std::move(widget))
    {
    }

    void StatelessElement::unmount()
    {
        if (child_)
            child_->unmount();
        Element::unmount();
    }

    void StatelessElement::update(WidgetRef new_widget)
    {
        Element::update(std::move(new_widget));
        rebuild();
    }

    void StatelessElement::performBuild()
    {
        const auto& w   = static_cast<const StatelessWidget&>(*widget_);
        WidgetRef   built = w.build(*this);
        child_          = updateChild(child_, std::move(built), this);
    }

} // namespace systems::leal::campello_widgets
