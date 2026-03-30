#pragma once

namespace systems::leal::campello_widgets
{

    /**
     * @brief Describes a per-pixel image filter applied to a compositing layer.
     *
     * Currently only Gaussian blur is supported. Both axes default to the same
     * sigma so callers can use the single-argument factory for isotropic blur.
     *
     * @code
     * ImageFilter::blur(8.0f)           // uniform 8-pixel blur
     * ImageFilter::blur(12.0f, 6.0f)    // wider horizontal blur
     * @endcode
     */
    struct ImageFilter
    {
        float sigma_x = 4.0f;  ///< Horizontal blur radius (Gaussian sigma, pixels).
        float sigma_y = 4.0f;  ///< Vertical   blur radius (Gaussian sigma, pixels).

        /** Construct an isotropic Gaussian blur with the given sigma. */
        static ImageFilter blur(float sigma) noexcept
        {
            return { sigma, sigma };
        }

        /** Construct an anisotropic Gaussian blur with separate per-axis sigmas. */
        static ImageFilter blur(float sigma_x, float sigma_y) noexcept
        {
            return { sigma_x, sigma_y };
        }
    };

} // namespace systems::leal::campello_widgets
