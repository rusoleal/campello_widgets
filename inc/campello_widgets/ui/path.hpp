#pragma once

#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <campello_widgets/ui/rrect.hpp>
#include <vector_math/matrix4.hpp>
#include <vector>
#include <cmath>

namespace systems::leal::campello_widgets
{
    using Matrix4 = systems::leal::vector_math::Matrix4<float>;

    /**
     * @brief A path for drawing and clipping custom shapes.
     *
     * Path supports a subset of Flutter's Path operations:
     * - Move to a point
     * - Line to a point
     * - Cubic Bezier curves
     * - Quadratic Bezier curves
     * - Arcs
     * - Close the path
     * - Add rect, oval, rounded rect
     *
     * Note: This is a simplified path implementation. Full SVG-style paths
     * would require more complex curve types and fill rules.
     */
    class Path
    {
    public:
        enum class FillType
        {
            winding,    // Non-zero winding rule
            evenOdd,    // Even-odd rule
        };

        enum class PathCommandType
        {
            moveTo,
            lineTo,
            cubicTo,      // Cubic Bezier
            quadTo,       // Quadratic Bezier
            arcTo,
            close,
        };

        struct PathCommand
        {
            PathCommandType type;
            Offset p1;      // Primary point (end point for line/curve)
            Offset cp1;     // Control point 1 (for curves)
            Offset cp2;     // Control point 2 (for cubic curves)
            // For arcs
            float radius = 0.0f;
            float start_angle = 0.0f;
            float sweep_angle = 0.0f;
        };

        Path() = default;

        // ------------------------------------------------------------------
        // Path construction
        // ------------------------------------------------------------------

        /** @brief Starts a new subpath at the given point. */
        void moveTo(float x, float y);
        void moveTo(const Offset& o) { moveTo(o.x, o.y); }

        /** @brief Adds a line from current point to the given point. */
        void lineTo(float x, float y);
        void lineTo(const Offset& o) { lineTo(o.x, o.y); }

        /** @brief Adds a cubic Bezier curve. */
        void cubicTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y);
        void cubicTo(const Offset& cp1, const Offset& cp2, const Offset& p)
        {
            cubicTo(cp1.x, cp1.y, cp2.x, cp2.y, p.x, p.y);
        }

        /** @brief Adds a quadratic Bezier curve. */
        void quadTo(float cpx, float cpy, float x, float y);
        void quadTo(const Offset& cp, const Offset& p)
        {
            quadTo(cp.x, cp.y, p.x, p.y);
        }

        /** @brief Adds an arc. */
        void arcTo(const Rect& rect, float start_angle, float sweep_angle, bool force_move_to);

        /** @brief Adds an arc with radius. */
        void arcToPoint(const Offset& arc_end, const Offset& arc_control, float radius);

        /** @brief Closes the current subpath. */
        void close();

        // ------------------------------------------------------------------
        // Shape additions (convenience methods)
        // ------------------------------------------------------------------

        /** @brief Adds a rectangle to the path. */
        void addRect(const Rect& rect);

        /** @brief Adds an oval to the path. */
        void addOval(const Rect& rect);

        /** @brief Adds a rounded rectangle to the path. */
        void addRRect(const RRect& rrect);

        /** @brief Adds an arc as a new subpath. */
        void addArc(const Rect& oval, float start_angle, float sweep_angle);

        /** @brief Adds a polygon from a list of points. */
        void addPolygon(const std::vector<Offset>& points, bool close);

        // ------------------------------------------------------------------
        // Path operations
        // ------------------------------------------------------------------

        /** @brief Resets the path to empty. */
        void reset();

        /** @brief Returns true if the path contains no commands. */
        bool isEmpty() const { return commands_.empty(); }

        /** @brief Returns the bounding rectangle of the path. */
        Rect getBounds() const;

        /** @brief Returns true if the path contains the given point. */
        bool contains(const Offset& point) const;

        /** @brief Transforms the path by the given matrix. */
        void transform(const Matrix4& matrix);

        /** @brief Returns a copy of this path. */
        Path copy() const;

        // ------------------------------------------------------------------
        // Accessors
        // ------------------------------------------------------------------

        const std::vector<PathCommand>& commands() const { return commands_; }
        FillType fillType() const { return fill_type_; }
        void setFillType(FillType type) { fill_type_ = type; }

        /** @brief Returns the current point (last point added). */
        Offset currentPoint() const { return current_point_; }

    private:
        std::vector<PathCommand> commands_;
        FillType fill_type_ = FillType::winding;
        Offset current_point_;
        Offset start_point_;  // For close()
        bool has_current_point_ = false;
    };

} // namespace systems::leal::campello_widgets
