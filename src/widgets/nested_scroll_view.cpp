#include <vector>
#include <campello_widgets/widgets/nested_scroll_view.hpp>
#include <campello_widgets/widgets/render_object_element.hpp>
#include <campello_widgets/ui/render_nested_scroll_view.hpp>
#include <campello_widgets/ui/render_sliver_overlap_absorber.hpp>
#include <campello_widgets/ui/render_sliver_overlap_injector.hpp>
#include <campello_widgets/ui/sliver_overlap_absorber_handle.hpp>
#include <campello_widgets/ui/render_sliver.hpp>

namespace systems::leal::campello_widgets
{

    // =========================================================================
    // NestedScrollViewElement — a single-child reconciliation for `header`
    // (mirrors SingleChildRenderObjectElement, casting to RenderSliver instead
    // of RenderBox) plus a positional multi-child reconciliation for `body`
    // (a direct copy of CustomScrollViewElement's own shape). Owns the
    // absorber/injector RenderObjects directly -- no public
    // SliverOverlapAbsorber/SliverOverlapInjector *widgets* exist, since this
    // Element is the only thing that ever touches those classes.
    // =========================================================================

    class NestedScrollViewElement : public RenderObjectElement
    {
    public:
        explicit NestedScrollViewElement(std::shared_ptr<const NestedScrollView> widget)
            : RenderObjectElement(std::move(widget))
            , handle_(std::make_shared<SliverOverlapAbsorberHandle>())
        {}

        void unmount() override
        {
            if (header_element_) { header_element_->unmount(); header_element_.reset(); }
            for (auto& e : body_elements_) if (e) e->unmount();
            body_elements_.clear();
            RenderObjectElement::unmount();
        }

        void onDescendantRenderObjectChanged() override
        {
            auto& rnsv = static_cast<RenderNestedScrollView&>(*render_object_);
            syncHeader(rnsv);
            syncBody(rnsv);
        }

        Element* firstChildElement() const noexcept override
        {
            if (header_element_) return header_element_.get();
            return body_elements_.empty() ? nullptr : body_elements_.front().get();
        }

        void visitChildren(const std::function<void(Element*)>& visitor) const override
        {
            if (header_element_) visitor(header_element_.get());
            for (const auto& e : body_elements_)
                if (e) visitor(e.get());
        }

    protected:
        void performBuild() override
        {
            const auto& w    = static_cast<const NestedScrollView&>(*widget_);
            auto&       rnsv = static_cast<RenderNestedScrollView&>(*render_object_);

            header_element_ = updateChild(header_element_, w.header, this);
            syncHeader(rnsv);

            std::vector<std::shared_ptr<Element>> new_body;
            new_body.reserve(w.body.size());
            for (size_t i = 0; i < w.body.size(); ++i)
            {
                auto existing = (i < body_elements_.size()) ? body_elements_[i] : nullptr;
                new_body.push_back(updateChild(std::move(existing), w.body[i], this));
            }
            for (size_t i = w.body.size(); i < body_elements_.size(); ++i)
                if (body_elements_[i]) body_elements_[i]->unmount();

            body_elements_ = std::move(new_body);
            syncBody(rnsv);
        }

    private:
        void syncHeader(RenderNestedScrollView& rnsv)
        {
            if (!header_element_) return;

            auto* roe = header_element_->findDescendantRenderObjectElement();
            if (!roe) return;

            auto header_sliver = std::dynamic_pointer_cast<RenderSliver>(roe->sharedRenderObject());
            if (!header_sliver) return;

            if (!absorber_)
            {
                absorber_ = std::make_shared<RenderSliverOverlapAbsorber>();
                absorber_->handle = handle_;
                rnsv.outerViewport().insertChild(absorber_, 0);
            }
            absorber_->setChild(header_sliver);
        }

        void syncBody(RenderNestedScrollView& rnsv)
        {
            if (!injector_)
            {
                injector_ = std::make_shared<RenderSliverOverlapInjector>();
                injector_->handle = handle_;
                rnsv.innerViewport().insertChild(injector_, 0);
            }

            for (int i = 0; i < static_cast<int>(body_elements_.size()); ++i)
            {
                if (!body_elements_[i]) continue;

                auto* roe = body_elements_[i]->findDescendantRenderObjectElement();
                if (!roe) continue;

                auto sliver = std::dynamic_pointer_cast<RenderSliver>(roe->sharedRenderObject());
                if (sliver) rnsv.innerViewport().insertChild(std::move(sliver), i + 1);
            }
            // +1 to leave room for the injector permanently occupying index 0.
            rnsv.innerViewport().truncateChildren(body_elements_.size() + 1);
        }

        std::shared_ptr<SliverOverlapAbsorberHandle>  handle_;
        std::shared_ptr<RenderSliverOverlapAbsorber>  absorber_;
        std::shared_ptr<RenderSliverOverlapInjector>  injector_;
        std::shared_ptr<Element>                      header_element_;
        std::vector<std::shared_ptr<Element>>         body_elements_;
    };

    // =========================================================================
    // NestedScrollView widget
    // =========================================================================

    std::shared_ptr<Element> NestedScrollView::createElement() const
    {
        return std::make_shared<NestedScrollViewElement>(
            std::static_pointer_cast<const NestedScrollView>(shared_from_this()));
    }

    std::shared_ptr<RenderObject> NestedScrollView::createRenderObject() const
    {
        auto r = std::make_shared<RenderNestedScrollView>();
        r->header_extent = header ? header->min_extent : 0.0f;
        if (outer_physics) r->outerViewport().physics = outer_physics;
        if (inner_physics) r->innerViewport().physics = inner_physics;
        r->outerViewport().setController(outer_controller);
        r->innerViewport().setController(inner_controller);
        return r;
    }

    void NestedScrollView::updateRenderObject(RenderObject& render_object) const
    {
        auto& rnsv = static_cast<RenderNestedScrollView&>(render_object);
        const float new_header_extent = header ? header->min_extent : 0.0f;
        if (rnsv.header_extent != new_header_extent)
        {
            rnsv.header_extent = new_header_extent;
            rnsv.markNeedsLayout();
        }
        if (outer_physics) rnsv.outerViewport().physics = outer_physics;
        if (inner_physics) rnsv.innerViewport().physics = inner_physics;
        rnsv.outerViewport().setController(outer_controller);
        rnsv.innerViewport().setController(inner_controller);
    }

} // namespace systems::leal::campello_widgets
