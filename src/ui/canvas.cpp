#include <campello_widgets/ui/canvas.hpp>
#include <campello_gpu/texture.hpp>
#include <vector_math/vector4.hpp>

#include "ui/nine_patch_geometry.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <type_traits>

namespace systems::leal::campello_widgets
{
    namespace vm = systems::leal::vector_math;

    // Helper for rotation matrix
    static Matrix4 rotationMatrix(float radians)
    {
        float c = std::cos(radians);
        float s = std::sin(radians);
        Matrix4 m = Matrix4::identity();
        m.data[0] = c;  m.data[1] = s;
        m.data[4] = -s; m.data[5] = c;
        return m;
    }

    // Helper for scale matrix
    static Matrix4 scaleMatrix(float sx, float sy)
    {
        Matrix4 m = Matrix4::identity();
        m.data[0] = sx;
        m.data[5] = sy;
        return m;
    }

    // Helper for skew matrix
    // Matches Flutter/Skia: x' = x + sx*y, y' = y + sy*x.
    // Matrix4 is row-major with column-vector convention, so row 0 affects x'
    // and row 1 affects y'.
    static Matrix4 skewMatrix(float sx, float sy)
    {
        Matrix4 m = Matrix4::identity();
        m.data[1] = sx;  // Horizontal skew (x' += sx * y)
        m.data[4] = sy;  // Vertical skew (y' += sy * x)
        return m;
    }

    // Blends `src` over `dst` using the Porter-Duff Fa/Fb formula for
    // `mode` (`modulate` is the one exception -- not a Porter-Duff formula,
    // a plain component-wise product, matching this codebase's own
    // shader-mask "modulate" blend -- see widgets.metal's
    // shaderMaskFragment). Both inputs and the result are straight
    // (non-premultiplied) alpha, matching Color's own convention; the
    // Fa/Fb math itself runs on premultiplied values, per the standard
    // Porter-Duff derivation. See Paint::color_filter's doc comment for
    // why this is exact everywhere for some modes and only edge-pixel-
    // approximate for others.
    static Color blendColors(const Color& src, const Color& dst, BlendMode mode)
    {
        const float sr = src.r * src.a, sg = src.g * src.a, sb = src.b * src.a, sa = src.a;
        const float dr = dst.r * dst.a, dg = dst.g * dst.a, db = dst.b * dst.a, da = dst.a;

        if (mode == BlendMode::modulate)
        {
            const float a = sa * da;
            return (a > 1e-5f)
                ? Color::fromRGBA(sr * dr / a, sg * dg / a, sb * db / a, a)
                : Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.0f);
        }

        float fa = 1.0f, fb = 0.0f;
        switch (mode)
        {
            case BlendMode::clear:   fa = 0.0f;      fb = 0.0f;      break;
            case BlendMode::src:     fa = 1.0f;      fb = 0.0f;      break;
            case BlendMode::dst:     fa = 0.0f;      fb = 1.0f;      break;
            case BlendMode::srcOver: fa = 1.0f;      fb = 1.0f - sa; break;
            case BlendMode::dstOver: fa = 1.0f - da; fb = 1.0f;      break;
            case BlendMode::srcIn:   fa = da;        fb = 0.0f;      break;
            case BlendMode::dstIn:   fa = 0.0f;      fb = sa;        break;
            case BlendMode::srcOut:  fa = 1.0f - da; fb = 0.0f;      break;
            case BlendMode::dstOut:  fa = 0.0f;      fb = 1.0f - sa; break;
            case BlendMode::srcATop: fa = da;        fb = 1.0f - sa; break;
            case BlendMode::dstATop: fa = 1.0f - da; fb = sa;        break;
            case BlendMode::xorMode: fa = 1.0f - da; fb = 1.0f - sa; break;
            case BlendMode::plus:    fa = 1.0f;      fb = 1.0f;      break;
            case BlendMode::modulate: break; // handled above
        }

        float r = sr * fa + dr * fb;
        float g = sg * fa + dg * fb;
        float b = sb * fa + db * fb;
        float a = sa * fa + da * fb;
        if (mode == BlendMode::plus)
        {
            r = std::min(r, 1.0f); g = std::min(g, 1.0f);
            b = std::min(b, 1.0f); a = std::min(a, 1.0f);
        }

        return (a > 1e-5f)
            ? Color::fromRGBA(r / a, g / a, b / a, a)
            : Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.0f);
    }

    // Resolves `invert_colors` (color -> 1-color), `color_filter`, and
    // per-canvas opacity into `p.color` once, on the CPU, when a draw
    // command is recorded. Correct for these solid-color fills/strokes:
    // both operations commute with rasterization the same way inverting a
    // flat color does, so this needs no backend/shader changes at all --
    // see Paint::invert_colors's/Paint::color_filter's doc comments for
    // their (deliberate) scope boundaries: neither applies to
    // drawImage()/drawTintedImage() (per-pixel texture content) or
    // saveLayer()'s Paint (the layer's contents aren't a single color).
    static void resolvePaint(Paint& p, float opacity)
    {
        if (p.invert_colors)
        {
            p.color = Color::fromRGBA(1.0f - p.color.r, 1.0f - p.color.g, 1.0f - p.color.b, p.color.a);
            p.invert_colors = false;
        }
        if (p.color_filter)
        {
            p.color = blendColors(p.color_filter->color, p.color, p.color_filter->blend_mode);
            p.color_filter.reset();
        }
        if (opacity < 1.0f)
            p.color.a *= opacity;
    }

    // Shifts a Shader's own anchor Offset(s) (LinearGradient's begin/end,
    // RadialGradient/SweepGradient's center) by (dx, dy) -- used below to
    // keep Paint::shader's coordinates anchored to a shape's *unstroked*
    // bounds even when the shader-mask capture region itself is grown to
    // avoid clipping a stroke's outer edge. A no-op copy when both are 0.
    static Shader shiftShaderOrigin(const Shader& shader, float dx, float dy)
    {
        if (dx == 0.0f && dy == 0.0f)
            return shader;
        return std::visit([&](auto s) -> Shader {
            using S = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<S, LinearGradient>)
            {
                s.begin.x += dx; s.begin.y += dy;
                s.end.x   += dx; s.end.y   += dy;
            }
            else // RadialGradient, SweepGradient -- both have `center`.
            {
                s.center.x += dx; s.center.y += dy;
            }
            return s;
        }, shader);
    }

    // If `paint.shader` is set, wraps `draw_mask_shape` (a call back into
    // one of Canvas's own drawXxx() methods, with a plain white mask Paint)
    // in a beginShaderMask()/endShaderMask() scope so the shape paints with
    // the shader instead of a solid color -- the same "opaque white shape,
    // modulate-blend by the gradient" technique BoxDecoration's own gradient
    // fill/border use (see render_decorated_box.cpp): drawing fully-opaque
    // white and multiplying (`modulate`) by the gradient's own premultiplied
    // output reproduces the gradient's color exactly, masked to the shape's
    // (antialiased) coverage.
    //
    // `bounds` is the shape's own tight/fill bounding box -- see
    // Paint::shader's doc comment for why the shader's coordinates are
    // relative to it. When `paint.style == PaintStyle::stroke`, the actual
    // painted pixels extend `stroke_width / 2` past `bounds`; the capture
    // region is grown by that amount so the stroke's outer half isn't
    // clipped at its edge, with the shader's own coordinates shifted by the
    // same amount so they stay anchored to `bounds`, not the grown capture
    // region (mirroring the *effect* of render_decorated_box.cpp's gradient
    // border inset, but by growing the capture instead of shrinking the
    // geometry, since these coordinates are user-authored here, not
    // auto-resolved from a widget's own size).
    //
    // Returns true if a shader was present and handled -- the caller must
    // not also record its own normal draw command in that case.
    template <typename DrawMaskFn>
    static bool paintWithShaderIfPresent(
        Canvas& canvas, const Rect& bounds, const Paint& paint, DrawMaskFn&& draw_mask_shape)
    {
        if (!paint.shader.has_value())
            return false;

        const float inset = (paint.style == PaintStyle::stroke) ? paint.stroke_width * 0.5f : 0.0f;
        const Rect capture_bounds = Rect::fromLTWH(
            bounds.x - inset, bounds.y - inset,
            bounds.width + 2.0f * inset, bounds.height + 2.0f * inset);
        const Shader shifted = shiftShaderOrigin(*paint.shader, inset, inset);

        Paint mask = paint;
        mask.shader.reset();
        mask.color = Color::fromRGBA(1.0f, 1.0f, 1.0f, paint.color.a);

        canvas.beginShaderMask(capture_bounds, shifted, BlendMode::modulate);
        draw_mask_shape(mask);
        canvas.endShaderMask();
        return true;
    }

    Canvas::Canvas(float viewport_width, float viewport_height)
        : current_transform_(Matrix4::identity())
        , current_clip_(Rect::fromLTWH(0.0f, 0.0f, viewport_width, viewport_height))
    {
    }

    // ------------------------------------------------------------------
    // Draw primitives
    // ------------------------------------------------------------------

    void Canvas::drawRect(const Rect& rect, const Paint& paint)
    {
        if (paintWithShaderIfPresent(*this, rect, paint,
                [&](const Paint& mask) { drawRect(rect, mask); }))
            return;

        Paint p = paint;
        resolvePaint(p, current_opacity_);
        commands_.push_back(DrawRectCmd{rect, p});
    }

    void Canvas::drawText(const TextSpan& span, const Offset& origin)
    {
        if (current_opacity_ >= 1.0f)
        {
            commands_.push_back(DrawTextCmd{span, origin});
        }
        else
        {
            TextSpan s = span;
            s.style.color.a *= current_opacity_;
            commands_.push_back(DrawTextCmd{s, origin});
        }
    }

    void Canvas::drawImage(
        std::shared_ptr<campello_gpu::Texture> texture,
        const Rect& src_rect,
        const Rect& dst_rect,
        FilterQuality filter_quality)
    {
        commands_.push_back(DrawImageCmd{std::move(texture), src_rect, dst_rect, current_opacity_, filter_quality});
    }

    void Canvas::drawImageNine(
        std::shared_ptr<campello_gpu::Texture> texture,
        const Rect& center,
        const Rect& dst_rect,
        FilterQuality filter_quality)
    {
        if (!texture) return;

        const float img_w = static_cast<float>(texture->getWidth());
        const float img_h = static_cast<float>(texture->getHeight());

        for (const auto& patch : computeNinePatchGeometry(img_w, img_h, center, dst_rect))
            drawImage(texture, patch.src, patch.dst, filter_quality);
    }

    void Canvas::drawTintedImage(
        std::shared_ptr<campello_gpu::Texture> texture,
        const Rect& src_rect,
        const Rect& dst_rect,
        const Color& tint,
        FilterQuality filter_quality)
    {
        commands_.push_back(DrawTintedImageCmd{std::move(texture), src_rect, dst_rect, tint, current_opacity_, filter_quality});
    }

    // ------------------------------------------------------------------
    // New drawing methods for Flutter Canvas API compatibility
    // ------------------------------------------------------------------

    void Canvas::drawCircle(const Offset& center, float radius, const Paint& paint)
    {
        const Rect bounds{center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f};
        if (paintWithShaderIfPresent(*this, bounds, paint,
                [&](const Paint& mask) { drawCircle(center, radius, mask); }))
            return;

        Paint p = paint;
        resolvePaint(p, current_opacity_);
        commands_.push_back(DrawCircleCmd{center, radius, p});
    }

    void Canvas::drawOval(const Rect& rect, const Paint& paint)
    {
        if (paintWithShaderIfPresent(*this, rect, paint,
                [&](const Paint& mask) { drawOval(rect, mask); }))
            return;

        Paint p = paint;
        resolvePaint(p, current_opacity_);
        commands_.push_back(DrawOvalCmd{rect, p});
    }

    void Canvas::drawArc(const Rect& rect, float start_angle, float sweep_angle,
                         bool use_center, const Paint& paint)
    {
        if (paintWithShaderIfPresent(*this, rect, paint,
                [&](const Paint& mask) { drawArc(rect, start_angle, sweep_angle, use_center, mask); }))
            return;

        Paint p = paint;
        resolvePaint(p, current_opacity_);
        commands_.push_back(DrawArcCmd{rect, start_angle, sweep_angle, use_center, p});
    }

    void Canvas::drawLine(const Offset& p1, const Offset& p2, const Paint& paint)
    {
        if (paint.shader.has_value())
        {
            // A line is always effectively "stroked" width-wise regardless
            // of paint.style (there's no fill concept for a zero-area
            // segment) -- force the stroke-width capture inset unconditionally
            // by routing through PaintStyle::stroke, rather than relying on
            // paintWithShaderIfPresent()'s paint.style check.
            const Rect bounds = Rect::fromLTRB(
                std::min(p1.x, p2.x), std::min(p1.y, p2.y),
                std::max(p1.x, p2.x), std::max(p1.y, p2.y));
            Paint stroke_paint = paint;
            stroke_paint.style = PaintStyle::stroke;
            if (paintWithShaderIfPresent(*this, bounds, stroke_paint,
                    [&](const Paint& mask) { drawLine(p1, p2, mask); }))
                return;
        }

        Paint p = paint;
        resolvePaint(p, current_opacity_);
        commands_.push_back(DrawLineCmd{p1, p2, p});
    }

    void Canvas::drawRRect(const RRect& rrect, const Paint& paint)
    {
        if (paintWithShaderIfPresent(*this, rrect.rect, paint,
                [&](const Paint& mask) { drawRRect(rrect, mask); }))
            return;

        Paint p = paint;
        resolvePaint(p, current_opacity_);
        commands_.push_back(DrawRRectCmd{rrect, p});
    }

    void Canvas::drawDRRect(const RRect& outer, const RRect& inner, const Paint& paint)
    {
        drawRRect(outer, paint);
        // Punch through the inner region with opaque white to produce the ring effect.
        // This is correct when the background is white (as in all fidelity tests).
        Paint clearPaint;
        clearPaint.color = Color{1.0f, 1.0f, 1.0f, 1.0f};
        drawRRect(inner, clearPaint);
    }

    void Canvas::drawPath(const Path& path, const Paint& paint)
    {
        if (paintWithShaderIfPresent(*this, path.getBounds(), paint,
                [&](const Paint& mask) { drawPath(path, mask); }))
            return;

        Paint p = paint;
        resolvePaint(p, current_opacity_);
        commands_.push_back(DrawPathCmd{path, p});
    }

    void Canvas::drawPoints(PointMode mode, const std::vector<Offset>& points, const Paint& paint)
    {
        Paint p = paint;
        resolvePaint(p, current_opacity_);
        commands_.push_back(DrawPointsCmd{mode, points, p});
    }

    void Canvas::drawShadow(const Path& path, const Color& color, float elevation, 
                            bool transparent_occluder)
    {
        commands_.push_back(DrawShadowCmd{path, color, elevation, transparent_occluder});
    }

    void Canvas::drawPaint(const Paint& paint)
    {
        // drawRect() itself resolves opacity/invert_colors -- applying
        // opacity here too would double-count it (color.a *= opacity twice).
        drawRect(current_clip_, paint);
    }

    void Canvas::drawColor(const Color& color, BlendMode blend_mode)
    {
        Paint p;
        p.color = color;
        p.blend_mode = blend_mode;
        drawRect(current_clip_, p);
    }

    // ------------------------------------------------------------------
    // State management
    // ------------------------------------------------------------------

    void Canvas::save()
    {
        save_stack_.push_back({current_transform_, current_clip_, current_opacity_, 0, 0, false});
    }

    void Canvas::saveLayer(const Rect& bounds, const Paint& paint)
    {
        // Start a new compositing layer
        save();

        // Mark the most recent save entry as a layer so restore() emits the
        // matching SaveLayerEndCmd.
        if (!save_stack_.empty()) {
            save_stack_.back().is_layer = true;
        }

        // Use the provided bounds or current clip
        Rect layer_bounds = bounds.isEmpty() ? current_clip_ : bounds.intersection(current_clip_);

        commands_.push_back(SaveLayerCmd{layer_bounds, paint});
    }

    void Canvas::restore()
    {
        assert(!save_stack_.empty() && "restore() without matching save()");

        const SaveEntry& entry = save_stack_.back();

        for (int i = 0; i < entry.pushed_transforms; ++i)
            commands_.push_back(PopTransformCmd{});

        for (int i = 0; i < entry.pushed_clips; ++i)
            commands_.push_back(PopClipRectCmd{});

        current_transform_ = entry.transform;
        current_clip_      = entry.clip;
        current_opacity_   = entry.opacity;

        // Emit the layer end marker after popping the layer's local state so
        // the Renderer composites the offscreen layer on top of the restored
        // content.
        if (entry.is_layer)
            commands_.push_back(SaveLayerEndCmd{});

        save_stack_.pop_back();
    }

    void Canvas::restoreToCount(int count)
    {
        int current = getSaveCount();
        while (current > count && !save_stack_.empty()) {
            restore();
            current--;
        }
    }

    int Canvas::getSaveCount() const
    {
        // Start at 1 for the initial state
        return 1 + static_cast<int>(save_stack_.size());
    }

    void Canvas::setOpacity(float factor) noexcept
    {
        current_opacity_ *= factor;
    }

    // ------------------------------------------------------------------
    // Transform
    // ------------------------------------------------------------------

    void Canvas::translate(float dx, float dy)
    {
        transform(Matrix4::translate({dx, dy, 0.0f}));
    }

    void Canvas::rotate(float radians)
    {
        transform(rotationMatrix(radians));
    }

    void Canvas::scale(float sx, float sy)
    {
        transform(scaleMatrix(sx, sy));
    }

    void Canvas::skew(float sx, float sy)
    {
        transform(skewMatrix(sx, sy));
    }

    void Canvas::transform(const Matrix4& matrix)
    {
        commands_.push_back(PushTransformCmd{matrix});
        current_transform_ = current_transform_ * matrix;

        if (!save_stack_.empty())
            ++save_stack_.back().pushed_transforms;
    }

    // ------------------------------------------------------------------
    // Clip
    // ------------------------------------------------------------------

    namespace
    {
        // Clip rects are specified in the caller's *local* space (the same
        // space as the `offset` it was derived from), but current_clip_ is
        // tracked in absolute space. Ancestors that scroll their content
        // (ListView, SingleChildScrollView, ...) apply that scroll purely
        // via canvas.translate() — current_transform_ — without touching
        // `offset` itself, so any descendant that establishes its own clip
        // from a raw offset-derived rect must transform it through
        // current_transform_ first, or it ends up intersecting a
        // pre-scroll rect against a post-scroll ambient clip. Corners are
        // transformed individually (not just two opposite ones) and
        // min/maxed into an AABB so rotation/skew don't produce an
        // inverted or wrong-sized box.
        Rect transformRectAABB(const Rect& rect, const Matrix4& m)
        {
            const vm::Vector4<float> corners[4] = {
                m * vm::Vector4<float>(rect.left(),  rect.top(),    0.0f, 1.0f),
                m * vm::Vector4<float>(rect.right(), rect.top(),    0.0f, 1.0f),
                m * vm::Vector4<float>(rect.left(),  rect.bottom(), 0.0f, 1.0f),
                m * vm::Vector4<float>(rect.right(), rect.bottom(), 0.0f, 1.0f),
            };
            float minx = corners[0].x(), maxx = corners[0].x();
            float miny = corners[0].y(), maxy = corners[0].y();
            for (int i = 1; i < 4; ++i)
            {
                minx = std::min(minx, corners[i].x());
                maxx = std::max(maxx, corners[i].x());
                miny = std::min(miny, corners[i].y());
                maxy = std::max(maxy, corners[i].y());
            }
            return Rect::fromLTRB(minx, miny, maxx, maxy);
        }
    }

    void Canvas::clipRect(const Rect& rect)
    {
        const Rect clipped = current_clip_.intersection(transformRectAABB(rect, current_transform_));
        commands_.push_back(PushClipRectCmd{clipped});
        current_clip_ = clipped;

        if (!save_stack_.empty())
            ++save_stack_.back().pushed_clips;
    }

    void Canvas::clipRRect(const RRect& rrect)
    {
        // For now, clip to the bounding rect
        // Full implementation would need GPU stencil buffer or shader-based clipping
        const Rect clipped = current_clip_.intersection(transformRectAABB(rrect.rect, current_transform_));
        commands_.push_back(PushClipRRectCmd{rrect});
        current_clip_ = clipped;

        if (!save_stack_.empty())
            ++save_stack_.back().pushed_clips;
    }

    void Canvas::clipOval(const Rect& rect)
    {
        const Rect clipped = current_clip_.intersection(transformRectAABB(rect, current_transform_));
        commands_.push_back(PushClipOvalCmd{rect});
        current_clip_ = clipped;

        if (!save_stack_.empty())
            ++save_stack_.back().pushed_clips;
    }

    void Canvas::clipPath(const Path& path)
    {
        // For now, clip to the path bounds
        // Full implementation would need GPU stencil buffer
        const Rect bounds = path.getBounds();
        const Rect clipped = current_clip_.intersection(bounds);
        commands_.push_back(PushClipPathCmd{path});
        current_clip_ = clipped;

        if (!save_stack_.empty())
            ++save_stack_.back().pushed_clips;
    }

    // ------------------------------------------------------------------
    // BackdropFilter scope
    // ------------------------------------------------------------------

    void Canvas::beginBackdropFilter(const Rect& bounds, const ImageFilter& filter)
    {
        commands_.push_back(DrawBackdropFilterBeginCmd{bounds, filter});
    }

    void Canvas::endBackdropFilter()
    {
        commands_.push_back(DrawBackdropFilterEndCmd{});
    }

    // ------------------------------------------------------------------
    // ShaderMask scope
    // ------------------------------------------------------------------

    void Canvas::beginShaderMask(
        const Rect&   bounds,
        const Shader& shader,
        BlendMode     blend_mode)
    {
        commands_.push_back(DrawShaderMaskBeginCmd{bounds, shader, blend_mode});
    }

    void Canvas::endShaderMask()
    {
        commands_.push_back(DrawShaderMaskEndCmd{});
    }

} // namespace systems::leal::campello_widgets
