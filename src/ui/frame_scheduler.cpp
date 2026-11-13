#include <campello_widgets/ui/frame_scheduler.hpp>
#include <campello_widgets/ui/thread_checker.hpp>
#include <campello_widgets/ui/view.hpp>

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

    // Multi-window (Phase 17): callers here have no window context
    // (animations, futures, video frames...), so every window gets asked.
    // A window with nothing dirty skips its frame cheaply (Renderer::
    // buildFrame bails out before painting). Code that knows its window
    // calls View::requestFrame() instead (see RenderObject::markNeedsPaint).
    for (View* view : View::all())
        view->requestFrame();
}

bool FrameScheduler::hasCallback() noexcept
{
    {
        std::lock_guard<std::mutex> lock(s_mutex_);
        if (s_callback_) return true;
    }
    for (View* view : View::all())
        if (view->hasFrameRequestCallback()) return true;
    return false;
}

} // namespace systems::leal::campello_widgets
