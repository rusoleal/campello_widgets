#pragma once

#include <memory>
#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/widgets/stateful_widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Element counterpart for StatefulWidget.
     *
     * Owns the State object for the lifetime of the element. Calls the State's
     * lifecycle hooks (`initState`, `dispose`, `didUpdateWidget`) at the
     * appropriate moments, and delegates `performBuild()` to `State::build()`.
     */
    class StatefulElement : public Element
    {
    public:
        explicit StatefulElement(std::shared_ptr<const StatefulWidget> widget);
        ~StatefulElement() override;

        void mount(Element* parent) override;
        void unmount() override;
        void update(WidgetRef new_widget) override;

        /**
         * @brief Schedules an immediate synchronous rebuild.
         *
         * Called by `StateBase::setState()` after state mutations have been applied.
         * In a full framework this would post to the scheduler; here it rebuilds
         * synchronously.
         */
        void scheduleBuild();

        Element* firstChildElement() const noexcept override { return child_.get(); }

    protected:
        void performBuild() override;

    private:
        std::unique_ptr<StateBase> state_;
        std::shared_ptr<Element>   child_;
    };

} // namespace systems::leal::campello_widgets
