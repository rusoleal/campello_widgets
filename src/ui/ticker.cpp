#include <campello_widgets/ui/ticker.hpp>
#include <campello_widgets/ui/frame_scheduler.hpp>

namespace systems::leal::campello_widgets
{

    TickerScheduler* TickerScheduler::s_active_ = nullptr;

    TickerScheduler* TickerScheduler::active() noexcept
    {
        return s_active_;
    }

    void TickerScheduler::setActive(TickerScheduler* scheduler) noexcept
    {
        s_active_ = scheduler;
    }

    void TickerScheduler::tick(uint64_t now_ms)
    {
        // Snapshot the map so callbacks can safely call unsubscribe() during dispatch.
        auto snapshot = listeners_;
        for (auto& [id, cb] : snapshot)
        {
            if (listeners_.count(id))
                cb(now_ms);
        }
        // If there are still active tickers after the tick (e.g. running
        // animations), request the next frame so the loop continues — mirroring
        // Flutter's Ticker which reschedules itself each frame while active.
        if (!listeners_.empty())
            FrameScheduler::scheduleFrame();
    }

    uint64_t TickerScheduler::subscribe(std::function<void(uint64_t)> callback)
    {
        const uint64_t id = next_id_++;
        listeners_.emplace(id, std::move(callback));
        // A new animation just started — request the first frame for it.
        FrameScheduler::scheduleFrame();
        return id;
    }

    void TickerScheduler::unsubscribe(uint64_t id)
    {
        listeners_.erase(id);
    }

} // namespace systems::leal::campello_widgets
