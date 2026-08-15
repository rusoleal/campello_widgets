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
        // A dependent can be unmounted (and destroyed) by ordinary tree
        // reconciliation earlier in the very same rebuild pass that also
        // touches this InheritedElement — e.g. a structurally different
        // subtree swapped in elsewhere under the same setState() — without
        // ever being removed from dependents_ (only InheritedElement's own
        // unmount() clears the set; unmounting a single dependent doesn't).
        // Prune stale entries lazily via the framework's liveness registry
        // rather than calling markNeedsBuild() on a dangling pointer.
        for (auto it = dependents_.begin(); it != dependents_.end();)
        {
            Element* dep = *it;
            if (!Element::isAlive(dep))
            {
                it = dependents_.erase(it);
                continue;
            }
            dep->markNeedsBuild();
            ++it;
        }
    }

} // namespace systems::leal::campello_widgets
