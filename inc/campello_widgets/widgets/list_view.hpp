#pragma once

#include <functional>
#include <memory>
#include <campello_widgets/widgets/render_object_widget.hpp>
#include <campello_widgets/ui/axis.hpp>
#include <campello_widgets/ui/scroll_physics.hpp>

namespace systems::leal::campello_widgets
{

    class BuildContext;
    class ScrollController;

    /// Callback that builds the widget for a given list index.
    using IndexedWidgetBuilder = std::function<WidgetRef(BuildContext&, int index)>;

    /**
     * @brief A scrollable, virtualised list of fixed-extent items.
     *
     * ListView only mounts elements for items currently visible in the viewport
     * (plus one item of buffer on each side). Items that scroll off screen are
     * unmounted and their resources released.
     *
     * `item_extent` must be set to a positive value (uniform item height for
     * vertical lists, or width for horizontal lists).
     *
     * Usage:
     * @code
     * auto lv          = std::make_shared<ListView>();
     * lv->builder      = [](BuildContext&, int i) { return makeRow(i); };
     * lv->item_count   = 1000;
     * lv->item_extent  = 48.0f;
     * @endcode
     */
    class ListView : public RenderObjectWidget
    {
    public:
        IndexedWidgetBuilder builder;
        int   item_count  = 0;
        float item_extent = 0.0f;

        Axis scroll_axis = Axis::vertical;

        std::shared_ptr<ScrollController> controller;
        std::shared_ptr<ScrollPhysics>    physics;

        std::shared_ptr<Element>      createElement()    const override;
        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& render_object) const override;
    };

} // namespace systems::leal::campello_widgets
