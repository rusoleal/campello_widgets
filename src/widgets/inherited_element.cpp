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
        const auto& old_w = static_cast<const InheritedWidget&>(*widget_);
        Element::update(std::move(new_widget));
        const auto& new_w = static_cast<const InheritedWidget&>(*widget_);
        if (new_w.updateShouldNotify(old_w))
            notifyDependents();
        rebuild();
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
        const auto& w = static_cast<const InheritedWidget&>(*widget_);
        child_ = updateChild(child_, w.child, this);
    }

    void InheritedElement::notifyDependents()
    {
        for (Element* dep : dependents_)
            dep->markNeedsBuild();
    }

} // namespace systems::leal::campello_widgets
