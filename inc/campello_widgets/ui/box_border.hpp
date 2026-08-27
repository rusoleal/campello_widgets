#pragma once

#include <optional>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/box_gradient.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A uniform border drawn around a box.
     *
     * All four sides share the same `color` (or `gradient`) and `width`. For
     * per-side borders extend this type or use the side properties
     * individually.
     *
     * If both `color` and `gradient` are set, `gradient` takes precedence
     * (matching `BoxDecoration`'s own `color`/`gradient` precedence).
     */
    struct BoxBorder
    {
        Color                       color = Color::black();
        std::optional<BoxGradient>  gradient;
        float                       width = 1.0f;

        static constexpr BoxBorder all(Color c, float w = 1.0f) noexcept
        {
            return {c, std::nullopt, w};
        }

        static BoxBorder gradientBorder(BoxGradient g, float w = 1.0f) noexcept
        {
            return {Color::black(), std::move(g), w};
        }

        bool operator==(const BoxBorder&) const noexcept = default;
    };

} // namespace systems::leal::campello_widgets
