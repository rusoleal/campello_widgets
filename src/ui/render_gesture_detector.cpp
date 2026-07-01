#include <cmath>
#include <chrono>
#include <campello_widgets/ui/render_gesture_detector.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>

namespace systems::leal::campello_widgets
{

    RenderGestureDetector::RenderGestureDetector()
    {
    }

    RenderGestureDetector::~RenderGestureDetector()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->arena().removeMember(this);
            d->removeHandler(this);
            d->removeTickHandler(this);
        }
    }

    void RenderGestureDetector::attach()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->addHandler(this, [this](const PointerEvent& e) { onPointerEvent(e); });
            d->addTickHandler(this, [this](uint64_t now) { onTick(now); });
        }
    }

    void RenderGestureDetector::detach()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->arena().removeMember(this);
            d->removeHandler(this);
            d->removeTickHandler(this);
        }
    }

    // -------------------------------------------------------------------------
    // GestureArenaMember
    // -------------------------------------------------------------------------

    void RenderGestureDetector::acceptGesture(int32_t /*pointer_id*/)
    {
        won_arena_ = true;
        if (has_down_ && !panning_ && exceedsSlop())
            panning_ = true;
        if (pending_tap_)
        {
            pending_tap_ = false;
            resolveTapOutcome();
        }
    }

    void RenderGestureDetector::rejectGesture(int32_t /*pointer_id*/)
    {
        lost_arena_  = true;
        pending_tap_ = false;
    }

    bool RenderGestureDetector::exceedsSlop() const noexcept
    {
        const float dx = last_pos_.x - down_pos_.x;
        const float dy = last_pos_.y - down_pos_.y;
        return std::sqrt(dx * dx + dy * dy) > kTapSlop;
    }

    void RenderGestureDetector::resolveTapOutcome()
    {
        // Check for double-tap.
        if (last_tap_valid_ && down_time_ms_ != 0 &&
            (down_time_ms_ - last_tap_time_ms_) <= kDoubleTapMs)
        {
            const float dtx   = down_pos_.x - last_tap_pos_.x;
            const float dty   = down_pos_.y - last_tap_pos_.y;
            const float ddist = std::sqrt(dtx * dtx + dty * dty);
            if (ddist < kTapSlop)
            {
                if (on_double_tap) on_double_tap();
                last_tap_valid_ = false;
                return;
            }
        }

        if (on_tap) on_tap();

        // Record this tap for double-tap detection.
        last_tap_valid_   = true;
        last_tap_time_ms_ = down_time_ms_;
        last_tap_pos_     = down_pos_;
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
            won_arena_        = false;
            lost_arena_       = false;
            pending_tap_      = false;
            down_pos_         = event.position;
            last_pos_         = event.position;
            {
                auto now = std::chrono::steady_clock::now();
                down_time_ms_ = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        now.time_since_epoch()).count());
            }
            arena_entry_.reset();
            if (auto* d = PointerDispatcher::activeDispatcher())
                arena_entry_.emplace(d->arena().add(event.pointer_id, this));
            if (on_pan_down) on_pan_down(event.local_position);
            break;

        case PointerEventKind::move:
            if (has_down_ && !lost_arena_)
            {
                const Offset delta = event.position - last_pos_;
                last_pos_ = event.position;

                if (!panning_ && exceedsSlop())
                {
                    if (won_arena_)
                    {
                        // Already ours (uncontested, or claimed via long
                        // press) — exceeding slop just reclassifies this as
                        // "not a tap" and is unrelated to arena competition.
                        panning_ = true;
                    }
                    else if (arena_entry_)
                    {
                        // Only fight for the gesture if we actually act on a
                        // pan — a pure tap/long-press detector (the common
                        // "button" case) has nothing to do with movement and
                        // should give up its claim instead, mirroring
                        // Flutter's TapGestureRecognizer self-rejecting once
                        // the pointer travels past slop. Otherwise an
                        // ancestor scrollable could never win past a button
                        // it merely happens to be dragged across.
                        arena_entry_->resolve(
                            (on_pan_update || on_pan_end)
                                ? GestureDisposition::accepted
                                : GestureDisposition::rejected);
                    }
                }

                if (panning_ && on_pan_update)
                    on_pan_update(delta);
            }
            break;

        case PointerEventKind::up:
            if (has_down_ && !lost_arena_)
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
                        if (won_arena_)
                            resolveTapOutcome();
                        else
                            pending_tap_ = true;
                    }
                }
            }
            has_down_ = false;
            panning_  = false;
            arena_entry_.reset();
            break;

        case PointerEventKind::cancel:
            has_down_    = false;
            panning_     = false;
            pending_tap_ = false;
            arena_entry_.reset();
            break;

        case PointerEventKind::scroll:
            if (on_scroll)
                on_scroll({event.scroll_delta_x, event.scroll_delta_y});
            break;
        }
    }

    void RenderGestureDetector::performLayout()
    {
        // Transparent: pass constraints through unchanged (same as RenderMouseRegion).
        // Using RenderBox::performLayout's loosen+center default would give children
        // loose constraints, causing Stacks and other fill-to-max widgets to claim
        // infinite height when placed in an unbounded main axis (e.g. Column).
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

    void RenderGestureDetector::onTick(uint64_t now_ms)
    {
        if (!has_down_ || lost_arena_) return;

        // Long press: fire once after kLongPressMs without moving past slop.
        if (!panning_ && !long_press_fired_ &&
            (now_ms - down_time_ms_) >= kLongPressMs)
        {
            // Claim explicitly — mirrors Flutter's LongPressGestureRecognizer
            // accepting as soon as its timer fires, so a competing ancestor
            // pan/scroll can no longer steal the gesture after this point.
            if (!won_arena_ && arena_entry_)
                arena_entry_->resolve(GestureDisposition::accepted);

            long_press_fired_ = true;
            if (on_long_press) on_long_press();
        }
    }

} // namespace systems::leal::campello_widgets
