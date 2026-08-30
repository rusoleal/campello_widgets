#pragma once

#include <campello_widgets/ui/vertices.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <campello_widgets/ui/draw_command.hpp>
#include <cstdint>
#include <vector>

namespace systems::leal::campello_widgets
{
    /**
     * @brief CPU-only result of expanding a `Vertices` mesh into a form
     * every GPU backend can draw directly: one flat vertex array (same
     * order/length as `Vertices::positions`) plus a flat triangle-list
     * index array (`indices.size() % 3 == 0`).
     */
    struct BuiltVertices
    {
        std::vector<VertexColorPoint> vertices;
        std::vector<uint32_t>         indices;
    };

    /**
     * @brief Pure CPU geometry for `Canvas::drawVertices()` -- split into
     * its own header so it's unit-testable without a GPU-backed `Texture`/
     * `Buffer`, mirroring `gpu/stroke_geometry.hpp` / `gpu/path_fill_aa.hpp`
     * / `ui/nine_patch_geometry.hpp`'s split.
     *
     * Per-vertex color is `v.colors.empty() ? paint_color :
     * blendColors(paint_color, v.colors[i], blend_mode)` -- `paint_color`
     * is the same "src" for every vertex in one `drawVertices()` call, so
     * this blend can be precomputed once per input vertex here and the GPU's
     * ordinary linear interpolation across each triangle reproduces the
     * true per-pixel Porter-Duff blend exactly whenever a triangle's vertex
     * alphas are equal (the common case), and for several blend modes
     * regardless of alpha variation (`srcIn`/`srcOut`/`src`/`dst`/`clear`/
     * `srcOver`/`modulate`) -- see `Canvas::drawVertices()`'s doc comment
     * for the narrow case where this is a smooth-interpolation
     * approximation rather than exact.
     *
     * `mode`/`v.indices` are normalised into a single flat triangle-list
     * index buffer:
     *  - `triangles`: `v.indices` used as-is (or implicit `0..N-1` if
     *    empty), truncated to a multiple of 3 if malformed.
     *  - `triangleStrip`: standard strip-to-list expansion, alternating
     *    winding on odd triangles so front-face orientation stays
     *    consistent.
     *  - `triangleFan`: every triangle shares index 0.
     */
    BuiltVertices buildTriangleListVertices(
        const Vertices& v, const Color& paint_color, BlendMode blend_mode);
}
