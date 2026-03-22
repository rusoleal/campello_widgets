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
    };

} // namespace systems::leal::campello_widgets
