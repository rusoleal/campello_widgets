#include <vector>
#include <campello_widgets/widgets/custom_scroll_view.hpp>
#include <campello_widgets/ui/render_viewport.hpp>
#include <campello_widgets/ui/render_sliver.hpp>
#include <campello_widgets/widgets/render_object_element.hpp>

namespace systems::leal::campello_widgets
{

    // =========================================================================
    // CustomScrollViewElement — reconciles the ordered sliver-widget list
    // against the widget's `slivers` vector, then syncs the resulting
    // RenderSlivers into the owned RenderViewport. Shape copied from
    // MultiChildRenderObjectElement (see multi_child_render_object_element.cpp)
    // but simplified to plain positional reconciliation (no key-partition
    // complexity -- ListViewElement doesn't do that either, and a
    // CustomScrollView's slivers aren't reordered), and syncing via
    // RenderSliver (not RenderBox) through RenderViewport's own
    // insertChild()/truncateChildren() rather than widget-level
    // insert/clear hooks (CustomScrollView, unlike MultiChildRenderObjectWidget,
    // has no such hooks -- it isn't RenderBox-typed).
    // =========================================================================

    class CustomScrollViewElement : public RenderObjectElement
    {
    public:
        explicit CustomScrollViewElement(std::shared_ptr<const CustomScrollView> widget)
            : RenderObjectElement(std::move(widget))
        {}

        void unmount() override
        {
            for (auto& child : child_elements_)
                if (child) child->unmount();
            child_elements_.clear();
            RenderObjectElement::unmount();
        }

        void onDescendantRenderObjectChanged() override
        {
            syncChildRenderObjects();
        }

        Element* firstChildElement() const noexcept override
        {
            return child_elements_.empty() ? nullptr : child_elements_.front().get();
        }

        void visitChildren(const std::function<void(Element*)>& visitor) const override
        {
            for (const auto& child : child_elements_)
                if (child) visitor(child.get());
        }

    protected:
        void performBuild() override
        {
            const auto& w = static_cast<const CustomScrollView&>(*widget_);

            std::vector<std::shared_ptr<Element>> new_elements;
            new_elements.reserve(w.slivers.size());
            for (size_t i = 0; i < w.slivers.size(); ++i)
            {
                auto existing = (i < child_elements_.size()) ? child_elements_[i] : nullptr;
                new_elements.push_back(updateChild(std::move(existing), w.slivers[i], this));
            }
            for (size_t i = w.slivers.size(); i < child_elements_.size(); ++i)
                if (child_elements_[i]) child_elements_[i]->unmount();

            child_elements_ = std::move(new_elements);
            syncChildRenderObjects();
        }

    private:
        void syncChildRenderObjects()
        {
            auto& rv = static_cast<RenderViewport&>(*render_object_);

            for (int i = 0; i < static_cast<int>(child_elements_.size()); ++i)
            {
                if (!child_elements_[i]) continue;

                auto* roe = child_elements_[i]->findDescendantRenderObjectElement();
                if (!roe) continue;

                auto sliver = std::dynamic_pointer_cast<RenderSliver>(roe->sharedRenderObject());
                if (sliver) rv.insertChild(std::move(sliver), i);
            }
            // No clearChildren() first -- insertChild() already reuses an
            // unchanged sliver in place at the same index (mirrors
            // RenderStack's own reuse optimization), so clearing first would
            // defeat that on every rebuild. Just drop any excess from a
            // previously-longer sliver list.
            rv.truncateChildren(child_elements_.size());
        }

        std::vector<std::shared_ptr<Element>> child_elements_;
    };

    // =========================================================================
    // CustomScrollView widget
    // =========================================================================

    std::shared_ptr<Element> CustomScrollView::createElement() const
    {
        return std::make_shared<CustomScrollViewElement>(
            std::static_pointer_cast<const CustomScrollView>(shared_from_this()));
    }

    std::shared_ptr<RenderObject> CustomScrollView::createRenderObject() const
    {
        auto r = std::make_shared<RenderViewport>();
        r->axis = axis;
        if (physics) r->physics = physics;
        r->setController(controller);
        return r;
    }

    void CustomScrollView::updateRenderObject(RenderObject& render_object) const
    {
        auto& rv = static_cast<RenderViewport&>(render_object);
        if (rv.axis != axis)
        {
            rv.axis = axis;
            rv.markNeedsLayout();
        }
        if (physics) rv.physics = physics;
        rv.setController(controller);
    }

} // namespace systems::leal::campello_widgets
