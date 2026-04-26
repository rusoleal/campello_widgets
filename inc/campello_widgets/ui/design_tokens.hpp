#pragma once

#include <campello_widgets/ui/brightness.hpp>
#include <campello_widgets/ui/color_scheme.hpp>
#include <campello_widgets/ui/curves.hpp>
#include <campello_widgets/ui/text_style.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Easing curve function signature.
     *
     * Maps a normalised time t in [0, 1] to an output value in [0, 1].
     */
    using Curve = double (*)(double);

    /**
     * @brief Elevation levels for shadows and depth.
     */
    struct ElevationTokens
    {
        float level0 = 0.0f;
        float level1 = 1.0f;
        float level2 = 3.0f;
        float level3 = 6.0f;
        float level4 = 8.0f;
        float level5 = 12.0f;

        bool operator==(const ElevationTokens&) const noexcept = default;
        bool operator!=(const ElevationTokens&) const noexcept = default;
    };

    /**
     * @brief Corner radius tokens.
     */
    struct ShapeTokens
    {
        float radius_none = 0.0f;
        float radius_xs = 2.0f;
        float radius_sm = 4.0f;
        float radius_md = 8.0f;
        float radius_lg = 12.0f;
        float radius_xl = 16.0f;
        float radius_full = 9999.0f;

        bool operator==(const ShapeTokens&) const noexcept = default;
        bool operator!=(const ShapeTokens&) const noexcept = default;
    };

    /**
     * @brief Spacing tokens for margins, padding, and gaps.
     */
    struct SpacingTokens
    {
        float xs = 4.0f;
        float sm = 8.0f;
        float md = 12.0f;
        float lg = 16.0f;
        float xl = 24.0f;
        float xxl = 32.0f;
        float xxxl = 48.0f;

        bool operator==(const SpacingTokens&) const noexcept = default;
        bool operator!=(const SpacingTokens&) const noexcept = default;
    };

    /**
     * @brief Typography scale for the design system.
     */
    struct TypographyScale
    {
        TextStyle display_large;
        TextStyle display_medium;
        TextStyle display_small;
        TextStyle headline_large;
        TextStyle headline_medium;
        TextStyle headline_small;
        TextStyle title_large;
        TextStyle title_medium;
        TextStyle title_small;
        TextStyle body_large;
        TextStyle body_medium;
        TextStyle body_small;
        TextStyle label_large;
        TextStyle label_medium;
        TextStyle label_small;

        bool operator==(const TypographyScale&) const noexcept = default;
        bool operator!=(const TypographyScale&) const noexcept = default;
    };

    /**
     * @brief Motion tokens for durations and easing curves.
     *
     * Durations are expressed in milliseconds (double).
     */
    struct MotionTokens
    {
        double duration_instant = 0.0;
        double duration_fast = 100.0;
        double duration_normal = 250.0;
        double duration_slow = 400.0;
        double duration_slower = 600.0;

        Curve curve_standard = Curves::easeInOut;
        Curve curve_decelerate = Curves::easeOut;
        Curve curve_accelerate = Curves::easeIn;
        Curve curve_emphasized = Curves::easeInOut;

        bool operator==(const MotionTokens&) const noexcept = default;
        bool operator!=(const MotionTokens&) const noexcept = default;
    };

    /**
     * @brief Complete design token set.
     *
     * Holds all visual primitives (colors, shapes, spacing, typography, motion)
     * that a DesignSystem implementation uses to construct widgets.
     */
    struct DesignTokens
    {
        ColorScheme colors;
        ElevationTokens elevation;
        ShapeTokens shape;
        SpacingTokens spacing;
        TypographyScale typography;
        MotionTokens motion;
        Brightness brightness = Brightness::light;

        bool operator==(const DesignTokens&) const noexcept = default;
        bool operator!=(const DesignTokens&) const noexcept = default;
    };

} // namespace systems::leal::campello_widgets
