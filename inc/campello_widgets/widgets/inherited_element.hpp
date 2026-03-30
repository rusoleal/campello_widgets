#pragma once

#include <unordered_set>
#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/widgets/inherited_widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Element counterpart for InheritedWidget.
     *
     * On mount, registers itself in the inherited_widgets_ map so that
     * descendants can look it up via BuildContext::dependOnInheritedWidgetOfExactType.
     *
     * On update, calls InheritedWidget::updateShouldNotify(). If it returns
     * true, all registered dependents are marked dirty and will rebuild on the
     * next frame.
     */
    class InheritedElement : public Element
    {
    public:
        explicit InheritedElement(std::shared_ptr<const InheritedWidget> widget);

        void mount(Element* parent) override;
        void update(WidgetRef new_widget) override;
        void unmount() override;

        Element* firstChildElement() const noexcept override { return child_.get(); }

        /**
         * @brief Registers `element` as a dependent of this InheritedElement.
         *
         * Called by Element::getInheritedWidget() when a descendant subscribes.
         * The dependent will be rebuilt whenever updateShouldNotify returns true.
         */
        void addDependent(Element* element);

    protected:
        void onMountInheritance() override;
        void performBuild() override;

    private:
        void notifyDependents();

        std::shared_ptr<Element>      child_;
        std::unordered_set<Element*>  dependents_;
    };

} // namespace systems::leal::campello_widgets
