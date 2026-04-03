#pragma once

#include <campello_widgets/widgets/single_child_render_object_widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Sizes its child to the child's own intrinsic width.
     *
     * The child is first measured unconstrained on the horizontal axis to
     * determine its natural width. The widget then re-lays out with that width
     * as a tight constraint, shrink-wrapping the child horizontally.
     *
     * `step_width` rounds the computed width up to the nearest multiple
     * (e.g. for grid snapping). Pass 0 to disable snapping.
     */
    class IntrinsicWidth : public SingleChildRenderObjectWidget
    {
    public:
        float step_width = 0.0f;

        IntrinsicWidth() = default;
        explicit IntrinsicWidth(WidgetRef c) { child = std::move(c); }
        explicit IntrinsicWidth(float step, WidgetRef c = nullptr)
            : step_width(step)
        {
            child = std::move(c);
        }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;
    };

} // namespace systems::leal::campello_widgets
