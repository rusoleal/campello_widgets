#include <campello_widgets/ui/render_nested_scroll_view.hpp>
#include <algorithm>

namespace systems::leal::campello_widgets
{

    // outer_->setParent(this)/inner_->setParent(this) below fire attach()
    // immediately (RenderObject::setParent() triggers attach()/detach()
    // purely on its own parent-pointer transition, not on whether the new
    // parent is itself part of a live tree yet -- confirmed against every
    // other setChild()/insertChild() call site in this codebase, e.g.
    // RenderSliverToBoxAdapter::setChild(), which relies on exactly the same
    // eager-attach behavior and needs no attach()/detach() override of its
    // own either). RenderNestedScrollView never registers anything with
    // PointerDispatcher on its own behalf -- only RenderViewport does that,
    // for itself -- so no attach()/detach() override is needed here.
    RenderNestedScrollView::RenderNestedScrollView()
        : outer_(std::make_shared<RenderViewport>())
        , inner_(std::make_shared<RenderViewport>())
        , coordinator_(std::make_shared<NestedScrollCoordinator>())
    {
        outer_->setParent(this);
        inner_->setParent(this);

        coordinator_->apply_to_outer = [this](float d) { return outer_->applyExternalScrollDelta(d); };
        coordinator_->apply_to_inner = [this](float d) { return inner_->applyExternalScrollDelta(d); };
        outer_->external_delta_redirect = [this](float d) { coordinator_->applyUserOffset(d); };
        inner_->external_delta_redirect = [this](float d) { coordinator_->applyUserOffset(d); };
    }

    void RenderNestedScrollView::performLayout()
    {
        size_ = constraints_.constrain({constraints_.max_width, constraints_.max_height});

        const float outer_h = std::clamp(header_extent, 0.0f, size_.height);
        const float inner_h = size_.height - outer_h;

        outer_->layout(BoxConstraints::tight(size_.width, outer_h));
        inner_->layout(BoxConstraints::tight(size_.width, inner_h));
    }

    void RenderNestedScrollView::performPaint(PaintContext& context, const Offset& offset)
    {
        outer_->paint(context, offset);
        inner_->paint(context, offset + Offset{0.0f, header_extent});
    }

    bool RenderNestedScrollView::hitTestChildren(HitTestResult& result, const Offset& position)
    {
        if (position.y >= header_extent)
            return inner_->hitTest(result, Offset{position.x, position.y - header_extent});
        return outer_->hitTest(result, position);
    }

    void RenderNestedScrollView::visitRenderChildren(const std::function<void(RenderBox*)>& visitor) const
    {
        visitor(outer_.get());
        visitor(inner_.get());
    }

} // namespace systems::leal::campello_widgets
