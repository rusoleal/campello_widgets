#include <campello_widgets/ui/render_flow.hpp>
#include <campello_widgets/ui/paint_context.hpp>

namespace systems::leal::campello_widgets
{
    using Matrix4 = systems::leal::vector_math::Matrix4<float>;
    using Vector3 = systems::leal::vector_math::Vector3<float>;

    void RenderFlow::insertChild(std::shared_ptr<RenderBox> box, int index)
    {
        if (index < 0) index = 0;
        if (index >= static_cast<int>(children_.size()))
            children_.resize(index + 1);

        if (children_[index] == box) return;

        if (children_[index]) children_[index]->setParent(nullptr);
        children_[index] = std::move(box);
        if (children_[index]) children_[index]->setParent(this);
        markNeedsLayout();
    }

    void RenderFlow::clearChildren()
    {
        for (auto& c : children_)
            if (c) c->setParent(nullptr);
        children_.clear();
        last_transforms_.clear();
        markNeedsLayout();
    }

    void RenderFlow::truncateChildren(size_t count)
    {
        if (count >= children_.size()) return;
        for (size_t i = count; i < children_.size(); ++i)
            if (children_[i]) children_[i]->setParent(nullptr);
        children_.resize(count);
        markNeedsLayout();
    }

    void RenderFlow::performLayout()
    {
        size_ = constraints_.constrain(
            delegate ? delegate->getSize(constraints_)
                     : Size{constraints_.max_width, constraints_.max_height});

        for (size_t i = 0; i < children_.size(); ++i)
        {
            if (!children_[i]) continue;
            const BoxConstraints cc = delegate
                ? delegate->getConstraintsForChild(i, constraints_)
                : constraints_;
            layoutChild(*children_[i], cc);
        }
    }

    void RenderFlow::performPaint(PaintContext& context, const Offset& offset)
    {
        if (!delegate) return;

        if (last_transforms_.size() != children_.size())
            last_transforms_.assign(children_.size(), Matrix4::identity());

        active_paint_context_ = &context;
        active_paint_offset_  = offset;
        delegate->paintChildren(*this);
        active_paint_context_ = nullptr;
    }

    bool RenderFlow::hitTestChildren(HitTestResult& result, const Offset& position)
    {
        // Reverse order: last-painted (topmost) child gets first chance.
        for (size_t ri = children_.size(); ri-- > 0; )
        {
            auto& child = children_[ri];
            if (!child) continue;

            const Matrix4 m = (ri < last_transforms_.size()) ? last_transforms_[ri] : Matrix4::identity();
            const Vector3 local = m.inverted().transform3(Vector3{position.x, position.y, 0.0f});

            if (child->hitTest(result, {local.x(), local.y()}))
                return true;
        }
        return false;
    }

    void RenderFlow::visitRenderChildren(const std::function<void(RenderBox*)>& visitor) const
    {
        for (const auto& c : children_)
            if (c) visitor(c.get());
    }

    Size RenderFlow::childSize(size_t index) const
    {
        if (index >= children_.size() || !children_[index]) return Size::zero();
        return children_[index]->size();
    }

    void RenderFlow::paintChild(size_t index, const Matrix4& transform, float opacity)
    {
        if (!active_paint_context_) return;
        if (index >= children_.size() || !children_[index]) return;

        if (index >= last_transforms_.size())
            last_transforms_.resize(children_.size(), Matrix4::identity());
        last_transforms_[index] = transform;

        Canvas& canvas = active_paint_context_->canvas();
        canvas.save();
        canvas.translate(active_paint_offset_.x, active_paint_offset_.y);
        canvas.transform(transform);
        if (opacity < 1.0f)
            canvas.setOpacity(opacity);
        children_[index]->paint(*active_paint_context_, Offset::zero());
        canvas.restore();
    }

} // namespace systems::leal::campello_widgets
