#include <campello_widgets/ui/gesture_recognizer.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>

namespace systems::leal::campello_widgets
{

    void GestureRecognizer::dispose(PointerDispatcher* dispatcher)
    {
        if (auto* d = dispatcher ? dispatcher : PointerDispatcher::activeDispatcher())
            d->arena().removeMember(this);
    }

} // namespace systems::leal::campello_widgets
