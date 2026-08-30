#pragma once

#include <vector>
#include <utility>
#include <functional>
#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/ui/focus_node.hpp>
#include <campello_widgets/ui/key_combo.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Maps key combinations to callbacks over its child subtree.
     *
     * Sits on the same key-bubbling `FocusNode::on_key` + `FocusManager`
     * mechanism `KeyboardListener` does (see its identical "caller owns the
     * FocusNode" convention, followed here for the same reason: a
     * StatelessWidget's `build()` can be called many times, and a FocusNode
     * has real registration lifecycle -- creating a new one every rebuild
     * would churn focus state). A matched combo consumes the event (returns
     * true), stopping it from bubbling further.
     *
     * No separate `Intent`/`Actions` split -- unlike Flutter, nothing in
     * this codebase needs multiple `Shortcuts` scopes rebinding the same
     * semantic action to different keys, which is the only reason Flutter's
     * version separates the two. A key combo maps directly to a callback.
     *
     * Matches (a simplified version of) Flutter's `Shortcuts` widget.
     *
     * @code
     * auto node = std::make_shared<FocusNode>();
     *
     * auto w = std::make_shared<Shortcuts>();
     * w->focus_node = node;
     * w->bindings = {
     *     {KeyCombo{KeyCode::s, KeyModifiers::ctrl}, [] { save(); }},
     *     {KeyCombo{KeyCode::escape}, [] { cancel(); }},
     * };
     * w->child = myChild;
     * @endcode
     */
    class Shortcuts : public StatelessWidget
    {
    public:
        std::shared_ptr<FocusNode> focus_node;
        bool                       auto_focus = false;
        std::vector<std::pair<KeyCombo, std::function<void()>>> bindings;
        WidgetRef                  child;

        WidgetRef build(BuildContext& ctx) const override;
    };

} // namespace systems::leal::campello_widgets
