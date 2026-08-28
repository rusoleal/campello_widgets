#pragma once

#include <campello_widgets/ui/offset.hpp>
#include <vector>

namespace systems::leal::campello_widgets
{

    /**
     * @brief One vertex of an antialiasing "skirt" quad -- see
     * buildFillAASkirt()'s doc comment.
     */
    struct FillAAVertex
    {
        Offset pos;
        float  alpha = 1.0f;
    };

    /**
     * @brief Builds an outward-only antialiasing "skirt" for a closed,
     * filled polygon contour -- a thin band of triangles hugging the
     * contour's boundary, with vertex alpha fading from 1.0 on the true
     * edge to 0.0 at `aa_width_local` further out. The GPU's own triangle
     * rasterizer linearly interpolates that alpha across each skirt
     * triangle, softening what would otherwise be a hard, jagged
     * (ear-clip-triangulated) silhouette into an antialiased one -- the
     * same technique NanoVG and similar 2D vector renderers use in place of
     * MSAA.
     *
     * The skirt sits *entirely outside* the polygon's own interior fill
     * (its alpha=1.0 inner edge is exactly the interior triangulation's own
     * boundary, zero overlap) -- safe to draw on top of that interior fill
     * for any paint alpha or blend mode, unlike a naive "stroke the outline
     * in the same color" approach, which would double-blend the overlap
     * band for anything other than a fully opaque srcOver paint.
     *
     * Pure CPU vector math, O(n) in point count -- no GPU/backend
     * dependency, shared by all 3 backends (mirrors stroke_geometry.hpp's
     * role for the analogous stroke-side problem).
     *
     * `raw_points` is one contour's flattened boundary, in the same
     * (already-transformed, if the caller pre-transforms per-vertex like
     * drawPath()'s existing fill/stroke code does) space as the interior
     * triangulation; an explicit closing duplicate (last == first) is
     * tolerated, and consecutive duplicate points are skipped. Contours
     * with fewer than 3 distinct points produce no geometry.
     *
     * `aa_width_local` is the desired AA band thickness, measured in the
     * *same units as `raw_points`* -- since the AA fringe must always be
     * ~1 *screen* pixel wide regardless of any ambient scale, and this
     * function has no notion of a transform, callers working in
     * pre-transform (local) space must pre-divide by the transform's scale
     * factor (`1.0f / scale`, the same `scale` extraction already used
     * throughout the backends, e.g. for `drawLine`'s stroke width) before
     * calling this; callers that transform points to screen space first
     * (as `drawPath()`'s existing per-vertex transform does) can just pass
     * a fixed screen-pixel width (e.g. `1.0f`) directly.
     */
    std::vector<FillAAVertex> buildFillAASkirt(
        const std::vector<Offset>& raw_points, float aa_width_local);

} // namespace systems::leal::campello_widgets
