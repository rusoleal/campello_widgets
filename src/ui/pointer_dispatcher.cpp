#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/render_box.hpp>

namespace systems::leal::campello_widgets
{

    PointerDispatcher* PointerDispatcher::s_active_dispatcher_ = nullptr;

    void PointerDispatcher::setActiveDispatcher(PointerDispatcher* dispatcher) noexcept
    {
        s_active_dispatcher_ = dispatcher;
    }

    PointerDispatcher* PointerDispatcher::activeDispatcher() noexcept
    {
        return s_active_dispatcher_;
    }

    PointerDispatcher::PointerDispatcher(std::shared_ptr<RenderBox> root)
        : root_(std::move(root))
    {
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

    void PointerDispatcher::tick(uint64_t now_ms)
    {
        for (auto& [box, handler] : tick_handlers_)
            handler(now_ms);
    }

    void PointerDispatcher::handlePointerEvent(const PointerEvent& event)
    {
        switch (event.kind)
        {
        case PointerEventKind::down:
        {
            // Hit-test and capture the path for this pointer.
            HitTestResult result;
            if (root_) root_->hitTest(result, event.position);

            std::vector<RenderBox*> path;
            path.reserve(result.path().size());
            for (const auto& entry : result.path())
                path.push_back(entry.target);

            dispatch(path, event);
            active_pointers_[event.pointer_id] = {std::move(path)};
            break;
        }

        case PointerEventKind::move:
        {
            auto it = active_pointers_.find(event.pointer_id);
            if (it != active_pointers_.end())
            {
                // Captured move — dispatch to the same path as the down event.
                dispatch(it->second.path, event);
            }
            else
            {
                // Hover move — re-hit-test each call.
                HitTestResult result;
                if (root_) root_->hitTest(result, event.position);

                std::vector<RenderBox*> new_path;
                new_path.reserve(result.path().size());
                for (const auto& entry : result.path())
                    new_path.push_back(entry.target);

                // Send cancel to boxes that were hovered last time but are no longer.
                std::unordered_set<RenderBox*> new_set(new_path.begin(), new_path.end());
                PointerEvent cancel_event = event;
                cancel_event.kind = PointerEventKind::cancel;
                for (RenderBox* box : last_hover_path_)
                {
                    if (new_set.find(box) == new_set.end())
                    {
                        auto hit = handlers_.find(box);
                        if (hit != handlers_.end())
                            hit->second(cancel_event);
                    }
                }

                dispatch(new_path, event);
                last_hover_path_ = std::move(new_path);
            }
            break;
        }

        case PointerEventKind::up:
        case PointerEventKind::cancel:
        {
            auto it = active_pointers_.find(event.pointer_id);
            if (it != active_pointers_.end())
            {
                dispatch(it->second.path, event);
                active_pointers_.erase(it);
            }
            break;
        }

        case PointerEventKind::scroll:
        {
            // Scroll events are not captured — re-hit-test each time.
            HitTestResult result;
            if (root_) root_->hitTest(result, event.position);

            std::vector<RenderBox*> path;
            path.reserve(result.path().size());
            for (const auto& entry : result.path())
                path.push_back(entry.target);

            dispatch(path, event);
            break;
        }
        }
    }

    void PointerDispatcher::dispatch(
        const std::vector<RenderBox*>& path, const PointerEvent& event)
    {
        for (RenderBox* box : path)
        {
            auto it = handlers_.find(box);
            if (it != handlers_.end())
                it->second(event);
        }
    }

} // namespace systems::leal::campello_widgets
