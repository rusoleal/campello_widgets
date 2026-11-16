#pragma once

#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/paint.hpp>
#include "gpu/path_fill_aa.hpp"
#include <vector>

namespace systems::leal::campello_widgets
{

    /** @brief One straight stroke segment body -- a butt-ended box, `half_width` wide. */
    struct StrokeSegment
    {
        Offset p0, p1;
    };

    /** @brief A round cap or round join, rendered as a filled circle of `half_width` radius. */
    struct StrokeCircle
    {
        Offset center;
    };

    /**
     * @brief A flat bevel or miter join wedge, as a fan of 1-2 triangles from
     * `hub` (the shared vertex) through `outer_a` -> (`miter_point`, if
     * `has_miter_point`) -> `outer_b`.
     */
    struct StrokeWedge
    {
        Offset hub;
        Offset outer_a;
        Offset outer_b;
        bool   has_miter_point = false;
        Offset miter_point;
    };

    /** @brief GPU-ready decomposition of a stroked polyline. */
    struct StrokeGeometry
    {
        std::vector<StrokeSegment> segments;
        std::vector<StrokeCircle>  circles; ///< round caps + round joins
        std::vector<StrokeWedge>   wedges;  ///< bevel/miter joins
    };

    /**
     * @brief Decomposes a polyline stroke into GPU-ready primitives: caps
     * (round/square/butt) at the two open ends (skipped if `closed`), and
     * joins (round/bevel/miter, with miter-limit fallback to bevel) at every
     * interior vertex (including the wrap-around vertex if `closed`).
     *
     * Pure CPU vector math, O(n) in point count -- no GPU/backend
     * dependency, shared by all 3 backends. `half_width` must already be in
     * the same (target) space as `points`. `points` should list distinct
     * vertices; an explicit closing duplicate (last == first) is tolerated
     * when `closed` is true. Consecutive duplicate points are skipped
     * (zero-length segments contribute no geometry).
     */
    StrokeGeometry buildStrokeGeometry(
        const std::vector<Offset>& points,
        bool closed,
        float half_width,
        StrokeCap cap,
        StrokeJoin join,
        float miter_limit);

    /**
     * @brief An antialiased stroke as coverage triangles (FillAAVertex:
     * position, alpha), for backends to draw with their fill-skirt
     * (per-vertex alpha) pipeline -- stroked paths in one draw call, like
     * the old flat batched triangles, but with soft edges.
     *
     * From buildStrokeGeometry(): each segment body is a solid core
     * (alpha 1) plus a fringe `aa_width_local` wide each side, straddling
     * the true edge (`half_width` from the centre line), fading to alpha 0,
     * so the stroke keeps its width; each round cap or join a solid disc
     * plus a fading ring; bevel/miter wedges solid. Under a pixel wide, the
     * core is a pixel wide at the stroke's coverage (a fainter hairline). Where
     * parts overlap (joins) the fringes blend twice: a slightly firmer edge
     * there, never a hole. `aa_width_local` is one device pixel in the
     * points' space (1 / the transform's scale).
     */
    std::vector<FillAAVertex> buildStrokeAAVertices(
        const std::vector<Offset>& points,
        bool closed,
        float half_width,
        StrokeCap cap,
        StrokeJoin join,
        float miter_limit,
        float aa_width_local);

} // namespace systems::leal::campello_widgets
