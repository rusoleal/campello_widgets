#pragma once

#include <campello_widgets/ui/draw_command.hpp>

#include <array>
#include <cmath>

namespace systems::leal::campello_widgets
{
    /**
     * @brief Corners of a rasterized text quad in physical pixels.
     *
     * The glyph texture is already rasterized at physical-pixel size, so the
     * quad keeps that size but follows the rotation (and mirroring) in
     * `transform`: the corners are the transformed origin plus the texture's
     * width and height along the transform's unit x and y axes. With no
     * rotation this is exactly the old axis-aligned quad.
     *
     * Order: top-left, top-right, bottom-left, bottom-right; `pad` shifts the
     * quad up-left along those axes (the 1 px padding baked into textures).
     */
    struct TextQuadCorner { float x; float y; };

    template <typename Matrix>
    inline std::array<TextQuadCorner, 4> textQuadCorners(
        const Matrix& transform, float origin_x, float origin_y,
        float width, float height, float pad)
    {
        namespace vm = systems::leal::vector_math;
        const auto o  = transform * vm::Vector4<float>(origin_x, origin_y, 0.0f, 1.0f);
        const auto ex = transform * vm::Vector4<float>(1.0f, 0.0f, 0.0f, 0.0f);
        const auto ey = transform * vm::Vector4<float>(0.0f, 1.0f, 0.0f, 0.0f);
        const float det = ex.x() * ey.y() - ex.y() * ey.x();
        const float scale = std::sqrt(std::fabs(det));
        float ux = 1.0f, uy = 0.0f, vx = 0.0f, vy = 1.0f;
        if (scale > 1e-6f)
        {
            ux = ex.x() / scale; uy = ex.y() / scale;
            vx = ey.x() / scale; vy = ey.y() / scale;
        }
        const float x0 = o.x() - pad * (ux + vx);
        const float y0 = o.y() - pad * (uy + vy);
        return {{
            {x0, y0},
            {x0 + ux * width, y0 + uy * width},
            {x0 + vx * height, y0 + vy * height},
            {x0 + ux * width + vx * height, y0 + uy * width + vy * height},
        }};
    }
} // namespace systems::leal::campello_widgets
