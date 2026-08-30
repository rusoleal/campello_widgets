#pragma once

#include <memory>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/focus_node.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief RenderBox that registers a FocusNode with the active FocusManager.
     *
     * On construction, registers `focus_node` with FocusManager::activeManager()
     * and optionally requests focus immediately (if `auto_focus` is true).
     * On destruction, unregisters the node.
     *
     * Layout and paint pass through to the single child (inherited from RenderBox).
     */
    class RenderFocus : public RenderBox
    {
    public:
        std::shared_ptr<FocusNode> focus_node;
        bool                       auto_focus = false;

        RenderFocus();
        ~RenderFocus();

        /**
         * @brief Passes paint through to the child, then records this
         * node's projected on-screen bounds onto `focus_node` (see
         * `FocusNode::bounds()`) for directional focus navigation.
         */
        void performPaint(PaintContext& context, const Offset& offset) override;

        FocusNode* ownedFocusNode() const noexcept override { return focus_node.get(); }

        /**
         * @brief Links focus_node->parent() to the nearest ancestor
         * FocusNode (see that method's doc comment) -- registration itself
         * still happens in the Focus widget's createRenderObject()/
         * updateRenderObject(), unchanged; this only wires the chain used
         * for key-event bubbling, which needs parent() to already be set
         * (guaranteed true here: RenderObject::setParent() always sets
         * parent_ before calling attach()).
         */
        void attach() override;

        /** @brief Clears the link set by attach() above. */
        void detach() override;
    };

} // namespace systems::leal::campello_widgets
