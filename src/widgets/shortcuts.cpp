#include <campello_widgets/widgets/shortcuts.hpp>
#include <campello_widgets/widgets/focus.hpp>

namespace systems::leal::campello_widgets
{

    WidgetRef Shortcuts::build(BuildContext&) const
    {
        if (focus_node)
        {
            const auto bound = bindings;
            focus_node->on_key = [bound](const KeyEvent& e) -> bool
            {
                if (e.kind == KeyEventKind::up) return false;
                for (const auto& [combo, cb] : bound)
                {
                    if (combo.key_code == e.key_code && combo.modifiers == e.modifiers)
                    {
                        if (cb) cb();
                        return true;
                    }
                }
                return false;
            };
        }

        auto w        = std::make_shared<Focus>();
        w->focus_node = focus_node;
        w->auto_focus = auto_focus;
        w->child      = child;
        return w;
    }

} // namespace systems::leal::campello_widgets
