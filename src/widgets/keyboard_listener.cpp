#include <campello_widgets/widgets/keyboard_listener.hpp>
#include <campello_widgets/widgets/focus.hpp>

namespace systems::leal::campello_widgets
{

    WidgetRef KeyboardListener::build(BuildContext&) const
    {
        if (focus_node && on_key_event)
        {
            auto cb = on_key_event;
            focus_node->on_key = [cb](const KeyEvent& e) {
                cb(e);
                return false; // observer only — never consumes the event
            };
        }

        auto w        = std::make_shared<Focus>();
        w->focus_node = focus_node;
        w->auto_focus = auto_focus;
        w->child      = child;
        return w;
    }

} // namespace systems::leal::campello_widgets
