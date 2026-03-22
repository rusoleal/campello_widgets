#include <algorithm>
#include <campello_widgets/ui/scroll_controller.hpp>
#include <campello_widgets/ui/animation_controller.hpp>

namespace systems::leal::campello_widgets
{

    ScrollController::~ScrollController()
    {
        if (anim_ && anim_listener_id_)
            anim_->removeListener(anim_listener_id_);
    }

    void ScrollController::jumpTo(float offset)
    {
        if (anim_) anim_->stop();
        setOffset(offset);
    }

    void ScrollController::animateTo(float offset, double duration_ms)
    {
        const float target = std::clamp(offset, min_extent_, max_extent_);
        if (target == offset_) return;

        if (anim_ && anim_listener_id_)
            anim_->removeListener(anim_listener_id_);

        anim_ = std::make_unique<AnimationController>(
            duration_ms,
            static_cast<double>(offset_),  // lower = current position
            static_cast<double>(target));   // upper = target position

        anim_listener_id_ = anim_->addListener([this]()
        {
            setOffset(static_cast<float>(anim_->value()));
        });

        anim_->forward();
    }

    void ScrollController::setExtents(float min_extent, float max_extent) noexcept
    {
        min_extent_ = min_extent;
        max_extent_ = max_extent;
    }

    uint64_t ScrollController::addListener(std::function<void()> fn)
    {
        const uint64_t id = next_listener_id_++;
        listeners_.emplace_back(id, std::move(fn));
        return id;
    }

    void ScrollController::removeListener(uint64_t id)
    {
        auto it = std::find_if(listeners_.begin(), listeners_.end(),
            [id](const auto& p) { return p.first == id; });
        if (it != listeners_.end())
            listeners_.erase(it);
    }

    void ScrollController::setOffset(float offset)
    {
        const float clamped = std::clamp(offset, min_extent_, max_extent_);
        if (clamped == offset_) return;
        offset_ = clamped;
        notifyListeners();
    }

    void ScrollController::notifyListeners()
    {
        for (auto& [id, fn] : listeners_)
            fn();
    }

} // namespace systems::leal::campello_widgets
