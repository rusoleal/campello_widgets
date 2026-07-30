#include <campello_widgets/ui/render_draw_surface.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/rect.hpp>

#include <algorithm>
#include <cmath>

namespace systems::leal::campello_widgets
{

    RenderDrawSurface::RenderDrawSurface() = default;

    RenderDrawSurface::~RenderDrawSurface()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
            d->removeHandler(this);
    }

    void RenderDrawSurface::attach()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
            d->addHandler(this, [this](const PointerEvent& e) { onPointerEvent(e); });
    }

    void RenderDrawSurface::detach()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->arena().removeMember(this);
            d->removeHandler(this);
        }
    }

    void RenderDrawSurface::performLayout()
    {
        RenderImage::performLayout();

        // Must run here, not from performPaint(): ensureSurface() calls
        // setTexture(), which calls markNeedsPaint() the first time it
        // (re)allocates. RenderObject::paint()'s wrapper clears
        // needs_paint_ *before* invoking performPaint() — so a
        // markNeedsPaint() fired from inside performPaint() re-dirties the
        // flag with nothing left to ever clear it again (this node is
        // never revisited once its RepaintBoundary ancestor's own flags
        // go quiet), permanently wedging every future stroke's
        // markNeedsPaint() call into a silent no-op. Layout always runs
        // before paint in the same frame, so calling it here instead lets
        // the dirty flag it may set get correctly consumed by this same
        // frame's upcoming paint pass.
        ensureSurface();
    }

    void RenderDrawSurface::ensureSurface()
    {
        auto* backend = RenderObject::activeBackend();
        if (!backend) return;

        const Size sz = size();
        if (sz.width <= 0.0f || sz.height <= 0.0f) return;

        if (surface_texture_ &&
            surface_logical_size_.width  == sz.width &&
            surface_logical_size_.height == sz.height)
            return;

        const float dpr = RenderObject::activeDevicePixelRatio();
        const uint32_t tw = static_cast<uint32_t>(std::ceil(sz.width  * dpr));
        const uint32_t th = static_cast<uint32_t>(std::ceil(sz.height * dpr));
        if (tw == 0 || th == 0) return;

        // A GPU texture can't be resized in place — allocating a fresh one
        // loses the previous texture's content unless it's explicitly
        // carried over (see the blit below). Stash it before overwriting
        // surface_texture_.
        auto previous_texture             = surface_texture_;
        const Size previous_logical_size  = surface_logical_size_;

        // Dedicated (non-pooled) — this texture is kept and displayed for
        // the RenderDrawSurface's entire lifetime, unlike the throwaway
        // per-frame composites the pool is designed for. See
        // IDrawBackend::createDedicatedOffscreenTexture()'s doc comment.
        surface_texture_      = backend->createDedicatedOffscreenTexture(tw, th);
        surface_logical_size_ = sz;
        setTexture(surface_texture_);

        pending_cmds_.clear();

        if (previous_texture)
        {
            // Resize: preserve existing strokes by blitting the old
            // texture's content into the new one's top-left corner
            // (Renderer::applyDrawSurfaceUpdate() performs the actual GPU
            // copy from blit_source) instead of wiping the canvas — matches
            // how a real drawing app crops/extends a canvas on resize
            // rather than clearing it.
            pending_blit_source_ = previous_texture;
            pending_clear_first_ = false;

            // Fill only the newly-exposed L-shaped region (if the surface
            // grew) with background_color — the blit already covers
            // [0,0]..[old size], so painting over that would erase what
            // was just preserved. Two non-overlapping rects: a full-width
            // bottom strip, then a right strip clipped to the old height
            // (avoids double-covering the bottom-right corner).
            if (sz.height > previous_logical_size.height)
            {
                pending_cmds_.push_back(DrawRectCmd{
                    Rect::fromLTWH(0, previous_logical_size.height,
                                   sz.width, sz.height - previous_logical_size.height),
                    Paint::filled(background_color)});
            }
            if (sz.width > previous_logical_size.width)
            {
                pending_cmds_.push_back(DrawRectCmd{
                    Rect::fromLTWH(previous_logical_size.width, 0,
                                   sz.width - previous_logical_size.width, previous_logical_size.height),
                    Paint::filled(background_color)});
            }
        }
        else
        {
            // First-ever allocation: content is undefined GPU memory —
            // paint a clean background so the first displayed frame isn't
            // garbage.
            pending_blit_source_.reset();
            pending_cmds_.push_back(DrawRectCmd{
                Rect::fromLTWH(0, 0, sz.width, sz.height), Paint::filled(background_color)});
            pending_clear_first_ = true;
        }
    }

    void RenderDrawSurface::performPaint(PaintContext& ctx, const Offset& offset)
    {
        if ((!pending_cmds_.empty() || pending_blit_source_) && surface_texture_)
        {
            DrawList bracket;
            bracket.reserve(pending_cmds_.size() + 2);
            bracket.push_back(DrawSurfaceUpdateBeginCmd{surface_texture_, pending_clear_first_, pending_blit_source_});
            for (const auto& c : pending_cmds_) bracket.push_back(c);
            bracket.push_back(DrawSurfaceUpdateEndCmd{});
            ctx.canvas().appendRecorded(bracket);

            pending_cmds_.clear();
            pending_clear_first_ = false;
            pending_blit_source_.reset();
        }

        RenderImage::performPaint(ctx, offset);
    }

    // -------------------------------------------------------------------------

    void RenderDrawSurface::acceptGesture(int32_t /*pointer_id*/)
    {
    }

    void RenderDrawSurface::rejectGesture(int32_t /*pointer_id*/)
    {
        lost_arena_ = true;
        pressed_    = false;
    }

    void RenderDrawSurface::onPointerEvent(const PointerEvent& event)
    {
        switch (event.kind)
        {
        case PointerEventKind::down:
            pressed_    = true;
            lost_arena_ = false;
            last_point_ = event.local_position;
            arena_entry_.reset();
            if (auto* d = PointerDispatcher::activeDispatcher())
            {
                arena_entry_.emplace(d->arena().add(event.pointer_id, this));
                // Claims immediately (no slop) — a draw surface should never
                // lose the pointer to an ancestor scrollable's pan gesture.
                arena_entry_->resolve(GestureDisposition::accepted);
            }
            stampSegment(event.local_position, event.local_position, event.pressure); // dot on tap
            break;

        case PointerEventKind::move:
            if (pressed_ && !lost_arena_)
            {
                stampSegment(last_point_, event.local_position, event.pressure);
                last_point_ = event.local_position;
            }
            break;

        case PointerEventKind::up:
        case PointerEventKind::cancel:
            pressed_ = false;
            arena_entry_.reset();
            break;

        default:
            break;
        }
    }

    void RenderDrawSurface::stampSegment(const Offset& from, const Offset& to, float pressure)
    {
        // Modulate stroke width by pressure, matching native drawing apps.
        // Mouse/finger input reports a constant 1.0 (see PointerEvent::pressure's
        // doc comment) so this is a no-op there — full width — and only
        // varies with genuine stylus pressure data.
        const float pressure_scale = 0.4f + 0.6f * std::clamp(pressure, 0.0f, 1.0f);
        const float radius = stroke_width * 0.5f * pressure_scale;
        const float dx     = to.x - from.x;
        const float dy     = to.y - from.y;
        const float dist   = std::sqrt(dx * dx + dy * dy);

        // Stamp closely enough together that fast pointer moves don't leave
        // gaps between circles.
        const float spacing = std::max(radius * 0.5f, 1.0f);
        const int   steps   = std::max(1, static_cast<int>(dist / spacing));

        const Paint paint = Paint::filled(stroke_color);
        for (int i = 0; i <= steps; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            pending_cmds_.push_back(DrawCircleCmd{
                Offset{from.x + dx * t, from.y + dy * t}, radius, paint});
        }

        markNeedsPaint();
    }

    void RenderDrawSurface::clear()
    {
        pending_cmds_.clear();
        pending_cmds_.push_back(DrawRectCmd{
            Rect::fromLTWH(0, 0, size().width, size().height), Paint::filled(background_color)});
        pending_clear_first_ = true;
        pending_blit_source_.reset();
        markNeedsPaint();
    }

} // namespace systems::leal::campello_widgets
