#pragma once

#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/widgets/stateless_widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Element counterpart for StatelessWidget.
     *
     * Delegates `performBuild()` to the associated StatelessWidget's `build()`
     * method, then reconciles the returned widget against its single child.
     */
    class StatelessElement : public Element
    {
    public:
        explicit StatelessElement(std::shared_ptr<const StatelessWidget> widget);

        void update(WidgetRef new_widget) override;

        Element* firstChildElement() const noexcept override { return child_.get(); }

    protected:
        void performBuild() override;

    private:
        std::shared_ptr<Element> child_;
    };

} // namespace systems::leal::campello_widgets
