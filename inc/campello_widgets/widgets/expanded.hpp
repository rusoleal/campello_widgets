#pragma once

#include <campello_widgets/widgets/flexible.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A Flexible child that expands to fill the available space.
     *
     * Equivalent to `Flexible` with `flex = 1`. Place inside a Row or Column
     * to absorb all remaining main-axis space.
     */
    class Expanded : public Flexible
    {
    public:
        Expanded() { flex = 1; }

        explicit Expanded(WidgetRef child_widget)
        {
            flex  = 1;
            child = std::move(child_widget);
        }

        explicit Expanded(int flex_value, WidgetRef child_widget)
        {
            flex  = flex_value;
            child = std::move(child_widget);
        }
    };

} // namespace systems::leal::campello_widgets
