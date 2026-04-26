#pragma once

#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/ui/color.hpp>

#include <optional>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A circular radio button that reads its selection state from the
     *        nearest `RadioGroup` ancestor.
     *
     * Must be placed inside a `RadioGroup`. Tapping fires the group's
     * `on_changed` callback with this radio's `value`.
     *
     * Pair with a `Text` widget (in a `Row`) to produce a labelled option.
     *
     * @code
     * // Inside a RadioGroup's child subtree:
     * auto row = std::make_shared<Row>();
     * row->children = {
     *     std::make_shared<Radio>(0),
     *     std::make_shared<Text>("Option A"),
     * };
     * @endcode
     */
    class Radio : public StatelessWidget
    {
    public:
        int   value  = 0; ///< The value this radio represents in the group
        float size   = 20.0f;

        std::optional<Color> active_color;
        std::optional<Color> inactive_color;

        explicit Radio(int v = 0) : value(v) {}

        WidgetRef build(BuildContext& ctx) const override;
    };

} // namespace systems::leal::campello_widgets
