#pragma once

#include <campello_widgets/ui/color.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A set of colors designed to work harmoniously together.
     *
     * Each color has a semantic role (primary, surface, error, etc.) and a
     * corresponding "on" color for text/icons drawn on top of it.
     *
     * Design system implementations map these semantic roles to concrete
     * colors. A Material implementation might use bold primaries; a Cupertino
     * implementation might use muted system colors; a custom implementation
     * can assign any palette.
     */
    struct ColorScheme
    {
        Color primary;
        Color on_primary;
        Color secondary;
        Color on_secondary;
        Color surface;
        Color on_surface;
        Color surface_variant;
        Color on_surface_variant;
        Color background;
        Color on_background;
        Color error;
        Color on_error;
        Color success;
        Color on_success;
        Color warning;
        Color on_warning;
        Color outline;
        Color outline_variant;
        Color shadow;
        Color scrim;
        Color inverse_surface;
        Color inverse_on_surface;
        Color inverse_primary;

        bool operator==(const ColorScheme&) const noexcept = default;
        bool operator!=(const ColorScheme&) const noexcept = default;
    };

} // namespace systems::leal::campello_widgets
