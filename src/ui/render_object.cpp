#include <campello_widgets/ui/render_object.hpp>
#include <campello_widgets/ui/debug_flags.hpp>

#include <iostream>

namespace systems::leal::campello_widgets
{

    // -----------------------------------------------------------------------
    // Debug helpers (translation unit local)
    // -----------------------------------------------------------------------

    static Color nextRainbowColor() noexcept
    {
        // Six semi-transparent palette entries that cycle across dirty repaints.
        static const Color kColors[] = {
            Color::fromRGBA(0.90f, 0.20f, 0.20f, 0.20f), // red
            Color::fromRGBA(0.90f, 0.55f, 0.10f, 0.20f), // orange
            Color::fromRGBA(0.80f, 0.80f, 0.10f, 0.20f), // yellow
            Color::fromRGBA(0.15f, 0.75f, 0.20f, 0.20f), // green
            Color::fromRGBA(0.15f, 0.45f, 0.90f, 0.20f), // blue
            Color::fromRGBA(0.55f, 0.15f, 0.85f, 0.20f), // purple
        };
        static int idx = 0;
        return kColors[idx++ % 6];
    }

    void RenderObject::markNeedsLayout() noexcept
    {
        if (!needs_layout_)
        {
            needs_layout_ = true;
            if (parent_) {
                parent_->markNeedsLayout();
            } else {
                std::cerr << "[RenderObject] markNeedsLayout with no parent: this=" << this << "\n";
            }
        }
        markNeedsPaint();
    }

    void RenderObject::markNeedsPaint() noexcept
    {
        if (needs_paint_) return;
        needs_paint_ = true;
        if (parent_) parent_->markNeedsPaint();
    }

    void RenderObject::layout(const BoxConstraints& constraints)
    {
        const bool constraints_changed = !(constraints == constraints_);

        if (!needs_layout_ && !constraints_changed) {
            std::cerr << "[RenderObject] layout skipped: needs_layout=" << needs_layout_ 
                      << " constraints_changed=" << constraints_changed << " this=" << this << "\n";
            return;
        }

        std::cerr << "[RenderObject] layout running: constraints_changed=" << constraints_changed 
                  << " this=" << this << "\n";
        
        const bool must_repaint = constraints_changed;
        constraints_  = constraints;
        needs_layout_ = false;
        performLayout();
        if (must_repaint)
            markNeedsPaint();
    }

    void RenderObject::paint(PaintContext& context, const Offset& offset)
    {
        const bool was_dirty = needs_paint_;

        // ------------------------------------------------------------------
        // Debug overlays — zero cost when all flags are false
        // ------------------------------------------------------------------

        if (DebugFlags::repaintRainbowEnabled && was_dirty)
        {
            const Rect bounds = Rect::fromLTWH(offset.x, offset.y,
                                               size_.width, size_.height);
            context.canvas().drawRect(bounds, Paint::filled(nextRainbowColor()));
        }

        performPaint(context, offset);
        needs_paint_ = false;

        if (DebugFlags::paintSizeEnabled && !size_.isEmpty())
        {
            const Rect  bounds = Rect::fromLTWH(offset.x, offset.y,
                                                size_.width, size_.height);
            // Teal 1-px outline — matches Flutter's debugPaintSizeEnabled colour.
            const Paint border = Paint::stroked(
                Color::fromRGBA(0.0f, 0.55f, 1.0f, 0.85f), 1.0f);
            context.canvas().drawRect(bounds, border);
        }
    }

} // namespace systems::leal::campello_widgets
