#include <campello_widgets/ui/render_indexed_stack.hpp>

namespace systems::leal::campello_widgets
{

    void RenderIndexedStack::performPaint(PaintContext& context, const Offset& offset)
    {
        if (index < 0 || static_cast<size_t>(index) >= childCount()) return;
        if (auto* box = boxAt(static_cast<size_t>(index)))
        {
            const Offset child_offset = offsetAt(static_cast<size_t>(index));
            box->paint(context, {offset.x + child_offset.x, offset.y + child_offset.y});
        }
    }

    bool RenderIndexedStack::hitTestChildren(HitTestResult& result, const Offset& position)
    {
        if (index < 0 || static_cast<size_t>(index) >= childCount()) return false;
        auto* box = boxAt(static_cast<size_t>(index));
        if (!box) return false;
        return box->hitTest(result, position - offsetAt(static_cast<size_t>(index)));
    }

} // namespace systems::leal::campello_widgets
