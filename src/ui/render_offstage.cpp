#include <campello_widgets/ui/render_offstage.hpp>
#include <campello_widgets/ui/paint_context.hpp>

namespace systems::leal::campello_widgets
{

    RenderOffstage::RenderOffstage(bool offstage)
        : offstage_(offstage)
    {
    }

    void RenderOffstage::setOffstage(bool offstage) noexcept
    {
        if (offstage_ == offstage) return;
        offstage_ = offstage;
        // Toggling offstage changes this node's reported size (zero vs the
        // child's real size), unlike Opacity's setOpacity() (which only
        // needs markNeedsPaint() since opacity never affects layout).
        // markNeedsLayout() already implies markNeedsPaint() too -- see its
        // own doc comment.
        markNeedsLayout();
    }

    void RenderOffstage::performLayout()
    {
        if (offstage_)
        {
            // Do not lay out the child at all while offstage -- it
            // contributes no space to this node's parent, matching
            // Flutter's RenderOffstage. The child's own constraints_/size_
            // simply stay whatever they were from its last real layout;
            // layout() re-derives them correctly on its own the moment
            // this flips back (see RenderObject::layout()'s constraints-
            // changed check).
            size_ = constraints_.constrain(Size::zero());
            return;
        }

        if (child_)
        {
            layoutChild(*child_, constraints_);
            size_ = child_->size();
        }
        else
        {
            size_ = constraints_.constrain(Size::zero());
        }
    }

    void RenderOffstage::performPaint(PaintContext& context, const Offset& offset)
    {
        if (!child_) return;
        paintChild(context, offset);
    }

    void RenderOffstage::paint(PaintContext& context, const Offset& offset)
    {
        if (offstage_)
        {
            // Never visited: no draw commands recorded, no descendant
            // RenderRepaintBoundary touched (so its cache can't go stale
            // relative to an ambient state it was never painted under --
            // see class doc comment), no GPU work at raster time. Dirty
            // flags still need clearing, or a pending markNeedsPaint()
            // would keep requesting new frames forever for content that
            // will never actually paint.
            needs_paint_ = false;
            needs_descendant_paint_ = false;
            return;
        }
        RenderObject::paint(context, offset);
    }

    bool RenderOffstage::hitTestChildren(HitTestResult& result, const Offset& position)
    {
        if (offstage_) return false;
        return RenderBox::hitTestChildren(result, position);
    }

} // namespace systems::leal::campello_widgets
