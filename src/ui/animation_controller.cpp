#include <campello_widgets/ui/animation_controller.hpp>
#include <campello_widgets/ui/ticker.hpp>

#include <algorithm>
#include <cmath>

namespace systems::leal::campello_widgets
{

    AnimationController::AnimationController(double duration_ms,
                                             double lower,
                                             double upper)
        : duration_ms_(duration_ms)
        , lower_(lower)
        , upper_(upper)
        , value_(lower)
    {
    }

    AnimationController::~AnimationController()
    {
        stopTicker();
    }

    // ------------------------------------------------------------------
    // State
    // ------------------------------------------------------------------

    bool AnimationController::isAnimating() const noexcept
    {
        return status_ == AnimationStatus::forward ||
               status_ == AnimationStatus::reverse;
    }

    double AnimationController::normalizedValue() const noexcept
    {
        const double range = upper_ - lower_;
        if (range == 0.0) return 0.0;
        return (value_ - lower_) / range;
    }

    // ------------------------------------------------------------------
    // Control
    // ------------------------------------------------------------------

    void AnimationController::forward(double from)
    {
        if (from >= 0.0)
            value_ = std::clamp(from, lower_, upper_);

        status_      = AnimationStatus::forward;
        last_tick_ms_ = 0; // reset so next tick establishes baseline
        startTicker();
        notifyListeners();
    }

    void AnimationController::reverse(double from)
    {
        if (from >= 0.0)
            value_ = std::clamp(from, lower_, upper_);

        status_      = AnimationStatus::reverse;
        last_tick_ms_ = 0;
        startTicker();
        notifyListeners();
    }

    void AnimationController::stop()
    {
        if (isAnimating())
        {
            status_ = (value_ >= upper_) ? AnimationStatus::completed
                                         : AnimationStatus::dismissed;
        }
        stopTicker();
    }

    void AnimationController::reset()
    {
        stopTicker();
        value_  = lower_;
        status_ = AnimationStatus::dismissed;
        notifyListeners();
    }

    // ------------------------------------------------------------------
    // Listeners
    // ------------------------------------------------------------------

    uint64_t AnimationController::addListener(std::function<void()> fn)
    {
        const uint64_t id = next_listener_id_++;
        listeners_.emplace_back(id, std::move(fn));
        return id;
    }

    void AnimationController::removeListener(uint64_t id)
    {
        listeners_.erase(
            std::remove_if(listeners_.begin(), listeners_.end(),
                           [id](const auto& p) { return p.first == id; }),
            listeners_.end());
    }

    // ------------------------------------------------------------------
    // Private
    // ------------------------------------------------------------------

    void AnimationController::onTick(uint64_t now_ms)
    {
        if (last_tick_ms_ == 0)
        {
            last_tick_ms_ = now_ms;
            return; // first tick: establish baseline, don't advance yet
        }

        const double dt    = static_cast<double>(now_ms - last_tick_ms_);
        last_tick_ms_       = now_ms;
        const double range  = upper_ - lower_;
        const double step   = (duration_ms_ > 0.0) ? (dt / duration_ms_ * range) : range;

        if (status_ == AnimationStatus::forward)
        {
            value_ = std::min(upper_, value_ + step);
            if (value_ >= upper_)
            {
                value_  = upper_;
                status_ = AnimationStatus::completed;
                stopTicker();
            }
        }
        else if (status_ == AnimationStatus::reverse)
        {
            value_ = std::max(lower_, value_ - step);
            if (value_ <= lower_)
            {
                value_  = lower_;
                status_ = AnimationStatus::dismissed;
                stopTicker();
            }
        }

        notifyListeners();
    }

    void AnimationController::startTicker()
    {
        if (ticker_id_ != 0) return; // already subscribed

        auto* scheduler = TickerScheduler::active();
        if (!scheduler) return;

        ticker_id_ = scheduler->subscribe(
            [this](uint64_t now_ms) { onTick(now_ms); });
    }

    void AnimationController::stopTicker()
    {
        if (ticker_id_ == 0) return;

        auto* scheduler = TickerScheduler::active();
        if (scheduler) scheduler->unsubscribe(ticker_id_);
        ticker_id_    = 0;
        last_tick_ms_ = 0;
    }

    void AnimationController::notifyListeners()
    {
        // Snapshot so listeners can safely remove themselves during notification.
        auto snapshot = listeners_;
        for (auto& [id, fn] : snapshot)
            fn();
    }

} // namespace systems::leal::campello_widgets
