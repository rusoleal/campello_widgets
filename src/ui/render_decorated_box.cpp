#include <campello_widgets/ui/render_decorated_box.hpp>
#include <algorithm>
#include <cmath>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/canvas.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <campello_widgets/ui/rrect.hpp>
#include <campello_widgets/ui/path.hpp>
#include <campello_widgets/ui/paint.hpp>

namespace systems::leal::campello_widgets
{

    void RenderDecoratedBox::performLayout()
    {
        if (child_)
        {
            layoutChild(*child_, constraints_);
            size_ = child_->size();
            positionChild(*child_, {0.0f, 0.0f});
        }
        else
        {
            // See RenderColoredBox::performLayout()'s identical comment: fill
            // the bounded max per axis, fall back to 0 (not infinity) on an
            // unbounded axis.
            const Size fill{
                std::isinf(constraints_.max_width)  ? 0.0f : constraints_.max_width,
                std::isinf(constraints_.max_height) ? 0.0f : constraints_.max_height,
            };
            size_ = constraints_.constrain(fill);
        }
    }

    void RenderDecoratedBox::performPaint(PaintContext& context, const Offset& offset)
    {
        if (position == DecorationPosition::background)
        {
            paintDecoration(context.canvas(), offset);
            paintChild(context, offset);
        }
        else
        {
            paintChild(context, offset);
            paintDecoration(context.canvas(), offset);
        }
    }

    void RenderDecoratedBox::paint(PaintContext& context, const Offset& offset)
    {
        // Only decorations needing the offscreen-composite path pay for the
        // OffsetLayer cache — see this class's doc comment. Everything else
        // takes the default (uncached, but cheap) path.
        if (!needsOffsetLayerFor(decoration))
        {
            RenderObject::paint(context, offset);
            return;
        }

        // OR needsDescendantPaint() in: a replay skips performPaint()
        // entirely, so a nested boundary further down must not be
        // silently stranded — see that flag's doc comment (this is the
        // exact bug that previously froze the app: a nested RenderClipRRect's
        // dirty state got stranded under this boundary's replay).
        if (!offset_layer_.maybeReplay(context, offset, size_,
                                        needsPaint(), needsDescendantPaint()))
            offset_layer_.record(context, offset, [&] { performPaint(context, offset); });

        needs_paint_ = false;
        needs_descendant_paint_ = false;
    }

    void RenderDecoratedBox::paintDecoration(Canvas& canvas, const Offset& offset) const
    {
        const bool  has_radius = decoration.border_radius > 0.0f;
        const Rect  bounds     = Rect::fromLTWH(offset.x, offset.y,
                                                size_.width, size_.height);
        const RRect rbounds    = RRect{bounds, decoration.border_radius};

        // 1. Box shadows
        for (const BoxShadow& shadow : decoration.box_shadow)
        {
            // Build a path for the shadow shape, displaced by shadow.offset.
            const Rect shadow_bounds = Rect::fromLTWH(
                bounds.x + shadow.offset.x - shadow.spread_radius,
                bounds.y + shadow.offset.y - shadow.spread_radius,
                bounds.width  + 2.0f * shadow.spread_radius,
                bounds.height + 2.0f * shadow.spread_radius);

            Path shadow_path;
            if (has_radius)
                shadow_path.addRRect(RRect{shadow_bounds, decoration.border_radius});
            else
                shadow_path.addRect(shadow_bounds);

            // blur_radius is used as the elevation approximation.
            canvas.drawShadow(shadow_path, shadow.color, shadow.blur_radius, false);
        }

        // 2. Background fill (gradient takes precedence over solid color)
        if (decoration.gradient.has_value())
        {
            const Shader shader = resolveBoxGradient(*decoration.gradient, size_);
            const Paint  fill   = Paint::filled(Color::white());

            // modulate (child * mask), not srcIn (child, masked by mask.a):
            // the drawn shape is opaque white specifically so multiplying by
            // the gradient's own color reproduces that color exactly, masked
            // to the shape by the white fill's own alpha (0 outside it).
            canvas.beginShaderMask(bounds, shader, BlendMode::modulate);
            if (has_radius)
                canvas.drawRRect(rbounds, fill);
            else
                canvas.drawRect(bounds, fill);
            canvas.endShaderMask();
        }
        else if (decoration.color.has_value())
        {
            const Paint fill = Paint::filled(*decoration.color);
            if (has_radius)
                canvas.drawRRect(rbounds, fill);
            else
                canvas.drawRect(bounds, fill);
        }

        // 3. Border (gradient takes precedence over solid color)
        if (decoration.border.has_value())
        {
            const auto& b = *decoration.border;
            if (b.gradient.has_value())
            {
                const Shader shader = resolveBoxGradient(*b.gradient, size_);
                const Paint  stroke = Paint::stroked(Color::white(), b.width);

                // A stroke is centered on the path it's drawn against, so
                // stroking `rbounds`/`bounds` directly (as the solid-color
                // branch below does) puts half of it outside `bounds`. That's
                // fine for a normal direct draw, but the shader-mask capture
                // below sizes its offscreen texture to exactly `bounds` (see
                // Renderer::applyShaderMask), so that outer half would be
                // silently clipped at the texture edge -- a hard, non-
                // antialiased cutoff, not a fade, making the border look like
                // two different strokes (soft inner half, missing outer
                // half). Drawing on a path inset by half the stroke width
                // instead keeps the whole stroke inside `bounds`; `bounds`
                // itself must stay unchanged since the backend anchors the
                // gradient's local coordinates on it.
                const float inset  = b.width * 0.5f;
                const Rect  ibounds = Rect::fromLTWH(
                    bounds.x + inset, bounds.y + inset,
                    std::max(bounds.width  - b.width, 0.0f),
                    std::max(bounds.height - b.width, 0.0f));

                // modulate, not srcIn -- see the background-fill case above.
                canvas.beginShaderMask(bounds, shader, BlendMode::modulate);
                if (has_radius)
                    canvas.drawRRect(RRect{ibounds, std::max(decoration.border_radius - inset, 0.0f)}, stroke);
                else
                    canvas.drawRect(ibounds, stroke);
                canvas.endShaderMask();
            }
            else
            {
                const Paint stroke = Paint::stroked(b.color, b.width);
                if (has_radius)
                    canvas.drawRRect(rbounds, stroke);
                else
                    canvas.drawRect(bounds, stroke);
            }
        }
    }

} // namespace systems::leal::campello_widgets
