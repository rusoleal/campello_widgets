#pragma once

#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/paint.hpp>

namespace systems::leal::campello_widgets
{
    /**
     * @brief Blends `src` over `dst` using the Porter-Duff Fa/Fb formula for
     * `mode` (`modulate` is the one exception -- not a Porter-Duff formula,
     * a plain component-wise product, matching this codebase's own
     * shader-mask "modulate" blend -- see widgets.metal's
     * shaderMaskFragment). Both inputs and the result are straight
     * (non-premultiplied) alpha, matching Color's own convention; the
     * Fa/Fb math itself runs on premultiplied values, per the standard
     * Porter-Duff derivation. See Paint::color_filter's doc comment for
     * why this is exact everywhere for some modes and only edge-pixel-
     * approximate for others.
     *
     * Shared by `Canvas::resolvePaint()` (Paint::color_filter) and
     * `buildTriangleListVertices()` (Canvas::drawVertices()'s per-vertex
     * blend with Paint::color) -- pulled into its own file, mirroring the
     * `gpu/stroke_geometry.hpp` / `gpu/path_fill_aa.hpp` /
     * `ui/nine_patch_geometry.hpp` split, so both call sites share one
     * implementation instead of duplicating the 13-case switch.
     */
    Color blendColors(const Color& src, const Color& dst, BlendMode mode);
}
