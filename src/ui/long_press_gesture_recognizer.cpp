#include <cmath>
#include <campello_widgets/ui/long_press_gesture_recognizer.hpp>
#include <campello_widgets/ui/render_gesture_detector.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/frame_scheduler.hpp>
#include <campello_widgets/ui/gesture_constants.hpp>

namespace systems::leal::campello_widgets
{

    LongPressGestureRecognizer::LongPressGestureRecognizer(RenderGestureDetector& owner)
        : owner_(owner)
    {
    }

    void LongPressGestureRecognizer::addPointer(const PointerEvent& down)
    {
        has_down_       = true;
        rejected_       = false;
        fired_          = false;
        down_pos_       = down.position;
        last_pos_       = down.position;
        down_local_pos_ = down.local_position;
        last_local_pos_ = down.local_position;
        down_time_ms_   = currentMonotonicMs();

        arena_entry_.reset();
        if (auto* d = PointerDispatcher::activeDispatcher())
            arena_entry_.emplace(d->arena().add(down.pointer_id, this));

        // Frames are only produced on demand -- a stationary press doesn't
        // itself invalidate anything, so nothing would otherwise ask for
        // another frame until the pointer moves or lifts. Without this,
        // handleTick() (which the deadline check depends on) would never
        // run again after this one, and long-press could never fire.
        if (owner_.on_long_press || owner_.on_long_press_start) FrameScheduler::scheduleFrame();

        if (owner_.on_long_press_down)
            owner_.on_long_press_down(LongPressDownDetails{down.position, down.local_position});
    }

    void LongPressGestureRecognizer::handlePointerEvent(const PointerEvent& event)
    {
        if (!has_down_ || rejected_) return;

        switch (event.kind)
        {
        case PointerEventKind::move:
            last_pos_       = event.position;
            last_local_pos_ = event.local_position;

            if (fired_)
            {
                // Already started -- a long press, once recognised, isn't
                // undone by subsequent movement; just report it.
                const auto now = std::chrono::steady_clock::now();
                vx_.addPosition(now, event.position.x);
                vy_.addPosition(now, event.position.y);
                if (owner_.on_long_press_move_update)
                    owner_.on_long_press_move_update(LongPressMoveUpdateDetails{
                        event.position, event.local_position, event.position - down_pos_});
            }
            else if (exceedsStationaryTolerance() && arena_entry_)
            {
                arena_entry_->resolve(GestureDisposition::rejected);
            }
            break;
        case PointerEventKind::up:
            if (fired_ && owner_.on_long_press_end)
            {
                const Offset velocity{vx_.getVelocity(), vy_.getVelocity()};
                owner_.on_long_press_end(LongPressEndDetails{event.position, event.local_position, velocity});
            }
            has_down_ = false;
            arena_entry_.reset();
            break;
        case PointerEventKind::cancel:
            has_down_ = false;
            // A cancel always abandons a not-yet-fired press, regardless of
            // what the arena eventually decides for this member (sweep()
            // still runs on cancel and could pick this member as the
            // first-added default winner via acceptGesture(), which is a
            // no-op here -- see acceptGesture()'s doc -- and would
            // otherwise leave on_long_press_cancel never firing). The
            // `rejected_` guard in rejectGesture() keeps this exactly-once
            // if sweep() does call back rejectGesture() instead.
            if (!fired_ && !rejected_)
            {
                rejected_ = true;
                if (owner_.on_long_press_cancel) owner_.on_long_press_cancel();
            }
            arena_entry_.reset();
            break;
        default:
            break;
        }
    }

    void LongPressGestureRecognizer::handleTick(uint64_t now_ms)
    {
        if (!has_down_ || rejected_ || fired_) return;

        if (!exceedsStationaryTolerance() && (now_ms - down_time_ms_) >= kLongPressMs)
        {
            if (arena_entry_) arena_entry_->resolve(GestureDisposition::accepted);
            fired_ = true;

            // Velocity is sampled from the moment the press starts, not
            // from the original pointer-down -- pre-fire movement is by
            // definition bounded by the stationary tolerance, so it
            // wouldn't contribute meaningful velocity anyway.
            vx_.reset();
            vy_.reset();
            const auto now = std::chrono::steady_clock::now();
            vx_.addPosition(now, last_pos_.x);
            vy_.addPosition(now, last_pos_.y);

            if (owner_.on_long_press) owner_.on_long_press();
            if (owner_.on_long_press_start)
                owner_.on_long_press_start(LongPressStartDetails{down_pos_, down_local_pos_});
        }
        else if ((owner_.on_long_press || owner_.on_long_press_start) && !exceedsStationaryTolerance())
        {
            // Deadline not reached yet and still a live candidate -- request
            // another frame so this tick keeps running.
            FrameScheduler::scheduleFrame();
        }
    }

    void LongPressGestureRecognizer::acceptGesture(int32_t /*pointer_id*/)
    {
        // No-op: firing already happens explicitly in handleTick() the
        // moment the deadline is reached, which is also where this
        // recognizer claims the arena. Winning via some other path (e.g.
        // sweep default, when nothing else in the arena resolved before
        // the pointer lifted) doesn't retroactively make a too-short press
        // into a long-press.
    }

    void LongPressGestureRecognizer::rejectGesture(int32_t /*pointer_id*/)
    {
        // Guarded -- see the cancel-case comment in handlePointerEvent()
        // for why this can also already have fired on_long_press_cancel.
        const bool already_notified = rejected_;
        rejected_ = true;
        owner_.setPressed(false);
        if (!already_notified && owner_.on_long_press_cancel) owner_.on_long_press_cancel();
    }

    bool LongPressGestureRecognizer::exceedsStationaryTolerance() const noexcept
    {
        const float dx = last_pos_.x - down_pos_.x;
        const float dy = last_pos_.y - down_pos_.y;
        return std::sqrt(dx * dx + dy * dy) > kStationaryTolerance;
    }

} // namespace systems::leal::campello_widgets
