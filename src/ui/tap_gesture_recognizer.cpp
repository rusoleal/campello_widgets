#include <cmath>
#include <campello_widgets/ui/tap_gesture_recognizer.hpp>
#include <campello_widgets/ui/render_gesture_detector.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/gesture_constants.hpp>

namespace systems::leal::campello_widgets
{

    TapGestureRecognizer::TapGestureRecognizer(RenderGestureDetector& owner,
                                                 PointerButton          button,
                                                 VoidField               on_tap_field,
                                                 TapDownField            on_tap_down_field,
                                                 TapUpField              on_tap_up_field,
                                                 VoidField               on_tap_cancel_field,
                                                 VoidField               on_double_tap_field)
        : owner_(owner)
        , button_(button)
        , on_tap_field_(on_tap_field)
        , on_tap_down_field_(on_tap_down_field)
        , on_tap_up_field_(on_tap_up_field)
        , on_tap_cancel_field_(on_tap_cancel_field)
        , on_double_tap_field_(on_double_tap_field)
    {
    }

    void TapGestureRecognizer::addPointer(const PointerEvent& down)
    {
        if (down.button != button_) return;

        has_down_       = true;
        won_            = false;
        rejected_       = false;
        pending_        = false;
        down_pos_       = down.position;
        down_local_pos_ = down.local_position;
        down_time_ms_   = currentMonotonicMs();

        arena_entry_.reset();
        if (auto* d = PointerDispatcher::activeDispatcher())
            arena_entry_.emplace(d->arena().add(down.pointer_id, this));

        if (on_tap_down_field_ && owner_.*on_tap_down_field_)
            (owner_.*on_tap_down_field_)(TapDownDetails{down.position, down.local_position});
    }

    void TapGestureRecognizer::handlePointerEvent(const PointerEvent& event)
    {
        if (!has_down_ || rejected_) return;

        switch (event.kind)
        {
        case PointerEventKind::move:
        {
            const float dx   = event.position.x - down_pos_.x;
            const float dy   = event.position.y - down_pos_.y;
            const float dist = std::sqrt(dx * dx + dy * dy);
            // Movement no longer tap-like -- cede the arena (to a
            // PanGestureRecognizer on this same detector, or an ancestor
            // scrollable). A tap never "wins by moving", so it always
            // rejects here rather than accepting, mirroring the original
            // combined state machine's no-pan-handlers branch.
            if (dist > kStationaryTolerance && arena_entry_)
                arena_entry_->resolve(GestureDisposition::rejected);
            break;
        }
        case PointerEventKind::up:
        {
            const float dx   = event.position.x - down_pos_.x;
            const float dy   = event.position.y - down_pos_.y;
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < kStationaryTolerance)
            {
                up_pos_       = event.position;
                up_local_pos_ = event.local_position;
                if (won_)
                    resolveTap();
                else
                    pending_ = true;
            }
            has_down_ = false;
            arena_entry_.reset();
            break;
        }
        case PointerEventKind::cancel:
            has_down_ = false;
            pending_  = false;
            // A cancel always abandons a pending tap, regardless of what
            // the arena eventually decides for this member (see
            // rejectGesture()'s doc) -- the `rejected_` guard there keeps
            // this exactly-once even if the arena still calls back later.
            if (!rejected_)
            {
                rejected_ = true;
                if (on_tap_cancel_field_ && owner_.*on_tap_cancel_field_)
                    (owner_.*on_tap_cancel_field_)();
            }
            arena_entry_.reset();
            break;
        default:
            break;
        }
    }

    void TapGestureRecognizer::acceptGesture(int32_t /*pointer_id*/)
    {
        won_ = true;
        if (pending_)
        {
            pending_ = false;
            resolveTap();
        }
    }

    void TapGestureRecognizer::rejectGesture(int32_t /*pointer_id*/)
    {
        // Guarded so a self-reject already handled by the cancel-case above
        // (or vice versa) can't fire the cancel callback twice for the same
        // pointer -- GestureArenaMember's contract only guarantees exactly
        // one of accept/reject per pointer, not that this method is the
        // only path to "abandoned".
        if (rejected_) return;
        rejected_ = true;
        pending_  = false;
        // Only the primary tap drives the shared press-visual state --
        // secondary/tertiary taps (right/middle click) aren't a "press and
        // hold" interaction in the same sense, and letting them toggle
        // pressed_ would fight with a concurrent primary press on the same
        // detector.
        if (button_ == PointerButton::primary) owner_.setPressed(false);
        if (on_tap_cancel_field_ && owner_.*on_tap_cancel_field_)
            (owner_.*on_tap_cancel_field_)();
    }

    void TapGestureRecognizer::resolveTap()
    {
        // Only a primary tap requests keyboard focus, matching Flutter's
        // convention that a right/middle click (typically a context-menu
        // trigger) doesn't also steal focus the way an activating click
        // does.
        if (button_ == PointerButton::primary) owner_.requestFocusOnTap();

        // Only check for double-tap when this button supports it and
        // something is actually listening -- see
        // DoubleTapGestureRecognizer's doc comment for why a plain tap
        // never delays waiting to find out.
        const bool is_double = on_double_tap_field_ &&
            double_tap_.checkAndConsume(static_cast<bool>(owner_.*on_double_tap_field_), down_time_ms_, down_pos_);

        if (is_double)
        {
            if (owner_.*on_double_tap_field_) (owner_.*on_double_tap_field_)();
        }
        else
        {
            if (on_tap_up_field_ && owner_.*on_tap_up_field_)
                (owner_.*on_tap_up_field_)(TapUpDetails{up_pos_, up_local_pos_});
            if (on_tap_field_ && owner_.*on_tap_field_)
                (owner_.*on_tap_field_)();
        }
    }

} // namespace systems::leal::campello_widgets
