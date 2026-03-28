#pragma once

#include <memory>
#include <variant>
#include <vector>
#include <campello_widgets/ui/rect.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <vector_math/matrix4.hpp>
#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/text_span.hpp>
#include <campello_widgets/ui/path.hpp>
#include <campello_widgets/ui/rrect.hpp>

// campello_gpu forward declaration — avoids pulling the full GPU headers
// into every widget translation unit.
namespace systems::leal::campello_gpu { class Texture; }

namespace systems::leal::campello_widgets
{

    using Matrix4 = systems::leal::vector_math::Matrix4<float>;

    // ------------------------------------------------------------------
    // New draw command types for Flutter Canvas API compatibility
    // ------------------------------------------------------------------

    /** @brief Draw a circle with center and radius. */
    struct DrawCircleCmd
    {
        Offset center;
        float  radius;
        Paint  paint;
    };

    /** @brief Draw an oval filling the given rectangle. */
    struct DrawOvalCmd
    {
        Rect  rect;
        Paint paint;
    };

    /** @brief Draw an arc. */
    struct DrawArcCmd
    {
        Rect  rect;           // Bounding rect (oval)
        float start_angle;    // Radians
        float sweep_angle;    // Radians
        bool  use_center;     // If true, draws a pie slice; if false, just the arc
        Paint paint;
    };

    /** @brief Draw a line between two points. */
    struct DrawLineCmd
    {
        Offset p1;
        Offset p2;
        Paint  paint;
    };

    /** @brief Draw a rounded rectangle. */
    struct DrawRRectCmd
    {
        RRect rrect;
        Paint paint;
    };

    /** @brief Draw a path. */
    struct DrawPathCmd
    {
        Path  path;
        Paint paint;
    };

    /** @brief Draw points (points, lines, or polygon). */
    enum class PointMode
    {
        points,     // Individual points
        lines,      // Lines between consecutive pairs
        polygon,    // Lines connecting all points in a loop
    };

    struct DrawPointsCmd
    {
        PointMode           mode;
        std::vector<Offset> points;
        Paint               paint;
    };

    /** @brief Draw a shadow for a path (material elevation). */
    struct DrawShadowCmd
    {
        Path   path;
        Color  color;
        float  elevation;
        bool   transparent_occluder;
    };

    /** @brief Save with a layer (for compositing). */
    struct SaveLayerCmd
    {
        Rect  bounds;  // Nullable - if empty, uses current clip
        Paint paint;   // For color filter, blend mode
    };

    /** @brief Clip using a path. */
    struct PushClipPathCmd
    {
        Path path;
    };

    /** @brief Clip using a rounded rectangle. */
    struct PushClipRRectCmd
    {
        RRect rrect;
    };

    // ------------------------------------------------------------------
    // Individual command types
    // ------------------------------------------------------------------

    /** @brief Draw a filled or stroked axis-aligned rectangle. */
    struct DrawRectCmd
    {
        Rect  rect;
        Paint paint;
    };

    /**
     * @brief Draw a text span at the given origin.
     *
     * The origin is the top-left corner of the text's bounding box
     * in the current transform's coordinate space.
     */
    struct DrawTextCmd
    {
        TextSpan span;
        Offset   origin;
    };

    /**
     * @brief Draw a GPU texture into a destination rectangle.
     *
     * @param texture  The source texture (must be valid for the draw).
     * @param src_rect Normalized source rectangle [0,1]×[0,1] within the texture.
     *                 Pass `Rect::fromLTWH(0,0,1,1)` for the whole texture.
     * @param dst_rect Destination rectangle in local coordinates.
     */
    struct DrawImageCmd
    {
        std::shared_ptr<campello_gpu::Texture> texture;
        Rect  src_rect;
        Rect  dst_rect;
        float opacity = 1.0f;
    };

    /** @brief Push a new clip rectangle onto the clip stack (intersects with current). */
    struct PushClipRectCmd
    {
        Rect rect;
    };

    /** @brief Pop the top clip rectangle from the stack. */
    struct PopClipRectCmd {};

    /** @brief Push a transform matrix onto the transform stack (post-multiplied). */
    struct PushTransformCmd
    {
        Matrix4 transform;
    };

    /** @brief Pop the top transform from the stack. */
    struct PopTransformCmd {};

    // ------------------------------------------------------------------
    // Variant
    // ------------------------------------------------------------------

    /**
     * @brief Tagged union of all draw operations emitted by PaintContext.
     *
     * The Renderer collects these from each PaintContext after a paint pass
     * and dispatches them to the registered IDrawBackend for GPU execution.
     */
    using DrawCommand = std::variant<
        // Basic shapes
        DrawRectCmd,
        DrawCircleCmd,
        DrawOvalCmd,
        DrawArcCmd,
        DrawLineCmd,
        DrawRRectCmd,
        DrawPointsCmd,
        
        // Complex shapes
        DrawPathCmd,
        DrawShadowCmd,
        
        // Text and images
        DrawTextCmd,
        DrawImageCmd,
        
        // Clipping
        PushClipRectCmd,
        PushClipRRectCmd,
        PushClipPathCmd,
        PopClipRectCmd,
        
        // Transforms and state
        PushTransformCmd,
        PopTransformCmd,
        SaveLayerCmd
    >;

    using DrawList = std::vector<DrawCommand>;

} // namespace systems::leal::campello_widgets
