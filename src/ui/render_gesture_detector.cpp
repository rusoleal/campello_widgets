#include <cmath>
#include <campello_widgets/ui/render_gesture_detector.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>

namespace systems::leal::campello_widgets
{

    RenderGestureDetector::RenderGestureDetector()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->addHandler(this, [this](const PointerEvent& e) { onPointerEvent(e); });
            d->addTickHandler(this, [this](uint64_t now) { onTick(now); });
        }
    }

    RenderGestureDetector::~RenderGestureDetector()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->removeHandler(this);
            d->removeTickHandler(this);
        }
    }

    // -------------------------------------------------------------------------

    void RenderGestureDetector::onPointerEvent(const PointerEvent& event)
    {
        switch (event.kind)
        {
        case PointerEventKind::down:
            has_down_         = true;
            panning_          = false;
            long_press_fired_ = false;
            down_pos_         = event.position;
            last_pos_         = event.position;
            down_time_ms_     = 0; // set by onTick on first tick after down
            break;

        case PointerEventKind::move:
            if (has_down_)
            {
                const float dx   = event.position.x - down_pos_.x;
                const float dy   = event.position.y - down_pos_.y;
                const float dist = std::sqrt(dx * dx + dy * dy);

                if (!panning_ && dist > kTapSlop)
                    panning_ = true;

                if (panning_ && on_pan_update)
                    on_pan_update(event.position - last_pos_);

                last_pos_ = event.position;
            }
            break;

        case PointerEventKind::up:
            if (has_down_)
            {
                if (panning_)
                {
                    if (on_pan_end) on_pan_end();
                }
                else if (!long_press_fired_)
                {
                    const float dx   = event.position.x - down_pos_.x;
                    const float dy   = event.position.y - down_pos_.y;
                    const float dist = std::sqrt(dx * dx + dy * dy);

                    if (dist < kTapSlop)
                    {
                        // Check for double-tap.
                        if (last_tap_valid_ && down_time_ms_ != 0 &&
                            (down_time_ms_ - last_tap_time_ms_) <= kDoubleTapMs)
                        {
                            const float dtx  = down_pos_.x - last_tap_pos_.x;
                            const float dty  = down_pos_.y - last_tap_pos_.y;
                            const float ddist = std::sqrt(dtx * dtx + dty * dty);
                            if (ddist < kTapSlop)
                            {
                                if (on_double_tap) on_double_tap();
                                last_tap_valid_ = false;
                                has_down_ = false;
                                panning_  = false;
                                break;
                            }
                        }

                        if (on_tap) on_tap();

                        // Record this tap for double-tap detection.
                        last_tap_valid_   = true;
                        last_tap_time_ms_ = down_time_ms_;
                        last_tap_pos_     = down_pos_;
                    }
                }
            }
            has_down_ = false;
            panning_  = false;
            break;

        case PointerEventKind::cancel:
            has_down_ = false;
            panning_  = false;
            break;

        case PointerEventKind::scroll:
            if (on_scroll)
                on_scroll({event.scroll_delta_x, event.scroll_delta_y});
            break;
        }
    }

    void RenderGestureDetector::onTick(uint64_t now_ms)
    {
        if (!has_down_) return;

        // Latch the down timestamp on the first tick after a down event.
        if (down_time_ms_ == 0)
            down_time_ms_ = now_ms;

        // Long press: fire once after kLongPressMs without moving past slop.
        if (!panning_ && !long_press_fired_ &&
            (now_ms - down_time_ms_) >= kLongPressMs)
        {
            long_press_fired_ = true;
            if (on_long_press) on_long_press();
        }
    }

} // namespace systems::leal::campello_widgets
