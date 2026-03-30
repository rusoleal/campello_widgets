#pragma once

#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/offset.hpp>
#include <variant>
#include <vector>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Linear gradient shader used as a mask source in ShaderMask.
     *
     * The gradient is evaluated along the line from `begin` to `end` in the
     * local coordinate space of the ShaderMask widget.  Colors are sampled
     * at normalized positions in `stops` (must be the same length as `colors`
     * and in non-decreasing order in [0, 1]).
     *
     * If `stops` is empty, colors are evenly spaced across [0, 1].
     */
    struct LinearGradient
    {
        Offset             begin;
        Offset             end;
        std::vector<Color> colors;
        std::vector<float> stops;   ///< Optional; must be same size as colors if provided.
    };

    /**
     * @brief Radial gradient shader used as a mask source in ShaderMask.
     *
     * The gradient is evaluated as the distance from `center` divided by
     * `radius`.  Values beyond the radius clamp to the last color.
     */
    struct RadialGradient
    {
        Offset             center;
        float              radius;
        std::vector<Color> colors;
        std::vector<float> stops;   ///< Optional.
    };

    /**
     * @brief Tagged union of all supported gradient/shader types.
     *
     * Used as the mask source in @ref ShaderMask.  Additional types (sweep
     * gradient, image shader, custom shader) may be added in later phases.
     */
    using Shader = std::variant<LinearGradient, RadialGradient>;

} // namespace systems::leal::campello_widgets
