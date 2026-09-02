#pragma once

#include <functional>
#include <memory>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/render_viewport.hpp>
#include <campello_widgets/ui/nested_scroll_coordinator.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Container RenderObject gluing two RenderViewports (outer above,
     * inner filling the remainder) together via a NestedScrollCoordinator --
     * the RenderObject the NestedScrollView widget is backed by.
     *
     * Owns outer_/inner_ directly (not through RenderBox's single child_,
     * which only ever supports one) and wires the coordinator once, in the
     * constructor -- see that constructor's own comment for why no
     * attach()/detach() override is needed here (confirmed against
     * RenderSliverToBoxAdapter, which needs none, vs. RenderViewport, which
     * does -- the difference is whether the class registers its own pointer
     * handler with PointerDispatcher, which this class never does; all
     * pointer handling is delegated to whichever of outer_/inner_ is hit).
     *
     * Only vertical layout is implemented -- matches every prior stage's own
     * vertical-first scope.
     */
    class RenderNestedScrollView : public RenderBox
    {
    public:
        /**
         * @brief The outer viewport's own height -- set by the NestedScrollView
         * widget from its header's min_extent, so the outer viewport's own
         * scrollable range lands exactly on the header's real collapse range
         * (see the widget's own doc for the worked derivation).
         */
        float header_extent = 0.0f;

        RenderNestedScrollView();

        RenderViewport& outerViewport() noexcept { return *outer_; }
        RenderViewport& innerViewport() noexcept { return *inner_; }

    protected:
        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
        bool hitTestChildren(HitTestResult& result, const Offset& position) override;
        void visitRenderChildren(const std::function<void(RenderBox*)>& visitor) const override;

    private:
        std::shared_ptr<RenderViewport>          outer_;
        std::shared_ptr<RenderViewport>          inner_;
        std::shared_ptr<NestedScrollCoordinator> coordinator_;
    };

} // namespace systems::leal::campello_widgets
