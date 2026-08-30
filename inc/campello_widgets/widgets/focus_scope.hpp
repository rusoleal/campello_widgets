#pragma once

#include <campello_widgets/widgets/focus.hpp>
#include <campello_widgets/ui/focus_node.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Convenience Focus that auto-creates its own scope-flagged
     * FocusNode -- see Focus::scope's doc comment and
     * FocusNode::isScope()'s doc comment for what a scope actually does.
     *
     * Wrap a dialog/popup/modal's content in one so Tab/Shift+Tab and
     * directional focus movement can't leak into the rest of the app
     * behind it, and so closing it (unmounting) automatically restores
     * focus to whatever was focused immediately before it opened. Mirrors
     * Flutter's FocusScope widget.
     *
     * Reuses 100% of Focus's existing registration lifecycle -- only the
     * constructor differs (auto-creating focus_node and setting scope),
     * so no separate RenderObject/Element class is needed.
     *
     * @code
     * auto scope = std::make_shared<FocusScope>();
     * scope->auto_focus = true;
     * scope->child = dialogContent;
     * @endcode
     */
    class FocusScope : public Focus
    {
    public:
        FocusScope()
        {
            focus_node = std::make_shared<FocusNode>();
            // Set directly here, not just via the inherited `scope` flag
            // Focus::createRenderObject()/updateRenderObject() apply at
            // mount time -- unlike base Focus (where focus_node is
            // externally supplied and could still be reassigned before
            // mounting), FocusScope always auto-creates a fresh node
            // exclusively for itself, so isScope() can and should already
            // be true the instant the node exists, not only once mounted.
            focus_node->setScope(true);
            scope = true;
        }
    };

} // namespace systems::leal::campello_widgets
