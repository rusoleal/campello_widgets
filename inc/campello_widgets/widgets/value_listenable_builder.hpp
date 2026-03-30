#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/value_notifier.hpp>

#include <functional>
#include <memory>

namespace systems::leal::campello_widgets
{

    // Forward declaration
    template<typename T> class ValueListenableBuilder;

    /**
     * @brief State for ValueListenableBuilder<T>.
     *
     * Subscribes to the ValueNotifier in initState and calls setState whenever
     * the value changes, triggering a rebuild via the builder callback.
     */
    template<typename T>
    class ValueListenableBuilderState : public State<ValueListenableBuilder<T>>
    {
    public:
        void initState() override
        {
            const auto& w = this->widget();
            if (w.valueListenable)
            {
                listener_id_ = w.valueListenable->addListener([this]() {
                    this->setState([](){});
                });
            }
        }

        void dispose() override
        {
            const auto& w = this->widget();
            if (w.valueListenable && listener_id_ != 0)
                w.valueListenable->removeListener(listener_id_);
        }

        void didUpdateWidget(const Widget& old_base) override
        {
            const auto& old_w = static_cast<const ValueListenableBuilder<T>&>(old_base);
            const auto& new_w = this->widget();

            if (old_w.valueListenable.get() != new_w.valueListenable.get())
            {
                if (old_w.valueListenable && listener_id_ != 0)
                    old_w.valueListenable->removeListener(listener_id_);
                listener_id_ = 0;
                if (new_w.valueListenable)
                {
                    listener_id_ = new_w.valueListenable->addListener([this]() {
                        this->setState([](){});
                    });
                }
            }
        }

        WidgetRef build(BuildContext& ctx) override
        {
            const auto& w = this->widget();
            const T& val  = w.valueListenable ? w.valueListenable->value() : T{};
            return w.builder(ctx, val, w.child);
        }

    private:
        uint64_t listener_id_ = 0;
    };

    /**
     * @brief Rebuilds whenever a ValueNotifier's value changes.
     *
     * Mirrors Flutter's `ValueListenableBuilder<T>`. The optional `child`
     * parameter is passed unchanged to `builder` — useful for expensive
     * subtrees that do not depend on the value.
     *
     * @code
     * auto counter = std::make_shared<ValueNotifier<int>>(0);
     *
     * auto w = std::make_shared<ValueListenableBuilder<int>>();
     * w->valueListenable = counter;
     * w->builder = [](BuildContext&, const int& v, WidgetRef) {
     *     return std::make_shared<Text>(std::to_string(v));
     * };
     * @endcode
     */
    template<typename T>
    class ValueListenableBuilder : public StatefulWidget
    {
    public:
        std::shared_ptr<ValueNotifier<T>>                               valueListenable;
        std::function<WidgetRef(BuildContext&, const T&, WidgetRef)>    builder;
        WidgetRef                                                        child;

        std::unique_ptr<StateBase> createState() const override
        {
            return std::make_unique<ValueListenableBuilderState<T>>();
        }
    };

} // namespace systems::leal::campello_widgets
