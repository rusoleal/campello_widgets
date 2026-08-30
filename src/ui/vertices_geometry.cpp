#include "ui/vertices_geometry.hpp"
#include "ui/blend_colors.hpp"

namespace systems::leal::campello_widgets
{
    namespace
    {
        // Appends the flat triangle-list index expansion of `idx` (already
        // resolved -- either `v.indices` or the implicit 0..N-1 sequence)
        // for `mode` into `out`.
        void expandToTriangleList(const std::vector<uint32_t>& idx, VertexMode mode,
            std::vector<uint32_t>& out)
        {
            if (idx.size() < 3) return;

            switch (mode)
            {
                case VertexMode::triangles:
                {
                    const size_t count = (idx.size() / 3) * 3;
                    out.insert(out.end(), idx.begin(), idx.begin() + static_cast<long>(count));
                    break;
                }
                case VertexMode::triangleStrip:
                {
                    for (size_t k = 0; k + 2 < idx.size(); ++k)
                    {
                        if ((k % 2) == 0)
                        {
                            out.push_back(idx[k]);
                            out.push_back(idx[k + 1]);
                            out.push_back(idx[k + 2]);
                        }
                        else
                        {
                            out.push_back(idx[k + 1]);
                            out.push_back(idx[k]);
                            out.push_back(idx[k + 2]);
                        }
                    }
                    break;
                }
                case VertexMode::triangleFan:
                {
                    for (size_t k = 0; k + 2 < idx.size(); ++k)
                    {
                        out.push_back(idx[0]);
                        out.push_back(idx[k + 1]);
                        out.push_back(idx[k + 2]);
                    }
                    break;
                }
            }
        }
    }

    BuiltVertices buildTriangleListVertices(
        const Vertices& v, const Color& paint_color, BlendMode blend_mode)
    {
        BuiltVertices result;
        if (v.positions.empty()) return result;

        result.vertices.reserve(v.positions.size());
        for (size_t i = 0; i < v.positions.size(); ++i)
        {
            const Color color = (i < v.colors.size())
                ? blendColors(paint_color, v.colors[i], blend_mode)
                : paint_color;
            result.vertices.push_back(VertexColorPoint{v.positions[i], color});
        }

        if (!v.indices.empty())
        {
            expandToTriangleList(v.indices, v.mode, result.indices);
        }
        else
        {
            std::vector<uint32_t> implicit(v.positions.size());
            for (uint32_t i = 0; i < implicit.size(); ++i) implicit[i] = i;
            expandToTriangleList(implicit, v.mode, result.indices);
        }

        return result;
    }
}
