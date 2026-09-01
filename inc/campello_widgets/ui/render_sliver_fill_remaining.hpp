#pragma once

#include <memory>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/render_sliver.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Claims whatever paint budget is left in the viewport for one
     * child -- Stage 1 of the NestedScrollView initiative, "the body
     * region" per the NestedScrollView Scoping artifact's own framing.
     *
     * Unlike RenderSliverToBoxAdapter (which lays its child out
     * unconstrained on the main axis and reports the child's own natural
     * size), this class lays its child out *tight* to
     * sliver_constraints_.remaining_paint_extent on the main axis -- the
     * whole point is "fill exactly this much," not "report your own
     * natural size". Its geometry (scroll_extent/paint_extent/
     * layout_extent/max_paint_extent) is set from that same leftover
     * extent regardless of whether a child is attached, matching Flutter's
     * real RenderSliverFillRemaining semantics (the fill region still
     * reserves its space even with no child yet) -- a deliberate
     * divergence from RenderSliverToBoxAdapter's own no-child behavior
     * (which reports all-zero geometry, since there's nothing to show and
     * no reason to reserve space).
     *
     * Flutter's real formula also subtracts min(overlap, 0.0) from the
     * claimed extent, to give a *floating* header's temporary negative
     * overlap extra fill room. Omitted here: overlap is never negative in
     * this codebase's model (it only ever accumulates a non-negative
     * pin_floor from preceding pinned headers), and floating headers are
     * an explicitly deferred later stage this codebase has never
     * implemented -- so that term is always exactly 0 here.
     *
     * Hit-testing remains out of scope at this stage, same as every other
     * RenderSliver subclass so far. No widget-layer bridge exists yet
     * either; constructing one still requires hand-wiring RenderObjects
     * directly, the same way the sliver-scrolling stages were verified.
     */
    class RenderSliverFillRemaining : public RenderSliver
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
