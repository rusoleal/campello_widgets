#pragma once

#include <memory>
#include <campello_widgets/widgets/render_object_widget.hpp>
#include <campello_widgets/widgets/list_view.hpp>
#include <campello_widgets/diagnostics/debug_assert.hpp>

namespace systems::leal::campello_widgets
{

    class BuildContext;

    /**
     * @brief Sliver-protocol analog of ListView -- a virtualised,
     * fixed-extent list of items usable as one sliver inside a
     * CustomScrollView.
     *
     * Widget-layer bridge for RenderSliverFixedExtentList. Reuses
     * IndexedWidgetBuilder (declared by list_view.hpp) for its `builder`
     * field. No scroll_axis/controller/physics fields -- those live on the
     * ancestor CustomScrollView/RenderViewport, not on an individual sliver.
     *
     * Usage:
     * @code
     * auto sl          = std::make_shared<SliverFixedExtentList>();
     * sl->builder      = [](BuildContext&, int i) { return makeRow(i); };
     * sl->item_count   = 1000;
     * sl->item_extent  = 48.0f;
     * @endcode
     */
    class SliverFixedExtentList : public RenderObjectWidget
    {
    public:
        IndexedWidgetBuilder builder;
        int   item_count  = 0;
        float item_extent = 0.0f;

        std::shared_ptr<Element>      createElement()      const override;
        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& render_object) const override;

        void debugValidate() const override
        {
            CW_ASSERT_MSG(item_count >= 0, "SliverFixedExtentList.item_count must be non-negative");
            CW_ASSERT_MSG(item_extent > 0.0f, "SliverFixedExtentList.item_extent must be greater than zero");
            if (item_count > 0)
                CW_ASSERT_MSG(builder, "SliverFixedExtentList.builder must be set when item_count > 0");
        }
    };

} // namespace systems::leal::campello_widgets
