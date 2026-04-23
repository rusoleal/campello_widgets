#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/widgets/stateful_element.hpp>
#include <campello_widgets/ui/frame_scheduler.hpp>

namespace systems::leal::campello_widgets
{

    void StateBase::setState(std::function<void()> fn)
    {
        fn();
        if (element_ && element_->mounted())
            element_->scheduleBuild();
        // Request a frame from the platform display system, mirroring
        // Flutter's SchedulerBinding.scheduleFrame().  This is idempotent —
        // calling it multiple times before the frame fires is a no-op.
        FrameScheduler::scheduleFrame();
    }

    std::shared_ptr<Element> StatefulWidget::createElement() const
    {
        return std::make_shared<StatefulElement>(
            std::static_pointer_cast<const StatefulWidget>(shared_from_this()));
    }

} // namespace systems::leal::campello_widgets
