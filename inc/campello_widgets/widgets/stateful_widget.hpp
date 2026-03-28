#pragma once

#include <functional>
#include <memory>
#include <campello_widgets/widgets/widget.hpp>
#include <campello_widgets/widgets/build_context.hpp>

namespace systems::leal::campello_widgets
{

    class StatefulElement;

    /**
     * @brief Non-template base class for all State objects.
     *
     * Holds the back-pointer to the owning StatefulElement and provides
     * `setState()` for triggering rebuilds. Users should subclass `State<W>`
     * rather than this class directly.
     */
    class StateBase
    {
    public:
        virtual ~StateBase() = default;

        /**
         * @brief Describes the UI for the current state.
         *
         * Called by the framework during every rebuild of the owning
         * StatefulElement. Must not have side effects or mutate state —
         * mutations belong in event handlers that call `setState()`.
         */
        virtual WidgetRef build(BuildContext& context) = 0;

        /** @brief Called once immediately after the element is inserted into the tree. */
        virtual void initState() {}

        /** @brief Called when the element is permanently removed from the tree. */
        virtual void dispose() {}

        /**
         * @brief Called when the parent supplies a new widget of the same type.
         *
         * @param old_widget The previous widget configuration.
         */
        virtual void didUpdateWidget(const Widget& /*old_widget*/) {}

        /**
         * @brief Notifies the framework that this state has changed.
         *
         * Executes `fn` synchronously (use it to mutate member variables), then
         * schedules a rebuild of the owning element.
         *
         * @param fn A callable that performs the state mutations.
         */
        void setState(std::function<void()> fn);

        /** @brief Returns the element associated with this state. */
        StatefulElement* element() const { return element_; }

    protected:
        friend class StatefulElement;

        const Widget*    current_widget_ = nullptr; ///< Kept in sync by StatefulElement.
        StatefulElement* element_        = nullptr; ///< Back-pointer to the owning element.
    };

    /**
     * @brief Typed state companion for a specific StatefulWidget subclass.
     *
     * Provides a typed `widget()` accessor so State implementations can read
     * their widget's configuration without casting.
     *
     * @tparam W  The StatefulWidget subclass this state is associated with.
     *
     * **Usage:**
     * @code
     * class CounterState : public State<CounterWidget> {
     * public:
     *     void initState() override { count_ = widget().initial_count; }
     *     WidgetRef build(BuildContext& ctx) override { ... }
     * private:
     *     int count_ = 0;
     * };
     * @endcode
     */
    template <typename W>
    class State : public StateBase
    {
    public:
        /** @brief The current widget configuration, typed as W. */
        const W& widget() const
        {
            return static_cast<const W&>(*current_widget_);
        }
    };

    /**
     * @brief A widget that pairs with a mutable State object.
     *
     * StatefulWidget itself is immutable (like all widgets). Mutable state lives
     * in the companion State object, which persists across rebuilds as long as the
     * widget remains at the same position in the tree.
     *
     * **Usage:**
     * @code
     * class CounterWidget : public StatefulWidget {
     * public:
     *     int initial_count = 0;
     *     std::unique_ptr<StateBase> createState() const override {
     *         return std::make_unique<CounterState>();
     *     }
     * };
     * @endcode
     */
    class StatefulWidget : public Widget
    {
    public:
        /**
         * @brief Factory method that produces the State object for this widget.
         *
         * Called once by the framework when the element is first mounted. The
         * returned State object lives for the lifetime of the element.
         */
        virtual std::unique_ptr<StateBase> createState() const = 0;

        std::shared_ptr<Element> createElement() const override;
    };

} // namespace systems::leal::campello_widgets
