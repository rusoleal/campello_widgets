#include "gpu/stroke_geometry.hpp"

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

} // namespace systems::leal::campello_widgets
