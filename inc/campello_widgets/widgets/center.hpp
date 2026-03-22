#pragma once

#include <campello_widgets/widgets/align.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Centers its child within itself.
     *
     * A convenience subclass of `Align` with `alignment = Alignment::center()`.
     */
    class Center : public Align
    {
    public:
        explicit Center(WidgetRef child_widget)
        {
            alignment = Alignment::center();
            child     = std::move(child_widget);
        }
    };

} // namespace systems::leal::campello_widgets
