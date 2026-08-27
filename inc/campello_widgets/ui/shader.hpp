#pragma once

#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/offset.hpp>
#include <numbers>
#include <variant>
#include <vector>

namespace systems::leal::campello_widgets
{

    /**
     * @brief How a gradient behaves outside its defined range ([0, 1] for
     * linear/radial `t`, `[start_angle, end_angle]` for sweep).
     *
     * Mirrors Flutter's `TileMode` (its `decal` variant is image-shader-only
     * and has no gradient equivalent here).
     */
    enum class TileMode
    {
        clamp,     ///< Extends the edge colors past the defined range (default).
        repeated,  ///< Repeats the gradient past the defined range.
        mirror,    ///< Repeats the gradient, alternating direction each repeat.
    };

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
        TileMode           tile_mode = TileMode::clamp;
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
        TileMode           tile_mode = TileMode::clamp;
    };

    /**
     * @brief Sweep (conic) gradient shader used as a mask source in ShaderMask.
     *
     * The gradient sweeps clockwise around `center`, starting at `start_angle`
     * and ending at `end_angle` (radians, 0 = positive x-axis). Colors are
     * sampled at normalized positions in `stops`, same convention as
     * `LinearGradient`.
     */
    struct SweepGradient
    {
        Offset             center;
        float              start_angle = 0.0f;
        float              end_angle   = 2.0f * std::numbers::pi_v<float>;
        std::vector<Color> colors;
        std::vector<float> stops;
        TileMode           tile_mode = TileMode::clamp;
    };

    /**
     * @brief Tagged union of all supported gradient/shader types.
     *
     * Used as the mask source in @ref ShaderMask.  Additional types (image
     * shader, custom shader) may be added in later phases.
     */
    using Shader = std::variant<LinearGradient, RadialGradient, SweepGradient>;

} // namespace systems::leal::campello_widgets
