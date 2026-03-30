#include <campello_widgets/ui/render_mouse_region.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>

namespace systems::leal::campello_widgets
{

    RenderMouseRegion::RenderMouseRegion()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->addHandler(this,     [this](const PointerEvent& e) { onPointerEvent(e); });
            d->addTickHandler(this, [this](uint64_t t)             { onTick(t);         });
        }
    }

    RenderMouseRegion::~RenderMouseRegion()
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
        if (event.kind != PointerEventKind::move) return;

        hovered_this_tick_ = true;
        if (on_hover) on_hover(event.position);
    }

    void RenderMouseRegion::onTick(uint64_t /*now_ms*/)
    {
        if (hovered_this_tick_)
        {
            if (!hovered_)
            {
                hovered_ = true;
                if (on_enter) on_enter();
            }
        }
        else if (hovered_)
        {
            hovered_ = false;
            if (on_exit) on_exit();
        }
        hovered_this_tick_ = false;
    }

} // namespace systems::leal::campello_widgets
