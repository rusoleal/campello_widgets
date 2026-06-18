#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/render_object.hpp>
#include <campello_widgets/ui/thread_checker.hpp>

#include <algorithm>
#include <iostream>
#include <typeinfo>

namespace systems::leal::campello_widgets
{

    std::atomic<PointerDispatcher*> PointerDispatcher::s_active_dispatcher_{nullptr};

    void PointerDispatcher::setActiveDispatcher(PointerDispatcher* dispatcher) noexcept
    {
        s_active_dispatcher_.store(dispatcher, std::memory_order_release);
    }

    PointerDispatcher* PointerDispatcher::activeDispatcher() noexcept
    {
        return s_active_dispatcher_.load(std::memory_order_acquire);
    }

    PointerDispatcher::PointerDispatcher(std::shared_ptr<RenderBox> root)
        : root_(std::move(root))
    {
    }

    void PointerDispatcher::setRoot(std::shared_ptr<RenderBox> root) noexcept
    {
        root_ = std::move(root);
        active_pointers_.clear();
        captured_pointers_.clear();
        last_hover_path_.clear();
    }

    void PointerDispatcher::addHandler(RenderBox* box, Handler handler)
    {
        handlers_[box] = std::move(handler);
    }

    void PointerDispatcher::removeHandler(RenderBox* box)
    {
        handlers_.erase(box);
    }

    void PointerDispatcher::addTickHandler(RenderBox* box, TickHandler handler)
    {
        tick_handlers_[box] = std::move(handler);
    }

    void PointerDispatcher::removeTickHandler(RenderBox* box)
    {
        tick_handlers_.erase(box);
    }

    bool PointerDispatcher::isBoxAlive(RenderBox* box) noexcept
    {
        return RenderObject::isAlive(box);
    }

    void PointerDispatcher::tick(uint64_t now_ms)
    {
        // Snapshot alive handlers to avoid iterator invalidation if a callback
        // mutates tick_handlers_ (e.g. calls removeTickHandler).
        std::vector<std::pair<RenderBox*, TickHandler>> snapshot;
        snapshot.reserve(tick_handlers_.size());
        for (auto it = tick_handlers_.begin(); it != tick_handlers_.end(); )
        {
            if (!isBoxAlive(it->first))
                it = tick_handlers_.erase(it);
            else
            {
                snapshot.emplace_back(it->first, it->second);
                ++it;
            }
        }
        for (auto& [box, handler] : snapshot)
        {
            if (isBoxAlive(box))
                handler(now_ms);
        }
    }

    void PointerDispatcher::handlePointerEvent(const PointerEvent& event)
    {
        ThreadChecker::instance().assertOnBoundThread("PointerDispatcher::handlePointerEvent");
        // Record position for debug visualization
        if (event.kind == PointerEventKind::down ||
            event.kind == PointerEventKind::move ||
            event.kind == PointerEventKind::up)
        {
            recent_positions_.push_back({event.position.x, event.position.y, 0});
            if (recent_positions_.size() > 16)
                recent_positions_.erase(recent_positions_.begin());
        }

        switch (event.kind)
        {
        case PointerEventKind::down:
        {
            // Hit-test and capture the path for this pointer. Each entry's
            // local_position is exact at this instant (fresh hit test).
            HitTestResult result;
            if (root_) root_->hitTest(result, event.position);

            dispatch(result.path(), event);
            active_pointers_[event.pointer_id] = {result.path(), event.position};
            break;
        }

        case PointerEventKind::move:
        {
            auto it = active_pointers_.find(event.pointer_id);
            if (it != active_pointers_.end())
            {
                // Captured move — dispatch to the same path as the down event.
                // Prune any boxes that were destroyed since the down event.
                auto& path = it->second.path;
                path.erase(
                    std::remove_if(path.begin(), path.end(),
                        [](const HitTestEntry& e) { return !isBoxAlive(e.target); }),
                    path.end());

                if (path.empty())
                    active_pointers_.erase(it);
                else
                {
                    // The path's local_position was captured at down time; the
                    // box hasn't moved since (only translations occur between
                    // ancestor and descendant), so local delta == global delta.
                    const Offset delta_since_down = event.position - it->second.down_position;
                    std::vector<HitTestEntry> adjusted;
                    adjusted.reserve(path.size());
                    for (const auto& entry : path)
                        adjusted.push_back({entry.target, entry.local_position + delta_since_down});

                    dispatch(adjusted, event);
                }
            }
            else
            {
                // Hover move — re-hit-test each call.
                HitTestResult result;
                if (root_) root_->hitTest(result, event.position);

                std::vector<RenderBox*> new_targets;
                new_targets.reserve(result.path().size());
                for (const auto& entry : result.path())
                    new_targets.push_back(entry.target);

                // Send cancel to boxes that were hovered last time but are no longer.
                std::unordered_set<RenderBox*> new_set(new_targets.begin(), new_targets.end());
                PointerEvent cancel_event = event;
                cancel_event.kind = PointerEventKind::cancel;
                auto old_hover = last_hover_path_; // snapshot in case setRoot() is triggered
                for (RenderBox* box : old_hover)
                {
                    if (!isBoxAlive(box))
                        continue;
                    if (new_set.find(box) == new_set.end())
                    {
                        auto hit = handlers_.find(box);
                        if (hit != handlers_.end())
                            hit->second(cancel_event);
                    }
                }

                dispatch(result.path(), event);
                last_hover_path_ = std::move(new_targets);
            }
            break;
        }

        case PointerEventKind::up:
        case PointerEventKind::cancel:
        {
            auto it = active_pointers_.find(event.pointer_id);
            if (it != active_pointers_.end())
            {
                // Copy the path and erase the entry BEFORE dispatching.
                // A handler (e.g. RenderTextField::onPointerEvent) may call
                // releasePointer() which mutates active_pointers_, invalidating
                // the iterator we are about to erase.
                auto path = it->second.path;
                const Offset delta_since_down = event.position - it->second.down_position;
                active_pointers_.erase(it);

                path.erase(
                    std::remove_if(path.begin(), path.end(),
                        [](const HitTestEntry& e) { return !isBoxAlive(e.target); }),
                    path.end());

                if (!path.empty())
                {
                    std::vector<HitTestEntry> adjusted;
                    adjusted.reserve(path.size());
                    for (const auto& entry : path)
                        adjusted.push_back({entry.target, entry.local_position + delta_since_down});

                    dispatch(adjusted, event);
                }
            }
            break;
        }

        case PointerEventKind::scroll:
        {
            // Scroll events are not captured — re-hit-test each time.
            HitTestResult result;
            if (root_) root_->hitTest(result, event.position);

            dispatch(result.path(), event);
            break;
        }
        }
    }

    void PointerDispatcher::dispatch(
        const std::vector<HitTestEntry>& path, const PointerEvent& event)
    {
        // Check if this pointer is captured by a specific box
        auto capture_it = captured_pointers_.find(event.pointer_id);
        if (capture_it != captured_pointers_.end())
        {
            // Only dispatch to the capturing box
            RenderBox* capturing_box = capture_it->second;
            if (!isBoxAlive(capturing_box))
            {
                captured_pointers_.erase(capture_it);
                return;
            }
            auto handler_it = handlers_.find(capturing_box);
            if (handler_it != handlers_.end())
            {
                PointerEvent local_event = event;
                for (const auto& entry : path)
                {
                    if (entry.target == capturing_box)
                    {
                        local_event.local_position = entry.local_position;
                        break;
                    }
                }
                handler_it->second(local_event);
            }
            return;
        }

        // Normal dispatch to all handlers in path
        for (const auto& entry : path)
        {
            if (!isBoxAlive(entry.target))
                continue;
            auto it = handlers_.find(entry.target);
            if (it != handlers_.end())
            {
                PointerEvent local_event = event;
                local_event.local_position = entry.local_position;
                it->second(local_event);
            }
        }
    }

    void PointerDispatcher::capturePointer(int32_t pointer_id, RenderBox* box)
    {
        if (box)
            captured_pointers_[pointer_id] = box;
    }

    void PointerDispatcher::releasePointer(int32_t pointer_id)
    {
        captured_pointers_.erase(pointer_id);
    }

    bool PointerDispatcher::isPointerCaptured(int32_t pointer_id) const
    {
        auto it = captured_pointers_.find(pointer_id);
        if (it == captured_pointers_.end())
            return false;
        return isBoxAlive(it->second);
    }

    RenderBox* PointerDispatcher::getCapturingBox(int32_t pointer_id) const
    {
        auto it = captured_pointers_.find(pointer_id);
        if (it != captured_pointers_.end() && isBoxAlive(it->second))
            return it->second;
        return nullptr;
    }

} // namespace systems::leal::campello_widgets
