#include <campello_widgets/ui/frame_scheduler.hpp>

namespace systems::leal::campello_widgets
{

std::function<void()> FrameScheduler::s_callback_;

void FrameScheduler::setCallback(std::function<void()> callback)
{
    s_callback_ = std::move(callback);
}

void FrameScheduler::scheduleFrame()
{
    if (s_callback_) s_callback_();
}

} // namespace systems::leal::campello_widgets
