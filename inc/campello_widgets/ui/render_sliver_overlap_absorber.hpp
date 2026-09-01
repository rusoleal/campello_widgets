#pragma once

#include <memory>
#include <campello_widgets/ui/render_sliver.hpp>
#include <campello_widgets/ui/sliver_overlap_absorber_handle.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Wraps one sliver child (the outer viewport's header content,
     * e.g. a RenderSliverPersistentHeader) and records how much it
     * permanently obstructs into a shared SliverOverlapAbsorberHandle --
     * Stage 2 of the NestedScrollView initiative.
     *
     * Unlike every prior sliver-wrapping class (RenderSliverToBoxAdapter,
     * RenderSliverPersistentHeader), this one's child is a RenderSliver, not
     * a RenderBox -- it wraps another sliver, not ordinary box content.
     *
     * A pure pass-through: forwards the child's own geometry unchanged (it
     * is not itself an obstruction, and does not alter the outer viewport's
     * layout at all) -- the handle write is a side channel riding along on
     * an otherwise transparent sliver.
     */
    class RenderSliverOverlapAbsorber : public RenderSliver
    {
    public:
        std::shared_ptr<SliverOverlapAbsorberHandle> handle;

        void setChild(std::shared_ptr<RenderSliver> child);
        RenderSliver* child() const noexcept { return child_.get(); }

    protected:
        void performLayoutSliver() override;
        void performPaint(PaintContext& context, const Offset& offset) override;

    private:
        std::shared_ptr<RenderSliver> child_;
    };

} // namespace systems::leal::campello_widgets
