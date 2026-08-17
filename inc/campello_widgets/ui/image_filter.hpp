#pragma once

#include <campello_widgets/ui/color.hpp>

namespace systems::leal::campello_widgets
{

    /** @brief Which effect an ImageFilter describes. */
    enum class ImageFilterKind
    {
        gaussianBlur,
        liquidGlass,
    };

    /**
     * @brief Describes a per-pixel image filter applied to a compositing layer.
     *
     * A single struct rather than a `std::variant` deliberately: a backend
     * that doesn't implement `liquidGlass` yet can still do something
     * reasonable by just reading `sigma_x`/`sigma_y` and blurring, since
     * those fields mean the same thing for both kinds (the frosted base
     * layer liquid glass refracts). A variant would force every backend to
     * explicitly handle (or reject) the new alternative before it compiles.
     *
     * @code
     * ImageFilter::blur(8.0f)                          // uniform 8px Gaussian blur
     * ImageFilter::blur(12.0f, 6.0f)                    // anisotropic blur
     * ImageFilter::liquidGlass(16.0f)                   // rounded-rect glass, corner radius 16px
     * @endcode
     */
    struct ImageFilter
    {
        ImageFilterKind kind = ImageFilterKind::gaussianBlur;

        float sigma_x = 4.0f;  ///< Horizontal blur radius (Gaussian sigma, pixels).
        float sigma_y = 4.0f;  ///< Vertical   blur radius (Gaussian sigma, pixels).

        // --- liquidGlass-only parameters -----------------------------------
        // Ignored by gaussianBlur, and by any backend that hasn't implemented
        // liquidGlass yet (which just blurs with sigma_x/sigma_y above — a
        // reasonable frosted-glass fallback, not a broken one).

        /// Corner radius (pixels) of the rounded-rect the effect is shaped
        /// to. Clamped to min(width, height)/2 at render time, so this same
        /// field covers a sharp rect (0), a rounded card, a capsule/pill,
        /// and a circle — no separate shape enum needed.
        float corner_radius = 16.0f;

        /// Straight-alpha tint blended over the (blurred, saturated)
        /// backdrop — the "tinted overlay" ingredient of the effect.
        Color tint = Color::fromRGBA(1.0f, 1.0f, 1.0f, 0.14f);

        /// Max UV offset (pixels) applied to the backdrop sample right at
        /// the shape's rim, tapering to 0 toward the interior — this is
        /// what makes the edge visually bend/refract the content behind it.
        float refraction_strength = 10.0f;

        /// Intensity ([0, 1]) of the specular rim highlight.
        float specular_intensity = 0.5f;

        /** Construct an isotropic Gaussian blur with the given sigma. */
        static constexpr ImageFilter blur(float sigma) noexcept
        {
            ImageFilter f{};
            f.kind    = ImageFilterKind::gaussianBlur;
            f.sigma_x = sigma;
            f.sigma_y = sigma;
            return f;
        }

        /** Construct an anisotropic Gaussian blur with separate per-axis sigmas. */
        static constexpr ImageFilter blur(float sigma_x, float sigma_y) noexcept
        {
            ImageFilter f{};
            f.kind    = ImageFilterKind::gaussianBlur;
            f.sigma_x = sigma_x;
            f.sigma_y = sigma_y;
            return f;
        }

        /**
         * @brief Construct a Liquid Glass filter shaped to a rounded rect.
         *
         * @param corner_radius        Pixels; see the field's doc comment.
         * @param tint                 Straight-alpha overlay color.
         * @param blur_sigma           Pre-refraction backdrop blur (Gaussian sigma, pixels).
         * @param refraction_strength  Max rim UV offset, pixels.
         * @param specular_intensity   Rim highlight intensity, [0, 1].
         */
        static constexpr ImageFilter liquidGlass(
            float corner_radius,
            Color tint                = Color::fromRGBA(1.0f, 1.0f, 1.0f, 0.14f),
            float blur_sigma          = 40.0f,
            float refraction_strength = 10.0f,
            float specular_intensity  = 0.8f) noexcept
        {
            ImageFilter f{};
            f.kind                = ImageFilterKind::liquidGlass;
            f.sigma_x             = blur_sigma;
            f.sigma_y             = blur_sigma;
            f.corner_radius       = corner_radius;
            f.tint                = tint;
            f.refraction_strength = refraction_strength;
            f.specular_intensity  = specular_intensity;
            return f;
        }

        bool operator==(const ImageFilter&) const noexcept = default;
        bool operator!=(const ImageFilter&) const noexcept = default;
    };

} // namespace systems::leal::campello_widgets
