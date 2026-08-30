#include <campello_widgets/ui/render_custom_multi_child_layout.hpp>

namespace systems::leal::campello_widgets
{

    RenderCustomMultiChildLayout::Entry* RenderCustomMultiChildLayout::findEntry(const std::string& id)
    {
        for (auto& e : children_)
            if (e.id == id) return &e;
        return nullptr;
    }

    void RenderCustomMultiChildLayout::insertChild(const std::string& id, std::shared_ptr<RenderBox> box)
    {
        if (auto* existing = findEntry(id))
        {
            if (existing->box == box) return;
            if (existing->box) existing->box->setParent(nullptr);
            existing->box = std::move(box);
            if (existing->box) existing->box->setParent(this);
            markNeedsLayout();
            return;
        }

        if (box) box->setParent(this);
        children_.push_back({id, std::move(box), {}});
        markNeedsLayout();
    }

    void RenderCustomMultiChildLayout::clearChildren()
    {
        for (auto& e : children_)
            if (e.box) e.box->setParent(nullptr);
        children_.clear();
        markNeedsLayout();
    }

    void RenderCustomMultiChildLayout::performLayout()
    {
        size_ = constraints_.constrain(
            delegate ? delegate->getSize(constraints_)
                     : Size{constraints_.max_width, constraints_.max_height});

        if (delegate)
        {
            delegate->ctx_ = this;
            delegate->performLayout(size_);
            delegate->ctx_ = nullptr;
        }
    }

    void RenderCustomMultiChildLayout::performPaint(PaintContext& context, const Offset& offset)
    {
        for (const auto& e : children_)
            if (e.box)
                e.box->paint(context, {offset.x + e.offset.x, offset.y + e.offset.y});
    }

    bool RenderCustomMultiChildLayout::hitTestChildren(HitTestResult& result, const Offset& position)
    {
        for (auto it = children_.rbegin(); it != children_.rend(); ++it)
        {
            if (!it->box) continue;
            if (it->box->hitTest(result, position - it->offset))
                return true;
        }
        return false;
    }

    void RenderCustomMultiChildLayout::visitRenderChildren(const std::function<void(RenderBox*)>& visitor) const
    {
        for (const auto& e : children_)
            if (e.box) visitor(e.box.get());
    }

    bool RenderCustomMultiChildLayout::hasChild(const std::string& id) const
    {
        for (const auto& e : children_)
            if (e.id == id) return e.box != nullptr;
        return false;
    }

    Size RenderCustomMultiChildLayout::layoutChild(const std::string& id, const BoxConstraints& constraints)
    {
        auto* e = findEntry(id);
        if (!e || !e->box) return Size::zero();
        return RenderBox::layoutChild(*e->box, constraints);
    }

    void RenderCustomMultiChildLayout::positionChild(const std::string& id, const Offset& offset)
    {
        if (auto* e = findEntry(id))
            e->offset = offset;
    }

} // namespace systems::leal::campello_widgets
