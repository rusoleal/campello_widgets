#include <cmath>
#include <campello_widgets/ui/drag_gesture_recognizer.hpp>
#include <campello_widgets/ui/render_gesture_detector.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/gesture_constants.hpp>

namespace systems::leal::campello_widgets
{

    DragGestureRecognizer::DragGestureRecognizer(RenderGestureDetector& owner)
        : owner_(owner)
    {
    }

    bool DragGestureRecognizer::exceedsSlopThreshold() const noexcept
    {
        // Mirrors the pre-decomposition PanGestureRecognizer: with no real
        // handlers set for this family, this degrades to "is the pointer
        // still basically stationary" using the fixed tap tolerance, so a
        // detector that doesn't act on this drag family doesn't fight an
        // ancestor scrollable over movement it can't do anything with.
        const float slop = hasHandlers() ? computePanSlop(device_kind_) : kStationaryTolerance;
        return axisDistance(last_pos_ - down_pos_) > slop;
    }

    void DragGestureRecognizer::addPointer(const PointerEvent& down)
    {
        has_down_       = true;
        dragging_       = false;
        won_            = false;
        rejected_       = false;
        down_pos_       = down.position;
        last_pos_       = down.position;
        down_local_pos_ = down.local_position;
        last_local_pos_ = down.local_position;
        device_kind_    = down.device_kind;

        vx_.reset();
        vy_.reset();
        vx_.addPosition(down.timestamp_ms, down.position.x);
        vy_.addPosition(down.timestamp_ms, down.position.y);

        arena_entry_.reset();
        if (auto* d = PointerDispatcher::activeDispatcher())
            arena_entry_.emplace(d->arena().add(down.pointer_id, this));

        fireDown(DragDownDetails{down.position, down.local_position});
    }

    void DragGestureRecognizer::beginDrag()
    {
        dragging_ = true;
        owner_.setPressed(false);

        const bool use_down = owner_.drag_start_behavior == DragStartBehavior::down;
        DragStartDetails details;
        details.global_position = use_down ? down_pos_ : last_pos_;
        details.local_position  = use_down ? down_local_pos_ : last_local_pos_;
        fireStart(details);
    }

    void DragGestureRecognizer::handlePointerEvent(const PointerEvent& event)
    {
        if (!has_down_ || rejected_) return;

        switch (event.kind)
        {
        case PointerEventKind::move:
        {
            const Offset delta = event.position - last_pos_;
            last_pos_          = event.position;
            last_local_pos_    = event.local_position;

            vx_.addPosition(event.timestamp_ms, event.position.x);
            vy_.addPosition(event.timestamp_ms, event.position.y);

            if (!dragging_ && exceedsSlopThreshold())
            {
                if (won_)
                {
                    // Already ours (uncontested) -- exceeding slop just
                    // starts the drag; unrelated to arena competition.
                    beginDrag();
                }
                else if (arena_entry_)
                {
                    // Only fight for the gesture if this family actually
                    // acts on it -- see exceedsSlopThreshold()'s doc.
                    // resolve(accepted) synchronously calls
                    // acceptGesture() below (the arena is already closed
                    // by this point), which is what actually starts the drag.
                    arena_entry_->resolve(
                        hasHandlers() ? GestureDisposition::accepted : GestureDisposition::rejected);
                }
            }

            if (dragging_)
                fireUpdate(DragUpdateDetails{delta, event.position, event.local_position});
            break;
        }
        case PointerEventKind::up:
            if (dragging_)
            {
                const Offset velocity{vx_.getVelocity(), vy_.getVelocity()};
                fireEnd(DragEndDetails{velocity, primaryVelocityOf(velocity)});
            }
            has_down_ = false;
            dragging_ = false;
            arena_entry_.reset();
            break;
        case PointerEventKind::cancel:
            has_down_ = false;
            dragging_ = false;
            arena_entry_.reset();
            break;
        default:
            break;
        }
    }

    void DragGestureRecognizer::acceptGesture(int32_t /*pointer_id*/)
    {
        won_ = true;
        if (has_down_ && !dragging_ && exceedsSlopThreshold())
            beginDrag();
    }

    void DragGestureRecognizer::rejectGesture(int32_t /*pointer_id*/)
    {
        rejected_ = true;
        owner_.setPressed(false);
    }

    // ---------------------------------------------------------------------
    // PanGestureRecognizer
    // ---------------------------------------------------------------------

    bool PanGestureRecognizer::hasHandlers() const noexcept
    {
        return static_cast<bool>(owner_.on_pan_update) || static_cast<bool>(owner_.on_pan_end) ||
               static_cast<bool>(owner_.on_pan_start);
    }

    float PanGestureRecognizer::axisDistance(const Offset& delta_from_down) const noexcept
    {
        return std::sqrt(delta_from_down.x * delta_from_down.x + delta_from_down.y * delta_from_down.y);
    }

    void PanGestureRecognizer::fireDown(const DragDownDetails& details)
    {
        if (owner_.on_pan_down) owner_.on_pan_down(details);
    }

    void PanGestureRecognizer::fireStart(const DragStartDetails& details)
    {
        if (owner_.on_pan_start) owner_.on_pan_start(details);
    }

    void PanGestureRecognizer::fireUpdate(const DragUpdateDetails& details)
    {
        if (owner_.on_pan_update) owner_.on_pan_update(details);
    }

    void PanGestureRecognizer::fireEnd(const DragEndDetails& details)
    {
        if (owner_.on_pan_end) owner_.on_pan_end(details);
    }

    // ---------------------------------------------------------------------
    // HorizontalDragGestureRecognizer
    // ---------------------------------------------------------------------

    bool HorizontalDragGestureRecognizer::hasHandlers() const noexcept
    {
        return static_cast<bool>(owner_.on_horizontal_drag_update) ||
               static_cast<bool>(owner_.on_horizontal_drag_end) ||
               static_cast<bool>(owner_.on_horizontal_drag_start);
    }

    float HorizontalDragGestureRecognizer::axisDistance(const Offset& delta_from_down) const noexcept
    {
        return std::abs(delta_from_down.x);
    }

    void HorizontalDragGestureRecognizer::fireDown(const DragDownDetails& details)
    {
        if (owner_.on_horizontal_drag_down) owner_.on_horizontal_drag_down(details);
    }

    void HorizontalDragGestureRecognizer::fireStart(const DragStartDetails& details)
    {
        if (owner_.on_horizontal_drag_start) owner_.on_horizontal_drag_start(details);
    }

    void HorizontalDragGestureRecognizer::fireUpdate(const DragUpdateDetails& details)
    {
        if (owner_.on_horizontal_drag_update) owner_.on_horizontal_drag_update(details);
    }

    void HorizontalDragGestureRecognizer::fireEnd(const DragEndDetails& details)
    {
        if (owner_.on_horizontal_drag_end) owner_.on_horizontal_drag_end(details);
    }

    // ---------------------------------------------------------------------
    // VerticalDragGestureRecognizer
    // ---------------------------------------------------------------------

    bool VerticalDragGestureRecognizer::hasHandlers() const noexcept
    {
        return static_cast<bool>(owner_.on_vertical_drag_update) ||
               static_cast<bool>(owner_.on_vertical_drag_end) ||
               static_cast<bool>(owner_.on_vertical_drag_start);
    }

    float VerticalDragGestureRecognizer::axisDistance(const Offset& delta_from_down) const noexcept
    {
        return std::abs(delta_from_down.y);
    }

    void VerticalDragGestureRecognizer::fireDown(const DragDownDetails& details)
    {
        if (owner_.on_vertical_drag_down) owner_.on_vertical_drag_down(details);
    }

    void VerticalDragGestureRecognizer::fireStart(const DragStartDetails& details)
    {
        if (owner_.on_vertical_drag_start) owner_.on_vertical_drag_start(details);
    }

    void VerticalDragGestureRecognizer::fireUpdate(const DragUpdateDetails& details)
    {
        if (owner_.on_vertical_drag_update) owner_.on_vertical_drag_update(details);
    }

    void VerticalDragGestureRecognizer::fireEnd(const DragEndDetails& details)
    {
        if (owner_.on_vertical_drag_end) owner_.on_vertical_drag_end(details);
    }

} // namespace systems::leal::campello_widgets
