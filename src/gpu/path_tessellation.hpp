#pragma once

#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/path.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <vector>

namespace systems::leal::campello_widgets
{

/**
 * @brief 2D vertex produced by path flattening/tessellation.
 *
 * Coordinates are in the same logical (pre-transform) space as the source
 * Path commands. Backends transform these to physical pixels before drawing.
 */
struct PathTessVertex
{
    float x = 0.0f;
    float y = 0.0f;
};

using PathContour = std::vector<PathTessVertex>;

/**
 * @brief Build closed, flattened contours from a Path.
 *
 * Cubic/quadratic Bezier curves and arcs are flattened into line segments.
 * The returned contours are suitable for fill triangulation or stroke
 * expansion.
 */
std::vector<PathContour> buildPathContours(const Path& path);

/**
 * @brief Ear-clip triangulation of a single closed contour.
 *
 * Appends triangles (3 vertices each) to `out`. The contour is assumed to be
 * a simple polygon after flattening; self-intersections are not handled.
 */
void triangulateContour(const PathContour& contour, std::vector<PathTessVertex>& out);

/**
 * @brief Flatten a quadratic Bezier from p0 through cp to p1.
 *
 * Appends the subdivided points (excluding p0) to `out`.
 */
void flattenQuadBezier(const Offset& p0, const Offset& cp, const Offset& p1,
                       std::vector<PathTessVertex>& out);

/**
 * @brief Flatten a cubic Bezier from p0 through cp1/cp2 to p3.
 *
 * Appends the subdivided points (excluding p0) to `out`.
 */
void flattenCubicBezier(const Offset& p0, const Offset& cp1, const Offset& cp2,
                        const Offset& p3, std::vector<PathTessVertex>& out);

/**
 * @brief Flatten an elliptical arc into line segments.
 *
 * The arc is inscribed in `rect` (x, y, width, height) starting at
 * `start_angle` and sweeping `sweep_angle` radians. Appended points exclude
 * the starting point.
 */
void flattenArc(const Rect& rect, float start_angle, float sweep_angle,
                std::vector<PathTessVertex>& out);

} // namespace systems::leal::campello_widgets
