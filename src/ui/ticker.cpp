#include <campello_widgets/ui/ticker.hpp>

namespace systems::leal::campello_widgets
{

    TickerScheduler* TickerScheduler::s_active_ = nullptr;

    void TickerScheduler::tick(uint64_t now_ms)
    {
        // Snapshot the map so callbacks can safely call unsubscribe() during dispatch.
        auto snapshot = listeners_;
        for (auto& [id, cb] : snapshot)
        {
            if (listeners_.count(id))
                cb(now_ms);
        }
    }

    uint64_t TickerScheduler::subscribe(std::function<void(uint64_t)> callback)
    {
        const uint64_t id = next_id_++;
        listeners_.emplace(id, std::move(callback));
        return id;
    }

    void TickerScheduler::unsubscribe(uint64_t id)
    {
        listeners_.erase(id);
    }

} // namespace systems::leal::campello_widgets
