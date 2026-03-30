#pragma once

#include <optional>
#include <vector>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/box_border.hpp>
#include <campello_widgets/ui/box_shadow.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Immutable description of how to paint a box.
     *
     * Supports a solid background colour, uniform corner radius, a uniform
     * border, and a list of box shadows. Gradient support is deferred until
     * the canvas layer exposes a shader API.
     *
     * @code
     * BoxDecoration{
     *     .color         = Color::fromRGB(0.2f, 0.5f, 0.9f),
     *     .border_radius = 8.0f,
     *     .border        = BoxBorder::all(Color::black(), 2.0f),
     *     .box_shadow    = { BoxShadow{Color::black(), {2,4}, 8.0f} },
     * }
     * @endcode
     */
    struct BoxDecoration
    {
        /** Solid background fill colour. No fill if unset. */
        std::optional<Color>     color;

        /** Uniform corner radius (logical pixels). 0 = sharp corners. */
        float                    border_radius = 0.0f;

        /** Uniform border drawn on top of the background. No border if unset. */
        std::optional<BoxBorder> border;

        /** Shadows painted below the background. Empty = no shadow. */
        std::vector<BoxShadow>   box_shadow;
    };

} // namespace systems::leal::campello_widgets
