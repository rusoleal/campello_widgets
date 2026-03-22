#pragma once

#include <memory>
#include <campello_widgets/widgets/widget.hpp>

namespace systems::leal::campello_widgets
{

    class RenderObject;

    /**
     * @brief A widget that is backed directly by a RenderObject.
     *
     * RenderObjectWidget is the bridge between the Widget/Element trees and the
     * RenderObject tree. Subclasses implement:
     *  - `createRenderObject()` — called once when the element is first mounted.
     *  - `updateRenderObject()` — called on subsequent rebuilds to sync the
     *    render object's properties with the new widget configuration.
     *
     * Prefer subclassing `StatelessWidget` or `StatefulWidget` for most use
     * cases. Use RenderObjectWidget only when you need direct control over
     * layout and painting.
     */
    class RenderObjectWidget : public Widget
    {
    public:
        /**
         * @brief Creates the RenderObject for this widget.
         *
         * Called once when the element is mounted. The returned object is owned
         * by the element and reused for the lifetime of the element.
         */
        virtual std::shared_ptr<RenderObject> createRenderObject() const = 0;

        /**
         * @brief Syncs the render object's properties from a new widget instance.
         *
         * Called after a rebuild when the widget type is unchanged. Update any
         * RenderObject properties (colours, text, sizes) that may have changed.
         * The default implementation does nothing.
         *
         * @param render_object The render object to update.
         */
        virtual void updateRenderObject(RenderObject& /*render_object*/) const {}

        std::shared_ptr<Element> createElement() const override;
    };

} // namespace systems::leal::campello_widgets
