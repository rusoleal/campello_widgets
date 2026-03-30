#pragma once

#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/offset.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A shadow cast by a box.
     *
     * `offset`        — displacement of the shadow from the box.
     * `blur_radius`   — standard deviation of the Gaussian blur; maps to
     *                   Material elevation for the underlying canvas primitive.
     * `spread_radius` — amount the shadow should expand before blurring.
     *                   Positive values enlarge, negative values shrink.
     * `color`         — colour of the shadow.
     */
    struct BoxShadow
    {
        Color  color         = Color::black();
        Offset offset        = {0.0f, 0.0f};
        float  blur_radius   = 0.0f;
        float  spread_radius = 0.0f;
    };

} // namespace systems::leal::campello_widgets
