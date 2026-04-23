#include <campello_widgets/ui/render_mouse_region.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/system_mouse_cursor.hpp>

namespace systems::leal::campello_widgets
{

    RenderMouseRegion::RenderMouseRegion() = default;

    RenderMouseRegion::~RenderMouseRegion()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->removeHandler(this);
            d->removeTickHandler(this);
        }
    }

    void RenderMouseRegion::attach()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->addHandler(this,     [this](const PointerEvent& e) { onPointerEvent(e); });
            d->addTickHandler(this, [this](uint64_t t)             { onTick(t);         });
        }
    }

    void RenderMouseRegion::detach()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->removeHandler(this);
            d->removeTickHandler(this);
        }
    }

    void RenderMouseRegion::performLayout()
    {
        // Transparent: size to child if present, otherwise to minimum.
        if (child_)
        {
            layoutChild(*child_, constraints_);
            size_ = child_->size();
        }
        else
        {
            size_ = constraints_.constrain({0.0f, 0.0f});
        }
    }

    void RenderMouseRegion::performPaint(PaintContext& ctx, const Offset& offset)
    {
        RenderBox::performPaint(ctx, offset);
    }

    // -------------------------------------------------------------------------

    void RenderMouseRegion::onPointerEvent(const PointerEvent& event)
    {
        if (event.kind == PointerEventKind::cancel)
        {
            if (hovered_)
            {
                hovered_ = false;
                resetSystemCursor();
                if (on_exit) on_exit();
            }
            return;
        }

        if (event.kind != PointerEventKind::move) return;

        if (!hovered_)
        {
            hovered_ = true;
            setSystemCursor(cursor);
            if (on_enter) on_enter();
        }
        if (on_hover) on_hover(event.position);
    }

    void RenderMouseRegion::onTick(uint64_t /*now_ms*/)
    {
        // Enter/exit are driven entirely by pointer events and cancel events
        // from PointerDispatcher. No tick-based logic needed.
    }

} // namespace systems::leal::campello_widgets
