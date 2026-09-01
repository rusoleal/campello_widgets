#pragma once

#include <memory>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/render_sliver.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Sliver-protocol pinned/collapsing header -- Stage 5 (final
     * stage) of the sliver-scrolling initiative, the SliverAppBar-style
     * "headline use case" per the Sliver Protocol Scoping artifact.
     *
     * Collapses from max_extent down to min_extent as its own local
     * scroll_offset grows, then holds at min_extent forever after --
     * min_extent == max_extent degenerates to a fixed-height, always-full,
     * non-collapsing "sticky" header (a real, common
     * SliverPersistentHeader(pinned: true) configuration). This class does
     * NOT clamp its own position -- the entire pin mechanism (keeping the
     * header glued to the viewport edge once its natural, scroll-delta-based
     * position would otherwise carry it off-screen, plus clipping later
     * content that would otherwise paint through its reserved space) lives
     * in RenderViewport, gated purely by this class reporting a non-zero
     * geometry().max_scroll_obstruction_extent. See RenderViewport::
     * performLayout()'s pin_floor bookkeeping and performPaint()'s two-pass
     * clip for the mechanism itself.
     *
     * No widget-layer bridge exists yet, consistent with every prior stage
     * -- constructing one still requires hand-wiring RenderObjects directly,
     * the same way Stages 1-4 were verified.
     */
    class RenderSliverPersistentHeader : public RenderSliver
    {
    public:
        /** Height once fully collapsed/pinned. Must be <= max_extent. */
        float min_extent = 0.0f;
        /** Height when fully expanded (scroll_offset == 0). */
        float max_extent = 0.0f;

        void setChild(std::shared_ptr<RenderBox> child);
        RenderBox* child() const noexcept { return child_.get(); }

    protected:
        void performLayoutSliver() override;
        void performPaint(PaintContext& context, const Offset& offset) override;

    private:
        std::shared_ptr<RenderBox> child_;
    };

} // namespace systems::leal::campello_widgets
