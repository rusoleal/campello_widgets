#pragma once

#include <memory>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/render_sliver.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Bridges one ordinary RenderBox into the sliver protocol --
     * Stage 3 of the sliver-scrolling initiative, "the bridge" per the
     * Sliver Protocol Scoping artifact's own framing.
     *
     * Lays the child out unconstrained on the main axis (tight on cross --
     * SliverConstraints only ever hands a sliver a single cross_axis_extent
     * scalar, not a range, unlike RenderSingleChildScrollView's own
     * cross-axis formula), then derives every SliverGeometry field from the
     * child's own reported size and the current SliverConstraints window.
     * No index math -- scroll_extent/paint_extent/max_paint_extent all come
     * straight from the child's natural size.
     *
     * Hit-testing remains out of scope at this stage (see RenderSliver's own
     * class doc) -- a RenderSliverToBoxAdapter's content is not tappable
     * yet. No widget-layer bridge exists yet either; constructing one still
     * requires hand-wiring RenderObjects directly, the same way Stages 1-2
     * were verified.
     */
    class RenderSliverToBoxAdapter : public RenderSliver
    {
    public:
        void setChild(std::shared_ptr<RenderBox> child);
        RenderBox* child() const noexcept { return child_.get(); }

    protected:
        void performLayoutSliver() override;
        void performPaint(PaintContext& context, const Offset& offset) override;

    private:
        std::shared_ptr<RenderBox> child_;
    };

} // namespace systems::leal::campello_widgets
