#include <campello_widgets/ui/render_slider.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>

#include <algorithm>
#include <cmath>

namespace systems::leal::campello_widgets
{

    RenderSlider::RenderSlider()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
            d->addHandler(this, [this](const PointerEvent& e) { onPointerEvent(e); });
    }

    RenderSlider::~RenderSlider()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
            d->removeHandler(this);
    }

    void RenderSlider::performLayout()
    {
        size_ = constraints_.constrain({
            std::isinf(constraints_.max_width) ? 0.0f : constraints_.max_width,
            std::isinf(constraints_.max_height) ? thumb_radius * 2.0f : constraints_.max_height
        });
    }

    void RenderSlider::performPaint(PaintContext& ctx, const Offset& offset)
    {
        global_offset_ = offset; // latch for pointer handling

        Canvas& canvas = ctx.canvas();
        const float w  = size_.width;
        const float h  = size_.height;
        const float cy = offset.y + h * 0.5f;
        const float tr = track_height * 0.5f;

        // Thumb x in global coords
        const float track_left  = offset.x + thumb_radius;
        const float track_right = offset.x + w - thumb_radius;
        const float thumb_x     = track_left + value * (track_right - track_left);

        // Inactive track (right of thumb)
        if (thumb_x < track_right)
        {
            canvas.drawRect(
                Rect::fromLTWH(thumb_x, cy - tr, track_right - thumb_x, track_height),
                Paint::filled(inactive_color));
        }

        // Active track (left of thumb)
        if (thumb_x > track_left)
        {
            canvas.drawRect(
                Rect::fromLTWH(track_left, cy - tr, thumb_x - track_left, track_height),
                Paint::filled(active_color));
        }

        // Thumb
        canvas.drawCircle({thumb_x, cy}, thumb_radius, Paint::filled(active_color));
    }

    // -------------------------------------------------------------------------

    void RenderSlider::onPointerEvent(const PointerEvent& event)
    {
        switch (event.kind)
        {
        case PointerEventKind::down:
            pressed_ = true;
            if (on_value_changed)
                on_value_changed(positionToValue(event.position.x));
            break;

        case PointerEventKind::move:
            if (pressed_ && on_value_changed)
                on_value_changed(positionToValue(event.position.x));
            break;

        case PointerEventKind::up:
        case PointerEventKind::cancel:
            pressed_ = false;
            break;

        default:
            break;
        }
    }

    float RenderSlider::positionToValue(float global_x) const noexcept
    {
        const float left  = global_offset_.x + thumb_radius;
        const float right = global_offset_.x + size_.width - thumb_radius;
        if (right <= left) return value;
        return std::clamp((global_x - left) / (right - left), 0.0f, 1.0f);
    }

} // namespace systems::leal::campello_widgets
