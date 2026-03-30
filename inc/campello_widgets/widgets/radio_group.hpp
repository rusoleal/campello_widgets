#pragma once

#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/widgets/inherited_widget.hpp>

#include <functional>

namespace systems::leal::campello_widgets
{

    // -----------------------------------------------------------------------
    // RadioGroupScope — InheritedWidget that carries group state
    // -----------------------------------------------------------------------

    /**
     * @brief InheritedWidget that propagates the selected value and change
     *        callback to all `Radio` widgets in the subtree.
     *
     * Users should not create this directly — use `RadioGroup` instead.
     */
    class RadioGroupScope : public InheritedWidget
    {
    public:
        int                       group_value = 0;
        std::function<void(int)>  on_changed;

        bool updateShouldNotify(const InheritedWidget& old) const override
        {
            const auto& o = static_cast<const RadioGroupScope&>(old);
            return o.group_value != group_value;
        }
    };

    // -----------------------------------------------------------------------
    // RadioGroup
    // -----------------------------------------------------------------------

    /**
     * @brief Provides a shared selection context for `Radio` widgets.
     *
     * Wrap any subtree containing `Radio` widgets. The `child` can be any
     * layout (Column, Row, ListView, etc.).
     *
     * @code
     * auto group = std::make_shared<RadioGroup>();
     * group->group_value = selected_idx;
     * group->on_changed  = [this](int v) { setState([&]{ selected_idx = v; }); };
     * group->child = [] {
     *     auto col = std::make_shared<Column>();
     *     col->children = {
     *         makeRow(std::make_shared<Radio>(0), std::make_shared<Text>("Option A")),
     *         makeRow(std::make_shared<Radio>(1), std::make_shared<Text>("Option B")),
     *     };
     *     return col;
     * }();
     * @endcode
     */
    class RadioGroup : public StatelessWidget
    {
    public:
        int                       group_value = 0;
        std::function<void(int)>  on_changed;
        WidgetRef                 child;

        WidgetRef build(BuildContext& ctx) const override;
    };

} // namespace systems::leal::campello_widgets
