#include <campello_widgets/widgets/inherited_element.hpp>

namespace systems::leal::campello_widgets
{

    InheritedElement::InheritedElement(std::shared_ptr<const InheritedWidget> widget)
        : Element(std::move(widget))
    {
    }

    void InheritedElement::mount(Element* parent)
    {
        Element::mount(parent);
    }

    void InheritedElement::onMountInheritance()
    {
        // Insert ourselves so descendants can find us by our widget's exact type.
        inherited_widgets_[std::type_index(widget_.get()->widgetType())] = this;
    }

    void InheritedElement::update(WidgetRef new_widget)
    {
        // See StatefulElement::update() for why the old widget must be kept
        // alive (as a shared_ptr) across Element::update(), not just
        // referenced — that call reassigns widget_ and, when nothing else
        // references the previous widget, destroys it immediately, which
        // would leave old_w dangling for the updateShouldNotify() call below.
        WidgetRef old_widget = widget_;
        const auto& old_w = static_cast<const InheritedWidget&>(*old_widget);
        Element::update(std::move(new_widget));
        const auto& new_w = static_cast<const InheritedWidget&>(*widget_);
        if (new_w.updateShouldNotify(old_w))
            notifyDependents();
    }

    void InheritedElement::unmount()
    {
        dependents_.clear();
        Element::unmount();
    }

    void InheritedElement::addDependent(Element* element)
    {
        dependents_.insert(element);
    }

    void InheritedElement::performBuild()
    {
        const auto& w        = static_cast<const InheritedWidget&>(*widget_);
        auto        old_child = child_;
        child_               = updateChild(child_, w.child, this);

        // See StatelessElement::performBuild() / StatefulElement::performBuild().
        if (child_ != old_child)
            onDescendantRenderObjectChanged();
    }

    void InheritedElement::notifyDependents()
    {
        for (Element* dep : dependents_)
            dep->markNeedsBuild();
    }

} // namespace systems::leal::campello_widgets
