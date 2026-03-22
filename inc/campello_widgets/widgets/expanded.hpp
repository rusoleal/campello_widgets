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
        explicit Expanded(WidgetRef child_widget)
        {
            flex  = 1;
            child = std::move(child_widget);
        }
    };

} // namespace systems::leal::campello_widgets
