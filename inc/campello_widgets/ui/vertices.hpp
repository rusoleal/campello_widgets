#pragma once

#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/color.hpp>
#include <cstdint>
#include <vector>

namespace systems::leal::campello_widgets
{

    /**
     * @brief How `Vertices::positions` (and `Vertices::indices`, when set)
     * are grouped into triangles -- matches Flutter's `VertexMode`.
     */
    enum class VertexMode
    {
        triangles,     ///< Each consecutive group of 3 is one triangle.
        triangleStrip, ///< Vertex i, i+1, i+2 form a triangle, for every i.
        triangleFan,   ///< Vertex 0, i+1, i+2 form a triangle, for every i.
    };

    /**
     * @brief A triangle mesh for `Canvas::drawVertices()` -- matches
     * Flutter's `Vertices` class, scoped down to position + per-vertex
     * color (see `Canvas::drawVertices()`'s doc comment for what's out of
     * scope: texture coordinates and mesh-edge antialiasing).
     */
    class Vertices
    {
    public:
        VertexMode mode = VertexMode::triangles;

        std::vector<Offset> positions;

        /**
         * @brief Per-vertex color, parallel to `positions`. Leave empty to
         * paint every vertex with `Canvas::drawVertices()`'s own `Paint`
         * color instead (a flat-colored mesh).
         */
        std::vector<Color> colors;

        /**
         * @brief Optional indices into `positions`/`colors`, interpreted
         * according to `mode`. Leave empty to use `positions` directly, in
         * `mode` order.
         */
        std::vector<uint32_t> indices;
    };

} // namespace systems::leal::campello_widgets
