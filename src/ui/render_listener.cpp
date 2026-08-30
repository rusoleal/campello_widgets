#include <campello_widgets/ui/render_listener.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>

namespace systems::leal::campello_widgets
{

    RenderListener::RenderListener() = default;

    RenderListener::~RenderListener()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
            d->removeHandler(this);
    }

    void RenderListener::attach()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
            d->addHandler(this, [this](const PointerEvent& e) { onPointerEvent(e); });
    }

    void RenderListener::detach()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
            d->removeHandler(this);
    }

    void RenderListener::performLayout()
    {
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

    void RenderListener::performPaint(PaintContext& ctx, const Offset& offset)
    {
        RenderBox::performPaint(ctx, offset);
    }

    void RenderListener::onPointerEvent(const PointerEvent& event)
    {
        switch (event.kind)
        {
            case PointerEventKind::down:   if (on_pointer_down)   on_pointer_down(event);   break;
            case PointerEventKind::move:   if (on_pointer_move)   on_pointer_move(event);   break;
            case PointerEventKind::up:     if (on_pointer_up)     on_pointer_up(event);     break;
            case PointerEventKind::cancel: if (on_pointer_cancel) on_pointer_cancel(event); break;
            case PointerEventKind::scroll: if (on_pointer_signal) on_pointer_signal(event); break;
        }
    }

} // namespace systems::leal::campello_widgets
