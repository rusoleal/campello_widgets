#pragma once

#include <functional>
#include <memory>
#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/ui/focus_node.hpp>
#include <campello_widgets/ui/key_event.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A widget that calls a callback for every keyboard event while focused.
     *
     * Mirrors Flutter's `KeyboardListener`. The callback is declared directly on
     * the widget rather than on a separate `FocusNode`, and it never consumes the
     * event — it is an observer, not an interceptor.
     *
     * Internally builds a `Focus` widget wired to `focus_node`. The caller is
     * responsible for creating and managing the `FocusNode`.
     *
     * Usage:
     * @code
     * auto node = std::make_shared<FocusNode>();
     *
     * auto w        = std::make_shared<KeyboardListener>();
     * w->focus_node = node;
     * w->auto_focus = true;
     * w->on_key_event = [](const KeyEvent& e) {
     *     if (e.kind == KeyEventKind::down && e.key_code == KeyCode::space) {
     *         // react to spacebar
     *     }
     * };
     * w->child = myChild;
     * @endcode
     */
    class KeyboardListener : public StatelessWidget
    {
    public:
        std::shared_ptr<FocusNode>           focus_node;
        bool                                 auto_focus  = false;
        std::function<void(const KeyEvent&)> on_key_event;
        WidgetRef                            child;

        WidgetRef build(BuildContext& ctx) const override;
    };

} // namespace systems::leal::campello_widgets
