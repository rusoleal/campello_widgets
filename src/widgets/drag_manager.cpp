#include <campello_widgets/ui/drag_details.hpp>

#include <algorithm>

namespace systems::leal::campello_widgets
{

    // -------------------------------------------------------------------------

    void DragManager::startSession(std::type_index type, const void* data, Offset pos)
    {
        dragging_     = true;
        current_type_ = type;
        current_data_ = data;
        current_pos_  = pos;

        // Reset all hover states
        for (auto& rec : targets_)
            rec.hovered = false;
    }

    void DragManager::updatePosition(Offset pos)
    {
        if (!dragging_) return;
        current_pos_ = pos;
        checkHovers(pos);
    }

    bool DragManager::endSession()
    {
        if (!dragging_) return false;

        bool accepted = false;
        for (auto& rec : targets_)
        {
            if (rec.hovered && rec.will_accept && rec.will_accept())
            {
                if (rec.on_accept) rec.on_accept();
                accepted = true;
                break; // first accepting target wins
            }
        }

        // Fire on_exit for everything that was hovered
        for (auto& rec : targets_)
        {
            if (rec.hovered)
            {
                if (rec.on_exit) rec.on_exit();
                rec.hovered = false;
            }
        }

        dragging_     = false;
        current_data_ = nullptr;
        return accepted;
    }

    // -------------------------------------------------------------------------

    void DragManager::registerTarget(void*                  id,
                                     std::function<bool()>  will_accept,
                                     std::function<void()>  on_enter,
                                     std::function<void()>  on_exit,
                                     std::function<void()>  on_accept)
    {
        // Remove any stale entry for the same id
        unregisterTarget(id);
        targets_.push_back({id, Rect{}, std::move(will_accept),
                            std::move(on_enter), std::move(on_exit),
                            std::move(on_accept), false});
    }

    void DragManager::unregisterTarget(void* id)
    {
        targets_.erase(
            std::remove_if(targets_.begin(), targets_.end(),
                           [id](const TargetRecord& r) { return r.id == id; }),
            targets_.end());
    }

    void DragManager::updateTargetBounds(void* id, const Rect& bounds)
    {
        if (auto* rec = findTarget(id))
            rec->bounds = bounds;
    }

    // -------------------------------------------------------------------------

    void DragManager::checkHovers(Offset pos)
    {
        for (auto& rec : targets_)
        {
            bool inside = rec.bounds.contains(pos.x, pos.y);
            bool accepts = rec.will_accept && rec.will_accept();

            if (inside && accepts && !rec.hovered)
            {
                rec.hovered = true;
                if (rec.on_enter) rec.on_enter();
            }
            else if ((!inside || !accepts) && rec.hovered)
            {
                rec.hovered = false;
                if (rec.on_exit) rec.on_exit();
            }
        }
    }

    DragManager::TargetRecord* DragManager::findTarget(void* id)
    {
        for (auto& rec : targets_)
            if (rec.id == id) return &rec;
        return nullptr;
    }

} // namespace systems::leal::campello_widgets
