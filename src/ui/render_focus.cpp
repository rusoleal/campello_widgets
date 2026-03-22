#include <campello_widgets/ui/render_focus.hpp>
#include <campello_widgets/ui/focus_manager.hpp>

namespace systems::leal::campello_widgets
{

    RenderFocus::RenderFocus()
    {
        // focus_node is set by the element via updateRenderObject before
        // this object is first used, so registration is deferred — the Focus
        // widget sets focus_node then manually registers if needed.
        // We register at the end of updateRenderObject via the Focus widget.
    }

    RenderFocus::~RenderFocus()
    {
        if (focus_node)
        {
            if (auto* m = FocusManager::activeManager())
                m->unregisterNode(focus_node.get());
        }
    }

} // namespace systems::leal::campello_widgets
