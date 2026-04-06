#include <campello_widgets/widgets/stateful_element.hpp>

namespace systems::leal::campello_widgets
{

    StatefulElement::StatefulElement(std::shared_ptr<const StatefulWidget> widget)
        : Element(widget) // keep a shared_ptr alive before createState()
        , state_(static_cast<const StatefulWidget&>(*widget).createState())
    {
        state_->element_        = this;
        state_->current_widget_ = widget_.get();
    }

    StatefulElement::~StatefulElement() = default;

    void StatefulElement::mount(Element* parent)
    {
        state_->initState();
        Element::mount(parent);
    }

    void StatefulElement::unmount()
    {
        state_->dispose();
        Element::unmount();
    }

    void StatefulElement::update(WidgetRef new_widget)
    {
        const Widget& old_widget = *widget_;
        Element::update(std::move(new_widget));
        state_->current_widget_ = widget_.get();
        state_->didUpdateWidget(old_widget);
        rebuild();
    }

    void StatefulElement::scheduleBuild()
    {
        markNeedsBuild();
        rebuild();
    }

    void StatefulElement::performBuild()
    {
        WidgetRef built = state_->build(*this);
        auto old_child  = child_;
        child_          = updateChild(child_, std::move(built), this);

        // When the child element is replaced with a different type, a new RenderObject
        // is created but the ancestor SingleChildRenderObjectElement still holds a
        // reference to the old one. Propagate upward so it re-syncs its child pointer.
        if (child_ != old_child && parent_)
            parent_->onDescendantRenderObjectChanged();
    }

} // namespace systems::leal::campello_widgets
