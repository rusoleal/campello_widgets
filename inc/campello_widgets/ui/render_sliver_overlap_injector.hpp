#pragma once

#include <memory>
#include <campello_widgets/ui/render_sliver.hpp>
#include <campello_widgets/ui/sliver_overlap_absorber_handle.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A pure spacer sliver -- no child at all -- that reserves space
     * matching a shared SliverOverlapAbsorberHandle's layout_extent. Sits as
     * the first sliver of the *inner* scrollable in a NestedScrollView, so
     * inner content doesn't render starting underneath the outer header.
     * Stage 2 of the NestedScrollView initiative.
     *
     * Geometry formula is exactly RenderSliverToBoxAdapter's own clamp
     * (render_sliver_to_box_adapter.cpp), with the handle's stored extent
     * standing in for a child's natural size.
     */
    class RenderSliverOverlapInjector : public RenderSliver
    {
    public:
        std::shared_ptr<SliverOverlapAbsorberHandle> handle;

    protected:
        void performLayoutSliver() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
    };

} // namespace systems::leal::campello_widgets
