#pragma once

#include <campello_widgets/widgets/focus.hpp>
#include <campello_widgets/ui/focus_node.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Convenience Focus that auto-creates its own group-flagged
     * FocusNode -- groups a subtree's Tab order into one contiguous block
     * without gating Tab-escape or restoring focus on unmount, orthogonal
     * to FocusScope (see FocusNode::isTraversalGroup()'s doc comment).
     * Mirrors Flutter's FocusTraversalGroup widget.
     *
     * The group's own node is never itself a Tab stop (canRequestFocus is
     * false, skipTraversal is true) -- it exists purely as a marker in the
     * focus tree. To give the group its own ordering policy, set it the
     * same way any scope already does:
     *
     * @code
     * auto group = std::make_shared<FocusTraversalGroup>();
     * group->focus_node->traversal_policy = std::make_shared<ReadingOrderTraversalPolicy>();
     * group->child = toolbarContent;
     * @endcode
     *
     * Reuses 100% of Focus's existing registration lifecycle -- only the
     * constructor differs (auto-creating focus_node and flagging it as a
     * traversal group), so no separate RenderObject/Element class is needed.
     */
    class FocusTraversalGroup : public Focus
    {
    public:
        FocusTraversalGroup()
        {
            focus_node = std::make_shared<FocusNode>();
            focus_node->setTraversalGroup(true);
            focus_node->setCanRequestFocus(false);
            focus_node->setSkipTraversal(true);
        }
    };

} // namespace systems::leal::campello_widgets
