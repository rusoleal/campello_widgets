#include <campello_widgets/ui/render_focus.hpp>
#include <campello_widgets/ui/focus_manager.hpp>
#include <campello_widgets/ui/dirty_region.hpp>

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

    void RenderFocus::performPaint(PaintContext& context, const Offset& offset)
    {
        if (focus_node)
        {
            // Same projection RenderGestureDetector::performPaint() uses for
            // its globalOffset() (see that doc comment) — accounts for the
            // safe-area inset baked into `offset` and any ambient canvas
            // transform (e.g. a scrolled ancestor), so directional focus
            // navigation compares nodes in one consistent coordinate space
            // regardless of where in the tree they live.
            const Offset paint_origin = RenderObject::activePaintOriginOffset();
            const Rect   local_bounds = Rect::fromLTWH(
                offset.x - paint_origin.x, offset.y - paint_origin.y,
                size_.width, size_.height);
            focus_node->bounds_ = projectedBounds(context.canvas().currentTransform(), local_bounds);
        }

        if (child_) paintChild(context, offset);
    }

} // namespace systems::leal::campello_widgets
