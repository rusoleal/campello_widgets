#pragma once

#include <memory>
#include <vector>
#include <campello_widgets/widgets/render_object_widget.hpp>
#include <campello_widgets/widgets/sliver_persistent_header.hpp>
#include <campello_widgets/ui/scroll_physics.hpp>

namespace systems::leal::campello_widgets
{

    class ScrollController;

    /**
     * @brief Widget-layer bridge for the NestedScrollView mechanism -- a
     * pinned/collapsing `header` above a scrolling `body`, sharing one drag
     * gesture stream via a NestedScrollCoordinator (see
     * RenderNestedScrollView, which this widget is backed by).
     *
     * `header` is concretely typed to SliverPersistentHeader (not a generic
     * WidgetRef) specifically so its min_extent is a known, plain value at
     * createRenderObject() time -- that's what lets the outer viewport's own
     * height be set automatically, with no separate configuration knob and
     * no way to pick a value that silently truncates the header's real
     * collapse range. `body` accepts any RenderSliver-producing widget(s),
     * same as CustomScrollView::slivers.
     *
     * Usage:
     * @code
     * auto nsv     = std::make_shared<NestedScrollView>();
     * nsv->header  = header;   // a SliverPersistentHeader
     * nsv->body    = { list }; // e.g. a SliverFixedExtentList
     * @endcode
     */
    class NestedScrollView : public RenderObjectWidget
    {
    public:
        std::shared_ptr<const SliverPersistentHeader> header;
        std::vector<WidgetRef>                        body;

        std::shared_ptr<ScrollPhysics>    outer_physics;
        std::shared_ptr<ScrollPhysics>    inner_physics;
        std::shared_ptr<ScrollController> outer_controller;
        std::shared_ptr<ScrollController> inner_controller;

        std::shared_ptr<Element>      createElement()      const override;
        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& render_object) const override;
    };

} // namespace systems::leal::campello_widgets
