#include <campello_widgets/ui/render_box.hpp>

namespace systems::leal::campello_widgets
{

    void RenderBox::setChild(std::shared_ptr<RenderBox> child) noexcept
    {
        if (child_) child_->setParent(nullptr);
        child_ = std::move(child);
        if (child_) child_->setParent(this);
        markNeedsLayout();
    }

    Size RenderBox::layoutChild(RenderBox& child, const BoxConstraints& constraints)
    {
        child.layout(constraints);
        return child.size();
    }

    void RenderBox::positionChild(RenderBox& child, const Offset& offset) noexcept
    {
        (void)child; // offset is stored on the parent, keyed by the child pointer
        child_offset_ = offset;
    }

    void RenderBox::paintChild(PaintContext& context, const Offset& origin) const
    {
        if (!child_) return;
        child_->paint(context, origin + child_offset_);
    }

    void RenderBox::performLayout()
    {
        if (child_)
        {
            const Size child_size = layoutChild(*child_, constraints_.loosen());
            size_         = constraints_.constrain(child_size);
            child_offset_ = Offset{
                (size_.width  - child_size.width)  * 0.5f,
                (size_.height - child_size.height) * 0.5f,
            };
        }
        else
        {
            size_ = constraints_.constrain(Size::zero());
        }
    }

    void RenderBox::performPaint(PaintContext& context, const Offset& offset)
    {
        paintChild(context, offset);
    }

    bool RenderBox::hitTest(HitTestResult& result, const Offset& position)
    {
        if (position.x < 0.0f || position.x >= size_.width ||
            position.y < 0.0f || position.y >= size_.height)
            return false;

        if (hitTestChildren(result, position) || hitTestSelf(position))
        {
            result.add({this, position});
            return true;
        }
        return false;
    }

    bool RenderBox::hitTestSelf(const Offset&) const
    {
        return true;
    }

    bool RenderBox::hitTestChildren(HitTestResult& result, const Offset& position)
    {
        if (!child_) return false;
        return child_->hitTest(result, position - child_offset_);
    }

} // namespace systems::leal::campello_widgets
