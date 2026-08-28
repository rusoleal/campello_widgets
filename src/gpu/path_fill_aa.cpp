#include "gpu/path_fill_aa.hpp"

#include <cmath>

namespace systems::leal::campello_widgets
{

// Not in an anonymous namespace: Unity Build (CMAKE_UNITY_BUILD) concatenates
// this file with stroke_geometry.cpp into one translation unit, and both
// files' anonymous namespaces collapse into the *same* unnamed namespace
// there -- an identically-named helper (e.g. `scale`/`dot`/`normalized`) in
// both would be a hard redefinition error. Suffixed names instead, kept
// static for the same internal-linkage/no-symbol-leak effect.

namespace
{

Offset scaleAA(const Offset& o, float s) { return {o.x * s, o.y * s}; }
float  dotAA(const Offset& a, const Offset& b) { return a.x * b.x + a.y * b.y; }
float  lengthAA(const Offset& o) { return std::sqrt(o.x * o.x + o.y * o.y); }

Offset normalizedAA(const Offset& o)
{
    const float len = lengthAA(o);
    return (len > 1e-5f) ? scaleAA(o, 1.0f / len) : Offset{1.0f, 0.0f};
}

// 90-degree rotation of a (unit) direction -- see stroke_geometry.cpp's
// identical helper; the sign convention only matters relative to itself
// (via the `side` derived from signed_area below), not in any absolute
// sense.
Offset perpOfAA(const Offset& dir) { return {-dir.y, dir.x}; }

} // namespace

std::vector<FillAAVertex> buildFillAASkirt(
    const std::vector<Offset>& raw_points, float aa_width_local)
{
    std::vector<FillAAVertex> out;

    // Dedupe consecutive coincident points and a trailing explicit closing
    // duplicate -- same tolerance/approach as buildStrokeGeometry().
    std::vector<Offset> points;
    points.reserve(raw_points.size());
    for (const Offset& p : raw_points)
    {
        if (points.empty() || lengthAA(p - points.back()) > 1e-4f)
            points.push_back(p);
    }
    if (points.size() > 1 && lengthAA(points.back() - points.front()) <= 1e-4f)
        points.pop_back();

    const size_t n = points.size();
    if (n < 3) return out;

    // Signed area determines which perpendicular sign points *outward*
    // (away from the polygon's own interior) -- same formula
    // triangulateContour() uses internally (path_tessellation.cpp), so
    // `side` stays consistent with whatever winding a given contour
    // happens to have, without needing to know which visual direction
    // "positive" corresponds to in this (y-down) coordinate space.
    float signed_area = 0.0f;
    for (size_t i = 0; i < n; ++i)
    {
        const Offset& a = points[i];
        const Offset& b = points[(i + 1) % n];
        signed_area += (a.x * b.y - b.x * a.y);
    }
    const float side = (signed_area > 0.0f) ? -1.0f : 1.0f;

    // Bounds the bisector extrusion length at sharp corners -- unlike a
    // user-controlled stroke width, this band is always ~1px, so a slightly
    // longer extrusion at a very acute corner is an invisible cosmetic
    // rounding rather than something that needs a true miter-limit fallback.
    constexpr float kMaxExtrudeScale = 4.0f;

    // One outward-extruded point per vertex, shared by the two skirt quads
    // (this vertex's incoming and outgoing edges) that meet there.
    std::vector<Offset> outer(n);
    for (size_t i = 0; i < n; ++i)
    {
        const Offset& prev = points[(i + n - 1) % n];
        const Offset& curr = points[i];
        const Offset& next = points[(i + 1) % n];

        const Offset dir_in   = normalizedAA(curr - prev);
        const Offset dir_out  = normalizedAA(next - curr);
        const Offset perp_in  = perpOfAA(dir_in);
        const Offset perp_out = perpOfAA(dir_out);

        // Same bisector/miter-length construction buildStrokeGeometry()'s
        // addJoin() uses for a stroke's miter point, just with a fixed,
        // bounded extrusion instead of a user-configurable miter limit.
        const Offset bisector       = normalizedAA(perp_in + perp_out);
        const float  cos_half_angle = dotAA(bisector, perp_in);

        float extrude_len = aa_width_local;
        if (std::abs(cos_half_angle) > 1e-4f)
        {
            extrude_len = std::min(
                aa_width_local / std::abs(cos_half_angle),
                aa_width_local * kMaxExtrudeScale);
        }

        outer[i] = curr + scaleAA(bisector, side * extrude_len);
    }

    // Two triangles per edge, connecting that edge's two (alpha=1) inner
    // points to the corresponding two (alpha=0) outer points.
    out.reserve(n * 6);
    for (size_t i = 0; i < n; ++i)
    {
        const size_t j = (i + 1) % n;
        const Offset& in0  = points[i];
        const Offset& in1  = points[j];
        const Offset& out0 = outer[i];
        const Offset& out1 = outer[j];

        out.push_back({in0, 1.0f});
        out.push_back({in1, 1.0f});
        out.push_back({out1, 0.0f});

        out.push_back({in0, 1.0f});
        out.push_back({out1, 0.0f});
        out.push_back({out0, 0.0f});
    }

    return out;
}

} // namespace systems::leal::campello_widgets
