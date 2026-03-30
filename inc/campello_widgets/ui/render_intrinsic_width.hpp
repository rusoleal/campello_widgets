#pragma once

#include <campello_widgets/ui/render_box.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Sizes the box to the child's intrinsic width.
     *
     * Lays out the child unconstrained on the horizontal axis, measures the
     * resulting width, then re-lays out with that width as a tight constraint.
     * `step_width` snaps the computed width to the nearest multiple (0 = disabled).
     */
    class RenderIntrinsicWidth : public RenderBox
    {
    public:
        float step_width  = 0.0f;

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
    };

} // namespace systems::leal::campello_widgets
