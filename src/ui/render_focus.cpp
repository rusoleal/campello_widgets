#include <campello_widgets/ui/render_focus.hpp>
#include <campello_widgets/ui/focus_manager.hpp>
#include <cstdio>

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

    void RenderFocus::attach()
    {
        if (focus_node) focus_node->setOwner(this);
    }

    void RenderFocus::detach()
    {
        if (focus_node) focus_node->setOwner(nullptr);
    }

    void RenderFocus::performPaint(PaintContext& context, const Offset& offset)
    {
        // Same projection RenderGestureDetector::performPaint() uses for its
        // globalOffset() (see RenderBox::computeGlobalRect()'s doc comment) —
        // accounts for the safe-area inset and any ambient canvas transform
        // (e.g. a scrolled ancestor), so directional focus navigation
        // compares nodes in one consistent coordinate space regardless of
        // where in the tree they live.
        if (focus_node)
            focus_node->bounds_ = computeGlobalRect(context, offset);

        if (child_) paintChild(context, offset);
    }

} // namespace systems::leal::campello_widgets
