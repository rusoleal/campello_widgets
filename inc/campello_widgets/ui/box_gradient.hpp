#pragma once

#include <algorithm>
#include <numbers>
#include <type_traits>
#include <variant>
#include <vector>
#include <campello_widgets/ui/alignment.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/shader.hpp>
#include <campello_widgets/ui/size.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Linear gradient for `BoxDecoration`, defined relative to the box
     * being painted (Flutter's `LinearGradient` semantics) rather than in
     * absolute pixels.
     *
     * `begin`/`end` are resolved against the decorated box's own size each
     * paint via `resolveBoxGradient()`, so one definition adapts automatically
     * when the box is resized.
     */
    struct LinearBoxGradient
    {
        Alignment           begin = Alignment::centerLeft();
        Alignment           end   = Alignment::centerRight();
        std::vector<Color>  colors;
        std::vector<float>  stops;   ///< Optional; must be same size as colors if provided.
        TileMode             tile_mode = TileMode::clamp;

        bool operator==(const LinearBoxGradient&) const = default;
    };

    /**
     * @brief Radial gradient for `BoxDecoration`, defined relative to the box
     * being painted (Flutter's `RadialGradient` semantics).
     *
     * `radius` is a fraction of the box's shortest side (default `0.5`,
     * matching Flutter's default), not a pixel value.
     */
    struct RadialBoxGradient
    {
        Alignment           center = Alignment::center();
        float               radius = 0.5f;
        std::vector<Color>  colors;
        std::vector<float>  stops;
        TileMode             tile_mode = TileMode::clamp;

        bool operator==(const RadialBoxGradient&) const = default;
    };

    /**
     * @brief Sweep (conic) gradient for `BoxDecoration`, defined relative to
     * the box being painted (Flutter's `SweepGradient` semantics).
     *
     * Sweeps clockwise around `center`, from `start_angle` to `end_angle`
     * (radians, 0 = positive x-axis, default full circle).
     */
    struct SweepBoxGradient
    {
        Alignment           center = Alignment::center();
        float               start_angle = 0.0f;
        float               end_angle   = 2.0f * std::numbers::pi_v<float>;
        std::vector<Color>  colors;
        std::vector<float>  stops;
        TileMode             tile_mode = TileMode::clamp;

        bool operator==(const SweepBoxGradient&) const = default;
    };

    /** @brief Tagged union of the gradient types `BoxDecoration::gradient` accepts. */
    using BoxGradient = std::variant<LinearBoxGradient, RadialBoxGradient, SweepBoxGradient>;

    /**
     * @brief Resolves a size-independent `BoxGradient` into the pixel-space
     * `Shader` consumed by `Canvas::beginShaderMask()`, using `box_size` as
     * the frame of reference (0,0 = the box's own top-left).
     */
    inline Shader resolveBoxGradient(const BoxGradient& gradient, Size box_size) noexcept
    {
        return std::visit([&](auto&& g) -> Shader {
            using T = std::decay_t<decltype(g)>;
            if constexpr (std::is_same_v<T, LinearBoxGradient>)
            {
                return LinearGradient{
                    g.begin.inscribe(Size::zero(), box_size),
                    g.end.inscribe(Size::zero(), box_size),
                    g.colors,
                    g.stops,
                    g.tile_mode,
                };
            }
            else if constexpr (std::is_same_v<T, RadialBoxGradient>)
            {
                const float shortest_side = std::min(box_size.width, box_size.height);
                return RadialGradient{
                    g.center.inscribe(Size::zero(), box_size),
                    g.radius * shortest_side,
                    g.colors,
                    g.stops,
                    g.tile_mode,
                };
            }
            else
            {
                return SweepGradient{
                    g.center.inscribe(Size::zero(), box_size),
                    g.start_angle,
                    g.end_angle,
                    g.colors,
                    g.stops,
                    g.tile_mode,
                };
            }
        }, gradient);
    }

} // namespace systems::leal::campello_widgets
