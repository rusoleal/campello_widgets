#pragma once

#include <campello_widgets/ui/render_box.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Sizes the box to the child's intrinsic height.
     *
     * Lays out the child unconstrained on the vertical axis, measures the
     * resulting height, then re-lays out with that height as a tight constraint.
     * `step_height` snaps the computed height to the nearest multiple (0 = disabled).
     */
    class RenderIntrinsicHeight : public RenderBox
    {
    public:
        float step_height = 0.0f;

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
    };

} // namespace systems::leal::campello_widgets
