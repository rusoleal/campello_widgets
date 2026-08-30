#include <campello_widgets/ui/render_box.hpp>

namespace systems::leal::campello_widgets
{

    std::optional<float> RenderBox::computeDistanceToActualBaseline(TextBaseline baseline) const
    {
        if (!child_) return std::nullopt;
        const auto child_baseline = child_->computeDistanceToActualBaseline(baseline);
        if (!child_baseline) return std::nullopt;
        return *child_baseline + child_offset_.y;
    }

    void RenderBox::setChild(std::shared_ptr<RenderBox> child) noexcept
    {
        if (child_ == child) return;
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
        // Mirrors Flutter's RenderBox.hitTestSelf default (false): a plain
        // layout box (Align, Padding, Center, SizedBox, ...) is transparent
        // to pointer events at points where none of its children claimed
        // the hit. Only widgets that actually register their own pointer
        // handling (GestureDetector, TextField, Slider, Draggable,
        // scrollables, MouseRegion, ...) override this to true. Without
        // that split, an oversized invisible wrapper (e.g. an Align sized
        // to fill its parent via StackFit::expand so it can center a small
        // child) would swallow every tap within its bounds — including taps
        // nowhere near its visible content — silently stealing hits from
        // whatever sits behind it in a Stack (e.g. a ModalBarrier meant to
        // dismiss on outside taps).
        return false;
    }

    bool RenderBox::hitTestChildren(HitTestResult& result, const Offset& position)
    {
        if (!child_) return false;
        return child_->hitTest(result, position - child_offset_);
    }

    std::vector<std::shared_ptr<DiagnosticsNode>> RenderBox::debugDescribeChildren() const
    {
        std::vector<std::shared_ptr<DiagnosticsNode>> result;
        visitRenderChildren([&](RenderBox* child) {
            result.push_back(child->toDiagnosticsNode());
        });
        return result;
    }

    void RenderBox::visitRenderChildren(const std::function<void(RenderBox*)>& visitor) const
    {
        if (child_)
            visitor(child_.get());
    }

} // namespace systems::leal::campello_widgets
