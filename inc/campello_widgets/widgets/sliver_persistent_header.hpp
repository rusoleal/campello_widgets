#pragma once

#include <memory>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Widget-layer bridge for RenderSliverPersistentHeader (pinned
     * variant) -- a header that stays glued to the leading edge of a
     * CustomScrollView once scrolled past, optionally collapsing from
     * max_extent down to min_extent first.
     *
     * min_extent == max_extent degenerates to a fixed-height, always-full,
     * non-collapsing "sticky" header. See RenderSliverPersistentHeader's own
     * class doc for the pinning mechanism itself (lives in RenderViewport).
     */
    class SliverPersistentHeader : public SingleChildRenderObjectWidget
    {
    public:
        /** Height once fully collapsed/pinned. Must be <= max_extent. */
        float min_extent = 0.0f;
        /** Height when fully expanded (scroll_offset == 0). */
        float max_extent = 0.0f;

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& render_object) const override;

        void insertRenderObjectChild(
            RenderObject&              parent,
            std::shared_ptr<RenderBox> child_box) const override;

        void removeRenderObjectChild(RenderObject& parent) const override;
    };

} // namespace systems::leal::campello_widgets
