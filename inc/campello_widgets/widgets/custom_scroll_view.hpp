#pragma once

#include <memory>
#include <vector>
#include <campello_widgets/widgets/render_object_widget.hpp>
#include <campello_widgets/ui/axis.hpp>
#include <campello_widgets/ui/scroll_physics.hpp>

namespace systems::leal::campello_widgets
{

    class ScrollController;

    /**
     * @brief A scroll view whose content is a sequence of slivers sharing
     * one scroll position -- the widget-layer entry point for the
     * sliver-scrolling protocol.
     *
     * `slivers` may contain any RenderSliver-producing widget (e.g.
     * SliverToBoxAdapter, SliverFixedExtentList, SliverPersistentHeader).
     * Widget-layer bridge for RenderViewport; follows ListView's own
     * pattern of subclassing RenderObjectWidget directly rather than
     * MultiChildRenderObjectWidget, since that base's child-insertion hooks
     * are hard-typed to RenderBox and CustomScrollView has no RenderBox to
     * insert.
     *
     * Usage:
     * @code
     * auto csv     = std::make_shared<CustomScrollView>();
     * csv->slivers = { header, list };
     * @endcode
     */
    class CustomScrollView : public RenderObjectWidget
    {
    public:
        std::vector<WidgetRef> slivers;
        Axis axis = Axis::vertical;

        std::shared_ptr<ScrollController> controller;
        std::shared_ptr<ScrollPhysics>    physics;

        std::shared_ptr<Element>      createElement()      const override;
        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& render_object) const override;
    };

} // namespace systems::leal::campello_widgets
