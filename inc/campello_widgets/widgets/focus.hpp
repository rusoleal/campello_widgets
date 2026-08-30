#pragma once

#include <memory>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/focus_node.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A widget that registers a FocusNode with the active FocusManager.
     *
     * Wraps a child widget and associates a FocusNode with it. Keyboard events
     * are routed to `focus_node->on_key` when this node has focus.
     *
     * Usage:
     * @code
     * auto node = std::make_shared<FocusNode>();
     * node->on_key = [](const KeyEvent& e) {
     *     // handle key
     *     return true; // consumed
     * };
     *
     * auto w = std::make_shared<Focus>();
     * w->focus_node = node;
     * w->auto_focus = true;
     * w->child      = myChild;
     * @endcode
     */
    class Focus : public SingleChildRenderObjectWidget
    {
    public:
        std::shared_ptr<FocusNode> focus_node;
        bool                       auto_focus = false;

        /**
         * @brief When true, marks focus_node as a traversal/restoration
         * scope boundary before it registers -- see
         * FocusNode::isScope()'s doc comment. Prefer the FocusScope
         * convenience widget (focus_scope.hpp) for new code, which also
         * auto-creates focus_node when none is supplied; this flag exists
         * so an already externally-owned FocusNode can opt in too.
         */
        bool scope = false;

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& render_object) const override;
    };

} // namespace systems::leal::campello_widgets
