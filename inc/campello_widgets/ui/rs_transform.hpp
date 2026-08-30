#pragma once

#include <cmath>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Rotated/scaled/translated placement for one `Canvas::drawAtlas()`
     * sprite -- matches Flutter's `RSTransform`.
     *
     * Represents the affine map `x' = scos*x - ssin*y + tx`,
     * `y' = ssin*x + scos*y + ty` applied to the sprite's own local
     * `[0,rect.width] x [0,rect.height]` box (top-left origin -- any pivot
     * offset is baked into `tx`/`ty` by `fromComponents()`, matching
     * Flutter's own factory).
     */
    struct RSTransform
    {
        float scos = 1.0f;
        float ssin = 0.0f;
        float tx   = 0.0f;
        float ty   = 0.0f;

        /**
         * @brief Builds an `RSTransform` from rotation/scale/anchor/
         * translation components -- matches Flutter's
         * `RSTransform.fromComponents()`.
         *
         * @param rotation    Radians, applied around (anchorX, anchorY).
         * @param scale       Uniform scale factor.
         * @param anchorX     Pivot X, in the sprite's own local (pre-
         *                    transform) coordinates.
         * @param anchorY     Pivot Y, in the sprite's own local (pre-
         *                    transform) coordinates.
         * @param translateX  Final screen-space X of the anchor point.
         * @param translateY  Final screen-space Y of the anchor point.
         */
        static RSTransform fromComponents(
            float rotation, float scale, float anchorX, float anchorY,
            float translateX, float translateY)
        {
            const float scos = scale * std::cos(rotation);
            const float ssin = scale * std::sin(rotation);
            return RSTransform{
                scos, ssin,
                translateX - scos * anchorX + ssin * anchorY,
                translateY - ssin * anchorX - scos * anchorY,
            };
        }
    };

} // namespace systems::leal::campello_widgets
