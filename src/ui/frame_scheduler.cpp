#include <campello_widgets/ui/frame_scheduler.hpp>
#include <campello_widgets/ui/thread_checker.hpp>

namespace systems::leal::campello_widgets
{

std::mutex FrameScheduler::s_mutex_;
std::function<void()> FrameScheduler::s_callback_;

void FrameScheduler::setCallback(std::function<void()> callback)
{
    std::lock_guard<std::mutex> lock(s_mutex_);
    s_callback_ = std::move(callback);
}

void FrameScheduler::scheduleFrame()
{
    ThreadChecker::instance().assertOnBoundThread("FrameScheduler::scheduleFrame");
    std::function<void()> cb;
    {
        std::lock_guard<std::mutex> lock(s_mutex_);
        cb = s_callback_;
    }
    if (cb) cb();
}

bool FrameScheduler::hasCallback() noexcept
{
    std::lock_guard<std::mutex> lock(s_mutex_);
    return static_cast<bool>(s_callback_);
}

} // namespace systems::leal::campello_widgets
