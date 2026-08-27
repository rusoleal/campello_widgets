#pragma once

#include <optional>
#include <vector>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/box_border.hpp>
#include <campello_widgets/ui/box_gradient.hpp>
#include <campello_widgets/ui/box_shadow.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Immutable description of how to paint a box.
     *
     * Supports a solid background colour or gradient, uniform corner radius,
     * a uniform border, and a list of box shadows.
     *
     * If both `color` and `gradient` are set, `gradient` takes precedence for
     * the background fill (matching Flutter's `BoxDecoration`).
     *
     * @code
     * BoxDecoration{
     *     .gradient      = LinearBoxGradient{
     *                          .begin  = Alignment::topLeft(),
     *                          .end    = Alignment::bottomRight(),
     *                          .colors = {Color::fromRGB(0.2f, 0.5f, 0.9f), Color::white()},
     *                      },
     *     .border_radius = 8.0f,
     *     .border        = BoxBorder::all(Color::black(), 2.0f),
     *     .box_shadow    = { BoxShadow{Color::black(), {2,4}, 8.0f} },
     * }
     * @endcode
     */
    struct BoxDecoration
    {
        /** Solid background fill colour. Ignored if `gradient` is set. */
        std::optional<Color>       color;

        /** Gradient background fill. Takes precedence over `color` if set. */
        std::optional<BoxGradient> gradient;

        /** Uniform corner radius (logical pixels). 0 = sharp corners. */
        float                      border_radius = 0.0f;

        /** Uniform border drawn on top of the background. No border if unset. */
        std::optional<BoxBorder>   border;

        /** Shadows painted below the background. Empty = no shadow. */
        std::vector<BoxShadow>     box_shadow;

        bool operator==(const BoxDecoration&) const noexcept = default;
    };

} // namespace systems::leal::campello_widgets
