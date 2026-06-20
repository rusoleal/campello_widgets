#pragma once

#include <campello_widgets/widgets/single_child_render_object_widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Caches its child's paint output so an unrelated repaint
     * elsewhere in the tree doesn't force this subtree to be re-walked.
     *
     * Wrap a subtree that changes independently from its surroundings (e.g.
     * a live-rendered viewport inside a larger, mostly-static UI) so that
     * repainting the rest of the tree doesn't also repaint this one, and
     * vice versa. See `RenderRepaintBoundary` for the caching mechanism.
     *
     * Mirrors Flutter's `RepaintBoundary`. Unlike `ClipRect`, this does not
     * clip — it's purely a paint-caching boundary.
     */
    class RepaintBoundary : public SingleChildRenderObjectWidget
    {
    public:
        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;
    };

} // namespace systems::leal::campello_widgets
