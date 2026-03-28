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
        Center() { alignment = Alignment::center(); }

        explicit Center(WidgetRef child_widget)
        {
            alignment = Alignment::center();
            child     = std::move(child_widget);
        }

        // Factory method
        static std::shared_ptr<Center> create(WidgetRef child = nullptr) {
            auto c = std::make_shared<Center>();
            c->alignment = Alignment::center();
            c->child = std::move(child);
            return c;
        }
    };

} // namespace systems::leal::campello_widgets
