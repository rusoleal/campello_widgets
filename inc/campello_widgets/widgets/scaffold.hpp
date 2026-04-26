#pragma once

#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/ui/color.hpp>

#include <optional>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A basic full-screen layout with a background color and a body widget.
     *
     * Phase 6 scaffold: fills all available space with `background_color` and
     * draws `body` on top. Future phases will add an app bar, bottom navigation,
     * drawers, and floating action buttons.
     */
    class Scaffold : public StatelessWidget
    {
    public:
        WidgetRef child;
        std::optional<Color> background_color;

        Scaffold() = default;
        explicit Scaffold(WidgetRef c)
            : child(std::move(c))
        {}
        explicit Scaffold(Color bg, WidgetRef c)
            : child(std::move(c)), background_color(bg)
        {}

        WidgetRef build(BuildContext& context) const override;
    };

} // namespace systems::leal::campello_widgets
