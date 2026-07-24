#include <campello_widgets/ui/render_slider.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>

#include <algorithm>
#include <cmath>

namespace systems::leal::campello_widgets
{

    RenderSlider::RenderSlider() = default;

    RenderSlider::~RenderSlider()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
            d->removeHandler(this);
    }

    void RenderSlider::attach()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
            d->addHandler(this, [this](const PointerEvent& e) { onPointerEvent(e); });
    }

    void RenderSlider::detach()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->arena().removeMember(this);
            d->removeHandler(this);
        }
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
        // Convert to tree-local space (subtract the safe-area inset baked
        // into `offset` — see RenderObject::setActivePaintOriginOffset's doc
        // comment) so this matches the space pointer positions (from
        // PointerDispatcher) are already in — otherwise thumb hit math is
        // off by the inset whenever it's non-zero (any iPhone).
        const Offset paint_origin = RenderObject::activePaintOriginOffset();
        global_offset_ = { offset.x - paint_origin.x, offset.y - paint_origin.y }; // latch for pointer handling

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

    void RenderSlider::acceptGesture(int32_t /*pointer_id*/)
    {
        won_arena_ = true;
    }

    void RenderSlider::rejectGesture(int32_t /*pointer_id*/)
    {
        lost_arena_ = true;
        pressed_    = false;
    }

    void RenderSlider::onPointerEvent(const PointerEvent& event)
    {
        switch (event.kind)
        {
        case PointerEventKind::down:
            pressed_    = true;
            won_arena_  = false;
            lost_arena_ = false;
            arena_entry_.reset();
            if (auto* d = PointerDispatcher::activeDispatcher())
            {
                arena_entry_.emplace(d->arena().add(event.pointer_id, this));
                // Slider claims immediately (no slop) so it reliably preempts
                // an ancestor scrollable's pan-to-scroll claim.
                arena_entry_->resolve(GestureDisposition::accepted);
            }
            if (on_value_changed) {
                on_value_changed(positionToValue(event.position.x));
                if (!RenderObject::isAlive(this)) return;
            }
            break;

        case PointerEventKind::move:
            if (pressed_ && !lost_arena_ && on_value_changed) {
                on_value_changed(positionToValue(event.position.x));
                if (!RenderObject::isAlive(this)) return;
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

    float RenderSlider::positionToValue(float global_x) const noexcept
    {
        const float left  = global_offset_.x + thumb_radius;
        const float right = global_offset_.x + size_.width - thumb_radius;
        if (right <= left) return value;
        return std::clamp((global_x - left) / (right - left), 0.0f, 1.0f);
    }

} // namespace systems::leal::campello_widgets
