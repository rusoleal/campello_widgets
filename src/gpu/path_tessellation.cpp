#include "gpu/path_tessellation.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace systems::leal::campello_widgets
{

namespace
{

float pathCross2d(const PathTessVertex& a, const PathTessVertex& b, const PathTessVertex& c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

bool pointInTriangle(const PathTessVertex& p, const PathTessVertex& a,
                     const PathTessVertex& b, const PathTessVertex& c)
{
    float d1 = pathCross2d(a, b, p);
    float d2 = pathCross2d(b, c, p);
    float d3 = pathCross2d(c, a, p);
    bool has_neg = (d1 < 0.0f) || (d2 < 0.0f) || (d3 < 0.0f);
    bool has_pos = (d1 > 0.0f) || (d2 > 0.0f) || (d3 > 0.0f);
    return !(has_neg && has_pos);
}

} // namespace

void flattenQuadBezier(const Offset& p0, const Offset& cp, const Offset& p1,
                       std::vector<PathTessVertex>& out)
{
    const float tol = 0.25f;
    float dx = p0.x - 2.0f * cp.x + p1.x;
    float dy = p0.y - 2.0f * cp.y + p1.y;
    float len = std::sqrt(dx * dx + dy * dy);
    int segments = std::max(1, static_cast<int>(std::ceil(std::sqrt(len / tol))));
    segments = std::min(segments, 64);
    for (int i = 1; i <= segments; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(segments);
        float u = 1.0f - t;
        float x = u * u * p0.x + 2.0f * u * t * cp.x + t * t * p1.x;
        float y = u * u * p0.y + 2.0f * u * t * cp.y + t * t * p1.y;
        out.push_back({x, y});
    }
}

void flattenCubicBezier(const Offset& p0, const Offset& cp1, const Offset& cp2,
                        const Offset& p3, std::vector<PathTessVertex>& out)
{
    const float tol = 0.25f;
    float dx = p3.x - p0.x;
    float dy = p3.y - p0.y;
    float chord = std::sqrt(dx * dx + dy * dy) + 0.0001f;
    float d1 = std::abs((cp1.x - p0.x) * dy - (cp1.y - p0.y) * dx);
    float d2 = std::abs((cp2.x - p0.x) * dy - (cp2.y - p0.y) * dx);
    float flat = std::max(d1, d2) / chord;
    int segments = std::max(1, std::min(64, static_cast<int>(std::ceil(std::sqrt(flat / tol)))));
    for (int i = 1; i <= segments; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(segments);
        float u = 1.0f - t;
        float u2 = u * u, u3 = u2 * u;
        float t2 = t * t, t3 = t2 * t;
        float x = u3 * p0.x + 3.0f * u2 * t * cp1.x + 3.0f * u * t2 * cp2.x + t3 * p3.x;
        float y = u3 * p0.y + 3.0f * u2 * t * cp1.y + 3.0f * u * t2 * cp2.y + t3 * p3.y;
        out.push_back({x, y});
    }
}

void flattenArc(const Rect& rect, float start_angle, float sweep_angle,
                std::vector<PathTessVertex>& out)
{
    float rx = rect.width * 0.5f;
    float ry = rect.height * 0.5f;
    float cx = rect.x + rx;
    float cy = rect.y + ry;
    float abs_sweep = std::abs(sweep_angle);
    int segments = std::max(3, std::min(128, static_cast<int>(abs_sweep * 20.0f)));
    for (int i = 1; i <= segments; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(segments);
        float a = start_angle + sweep_angle * t;
        out.push_back({cx + rx * std::cos(a), cy + ry * std::sin(a)});
    }
}

std::vector<PathContour> buildPathContours(const Path& path)
{
    std::vector<PathContour> contours;
    PathContour* current = nullptr;
    PathTessVertex current_pt{0.0f, 0.0f};

    for (const auto& cmd : path.commands())
    {
        switch (cmd.type)
        {
            case Path::PathCommandType::moveTo:
            {
                contours.emplace_back();
                current = &contours.back();
                current_pt = {cmd.p1.x, cmd.p1.y};
                current->push_back(current_pt);
                break;
            }
            case Path::PathCommandType::lineTo:
            {
                if (!current) { contours.emplace_back(); current = &contours.back(); }
                current_pt = {cmd.p1.x, cmd.p1.y};
                current->push_back(current_pt);
                break;
            }
            case Path::PathCommandType::cubicTo:
            {
                if (!current) { contours.emplace_back(); current = &contours.back(); }
                flattenCubicBezier(Offset{current_pt.x, current_pt.y}, cmd.cp1, cmd.cp2, cmd.p1, *current);
                current_pt = current->back();
                break;
            }
            case Path::PathCommandType::quadTo:
            {
                if (!current) { contours.emplace_back(); current = &contours.back(); }
                flattenQuadBezier(Offset{current_pt.x, current_pt.y}, cmd.cp1, cmd.p1, *current);
                current_pt = current->back();
                break;
            }
            case Path::PathCommandType::arcTo:
            {
                if (!current) { contours.emplace_back(); current = &contours.back(); }
                flattenArc(Rect{cmd.p1.x, cmd.p1.y, cmd.cp1.x, cmd.cp1.y},
                           cmd.start_angle, cmd.sweep_angle, *current);
                current_pt = current->back();
                break;
            }
            case Path::PathCommandType::close:
            {
                if (current && !current->empty())
                {
                    if (current->front().x != current_pt.x || current->front().y != current_pt.y)
                        current->push_back(current->front());
                    current_pt = current->front();
                }
                break;
            }
        }
    }
    return contours;
}

void triangulateContour(const PathContour& poly, std::vector<PathTessVertex>& triangles)
{
    PathContour verts = poly;
    if (verts.size() > 1 &&
        verts.front().x == verts.back().x && verts.front().y == verts.back().y)
    {
        verts.pop_back();
    }
    if (verts.size() < 3) return;

    float signed_area = 0.0f;
    for (size_t i = 0; i < verts.size(); ++i)
    {
        const auto& a = verts[i];
        const auto& b = verts[(i + 1) % verts.size()];
        signed_area += (a.x * b.y - b.x * a.y);
    }
    bool ccw = signed_area > 0.0f;

    std::vector<size_t> idx(verts.size());
    std::iota(idx.begin(), idx.end(), 0);

    size_t count = idx.size();
    size_t i = 0;
    while (count > 3)
    {
        size_t i0 = idx[(i + count - 1) % count];
        size_t i1 = idx[i];
        size_t i2 = idx[(i + 1) % count];

        const auto& a = verts[i0];
        const auto& b = verts[i1];
        const auto& c = verts[i2];

        float cr = pathCross2d(a, b, c);
        bool convex = ccw ? (cr >= -1e-4f) : (cr <= 1e-4f);
        if (!convex) { i = (i + 1) % count; continue; }

        bool inside = false;
        for (size_t j = 0; j < count; ++j)
        {
            size_t vi = idx[j];
            if (vi == i0 || vi == i1 || vi == i2) continue;
            if (pointInTriangle(verts[vi], a, b, c)) { inside = true; break; }
        }
        if (inside) { i = (i + 1) % count; continue; }

        triangles.push_back(a);
        triangles.push_back(b);
        triangles.push_back(c);

        idx.erase(idx.begin() + i);
        --count;
        i = i % count;
    }

    if (count == 3)
    {
        triangles.push_back(verts[idx[0]]);
        triangles.push_back(verts[idx[1]]);
        triangles.push_back(verts[idx[2]]);
    }
}

} // namespace systems::leal::campello_widgets
