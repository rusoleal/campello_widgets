#include <campello_widgets/ui/post_frame_callbacks.hpp>

namespace systems::leal::campello_widgets
{

    std::mutex                         PostFrameCallbacks::s_mutex_;
    std::vector<std::function<void()>> PostFrameCallbacks::s_pending_;

    void PostFrameCallbacks::schedule(std::function<void()> callback)
    {
        std::lock_guard<std::mutex> lock(s_mutex_);
        s_pending_.push_back(std::move(callback));
    }

    void PostFrameCallbacks::runPending()
    {
        std::vector<std::function<void()>> due;
        {
            std::lock_guard<std::mutex> lock(s_mutex_);
            due.swap(s_pending_);
        }
        for (auto& cb : due)
            if (cb) cb();
    }

} // namespace systems::leal::campello_widgets
