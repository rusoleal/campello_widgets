#pragma once

#include <functional>
#include <memory>
#include <campello_widgets/widgets/render_object_widget.hpp>
#include <campello_widgets/widgets/list_view.hpp>
#include <campello_widgets/ui/scroll_physics.hpp>

namespace systems::leal::campello_widgets
{

    class BuildContext;
    class ScrollController;

    /**
     * @brief A scrollable, virtualised 2D grid of fixed-extent items.
     *
     * Items are laid out in a row-major grid that scrolls vertically.
     * Only items in the visible rows are mounted; items that scroll off screen
     * are unmounted.
     *
     * Usage:
     * @code
     * auto gv              = std::make_shared<GridView>();
     * gv->builder          = [](BuildContext&, int i) { return makeCell(i); };
     * gv->item_count       = 200;
     * gv->item_extent      = 120.0f; // row height
     * gv->cross_axis_count = 3;      // three columns
     * @endcode
     */
    class GridView : public RenderObjectWidget
    {
    public:
        IndexedWidgetBuilder builder;
        int   item_count       = 0;
        float item_extent      = 0.0f;
        int   cross_axis_count = 2;

        std::shared_ptr<ScrollController> controller;
        std::shared_ptr<ScrollPhysics>    physics;

        std::shared_ptr<Element>      createElement()    const override;
        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& render_object) const override;
    };

} // namespace systems::leal::campello_widgets
