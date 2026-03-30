#pragma once

#include <campello_widgets/widgets/single_child_render_object_widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Sizes its child to the child's own intrinsic height.
     *
     * The child is first measured unconstrained on the vertical axis to
     * determine its natural height. The widget then re-lays out with that height
     * as a tight constraint, shrink-wrapping the child vertically.
     *
     * `step_height` rounds the computed height up to the nearest multiple
     * (e.g. for grid snapping). Pass 0 to disable snapping.
     */
    class IntrinsicHeight : public SingleChildRenderObjectWidget
    {
    public:
        float step_height = 0.0f;

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;
    };

} // namespace systems::leal::campello_widgets
