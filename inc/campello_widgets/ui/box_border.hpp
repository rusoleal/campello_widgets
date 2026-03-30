#pragma once

#include <campello_widgets/ui/color.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A uniform border drawn around a box.
     *
     * All four sides share the same `color` and `width`. For per-side borders
     * extend this type or use the side properties individually.
     */
    struct BoxBorder
    {
        Color color = Color::black();
        float width = 1.0f;

        static constexpr BoxBorder all(Color c, float w = 1.0f) noexcept
        {
            return {c, w};
        }
    };

} // namespace systems::leal::campello_widgets
