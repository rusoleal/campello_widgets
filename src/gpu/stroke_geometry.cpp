#include "gpu/stroke_geometry.hpp"

#include <algorithm>

#include <cmath>

namespace systems::leal::campello_widgets
{

namespace
{

Offset scale(const Offset& o, float s) { return {o.x * s, o.y * s}; }
float  dot(const Offset& a, const Offset& b) { return a.x * b.x + a.y * b.y; }
float  cross(const Offset& a, const Offset& b) { return a.x * b.y - a.y * b.x; }
float  length(const Offset& o) { return std::sqrt(o.x * o.x + o.y * o.y); }

Offset normalized(const Offset& o)
{
    const float len = length(o);
    return (len > 1e-5f) ? scale(o, 1.0f / len) : Offset{1.0f, 0.0f};
}

// 90-degree rotation of a (unit) direction -- used to get a perpendicular
// offset of the same magnitude.
Offset perpOf(const Offset& dir) { return {-dir.y, dir.x}; }

// Appends the join geometry at `curr`, given the (already unit) incoming
// direction (prev -> curr) and outgoing direction (curr -> next).
void addJoin(StrokeGeometry& out, const Offset& curr,
             const Offset& dir_in, const Offset& dir_out,
             float half_width, StrokeJoin join, float miter_limit)
{
    if (join == StrokeJoin::round)
    {
        out.circles.push_back({curr});
        return;
    }

    const Offset perp_in  = perpOf(dir_in);
    const Offset perp_out = perpOf(dir_out);

    // cross(dir_in, dir_out) tells which way the path turns; `side` picks
    // whichever perpendicular sign lands on the *outer* side of that turn,
    // consistently for both segments meeting at `curr`.
    const float turn = cross(dir_in, dir_out);
    const float side = (turn < 0.0f) ? 1.0f : -1.0f;

    const Offset outer_a = curr + scale(perp_in,  side * half_width);
    const Offset outer_b = curr + scale(perp_out, side * half_width);

    if (join == StrokeJoin::bevel || std::abs(turn) < 1e-5f)
    {
        out.wedges.push_back({curr, outer_a, outer_b, false, Offset{}});
        return;
    }

    // Miter: intersection of the two segments' outer offset edges, found via
    // the bisector of perp_in/perp_out. cos_half_angle = cos(theta/2), where
    // theta is the angle between the two segments; miter_length =
    // half_width / cos(theta/2) is the standard miter-join formula.
    const Offset bisector = normalized(perp_in + perp_out);
    const float  cos_half_angle = dot(bisector, perp_in);

    if (std::abs(cos_half_angle) < 1e-4f)
    {
        // Near-180-degree reversal: miter length -> infinity, which always
        // exceeds any real miter_limit -- bevel directly rather than risk a
        // huge/NaN point from dividing by ~0.
        out.wedges.push_back({curr, outer_a, outer_b, false, Offset{}});
        return;
    }

    const float miter_len = half_width / std::abs(cos_half_angle);
    if (miter_len > miter_limit * half_width)
    {
        out.wedges.push_back({curr, outer_a, outer_b, false, Offset{}});
        return;
    }

    const Offset miter_point = curr + scale(bisector, side * miter_len);
    out.wedges.push_back({curr, outer_a, outer_b, true, miter_point});
}

} // namespace

StrokeGeometry buildStrokeGeometry(
    const std::vector<Offset>& raw_points,
    bool closed,
    float half_width,
    StrokeCap cap,
    StrokeJoin join,
    float miter_limit)
{
    StrokeGeometry out;

    // Dedupe consecutive coincident points -- zero-length segments have no
    // well-defined direction and contribute no visible geometry anyway.
    std::vector<Offset> points;
    points.reserve(raw_points.size());
    for (const Offset& p : raw_points)
    {
        if (points.empty() || length(p - points.back()) > 1e-4f)
            points.push_back(p);
    }
    if (closed && points.size() > 1 && length(points.back() - points.front()) <= 1e-4f)
        points.pop_back(); // caller passed an explicit closing duplicate

    const size_t n = points.size();
    if (n < 2) return out;

    const size_t segment_count = closed ? n : (n - 1);

    // Segment bodies, with square-cap endpoint extension at the two open
    // ends (butt/round leave the endpoint untouched; round's extra geometry
    // is added separately below).
    for (size_t i = 0; i < segment_count; ++i)
    {
        Offset p0 = points[i];
        Offset p1 = points[(i + 1) % n];
        const Offset dir = normalized(p1 - p0);

        if (!closed && cap == StrokeCap::square)
        {
            if (i == 0)                 p0 = p0 - scale(dir, half_width);
            if (i == segment_count - 1) p1 = p1 + scale(dir, half_width);
        }
        out.segments.push_back({p0, p1});
    }

    // Joins at every interior vertex (all of them, if closed).
    const size_t join_start = closed ? 0 : 1;
    const size_t join_end   = closed ? n : (n - 1); // exclusive
    for (size_t i = join_start; i < join_end; ++i)
    {
        const Offset& prev = points[(i + n - 1) % n];
        const Offset& curr = points[i];
        const Offset& next = points[(i + 1) % n];
        addJoin(out, curr, normalized(curr - prev), normalized(next - curr),
                half_width, join, miter_limit);
    }

    // Caps at the two open ends. Square is handled above (segment
    // extension); butt needs no extra geometry.
    if (!closed && cap == StrokeCap::round)
    {
        out.circles.push_back({points.front()});
        out.circles.push_back({points.back()});
    }

    return out;
}

std::vector<FillAAVertex> buildStrokeAAVertices(
    const std::vector<Offset>& points,
    bool closed,
    float half_width,
    StrokeCap cap,
    StrokeJoin join,
    float miter_limit,
    float aa_width_local)
{
    const StrokeGeometry geo = buildStrokeGeometry(points, closed, half_width, cap, join, miter_limit);
    const float aa = aa_width_local > 0.0f ? aa_width_local : 1.0f;
    std::vector<FillAAVertex> out;
    out.reserve(geo.segments.size() * 18 + geo.circles.size() * 24 * 9 + geo.wedges.size() * 6);
    const auto tri = [&out](Offset a, float aa_a, Offset b, float aa_b, Offset c, float aa_c) {
        out.push_back({a, aa_a});
        out.push_back({b, aa_b});
        out.push_back({c, aa_c});
    };
    const auto quad = [&tri](Offset a, float alpha_a, Offset b, float alpha_b, Offset c, float alpha_c, Offset d, float alpha_d) {
        // a-b-c-d in order round the quad.
        tri(a, alpha_a, b, alpha_b, c, alpha_c);
        tri(a, alpha_a, c, alpha_c, d, alpha_d);
    };

    // The fringe straddles the true edge (half a pixel in, half out), so
    // the stroke keeps its width; a stroke under a pixel wide keeps a
    // pixel-wide core at its coverage instead (a hairline stays a hairline,
    // fainter, rather than vanishing or thickening).
    const float inner     = std::max(0.0f, half_width - aa * 0.5f);
    const float outer     = std::max(half_width + aa * 0.5f, inner + aa);
    const float coreAlpha = std::min(1.0f, (2.0f * half_width) / aa);

    // Segment bodies: the core, then a fringe each side.
    for (const auto& seg : geo.segments)
    {
        const float dx  = seg.p1.x - seg.p0.x;
        const float dy  = seg.p1.y - seg.p0.y;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-6f) continue;
        const float nx = -dy / len;
        const float ny =  dx / len;
        const auto off = [&](const Offset& p, float d) { return Offset{p.x + nx * d, p.y + ny * d}; };
        const Offset in0 = off(seg.p0,  inner), in1 = off(seg.p1,  inner);
        const Offset on0 = off(seg.p0, -inner), on1 = off(seg.p1, -inner);
        quad(on0, coreAlpha, in0, coreAlpha, in1, coreAlpha, on1, coreAlpha);
        const Offset fo0 = off(seg.p0,  outer), fo1 = off(seg.p1,  outer);
        quad(in0, coreAlpha, fo0, 0.0f, fo1, 0.0f, in1, coreAlpha);
        const Offset gn0 = off(seg.p0, -outer), gn1 = off(seg.p1, -outer);
        quad(on0, coreAlpha, on1, coreAlpha, gn1, 0.0f, gn0, 0.0f);
    }

    // Round caps and joins: a disc and a fading ring, enough sides to look
    // round at the size (at least 12).
    const float kPi = 3.14159265358979323846f;
    const float radius_px = half_width / aa;
    const int sides = std::max(12, std::min(64, static_cast<int>(std::ceil(radius_px * 2.0f * kPi / 3.0f))));
    for (const auto& circ : geo.circles)
    {
        for (int i = 0; i < sides; ++i)
        {
            const float a0 = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(sides);
            const float a1 = 2.0f * kPi * static_cast<float>(i + 1) / static_cast<float>(sides);
            const Offset d0{std::cos(a0), std::sin(a0)};
            const Offset d1{std::cos(a1), std::sin(a1)};
            const Offset r0{circ.center.x + d0.x * inner, circ.center.y + d0.y * inner};
            const Offset r1{circ.center.x + d1.x * inner, circ.center.y + d1.y * inner};
            const Offset f0{circ.center.x + d0.x * outer, circ.center.y + d0.y * outer};
            const Offset f1{circ.center.x + d1.x * outer, circ.center.y + d1.y * outer};
            tri(circ.center, coreAlpha, r0, coreAlpha, r1, coreAlpha);
            quad(r0, coreAlpha, f0, 0.0f, f1, 0.0f, r1, coreAlpha);
        }
    }

    // Bevel/miter wedges: solid (they sit inside the segments' fringes).
    for (const auto& w : geo.wedges)
    {
        if (w.has_miter_point)
        {
            tri(w.hub, coreAlpha, w.outer_a, coreAlpha, w.miter_point, coreAlpha);
            tri(w.hub, coreAlpha, w.miter_point, coreAlpha, w.outer_b, coreAlpha);
        }
        else
        {
            tri(w.hub, coreAlpha, w.outer_a, coreAlpha, w.outer_b, coreAlpha);
        }
    }
    return out;
}

} // namespace systems::leal::campello_widgets
