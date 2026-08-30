#pragma once

#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/text_baseline.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Positions its child so that a specific baseline within it sits
     * `baseline` logical pixels down from this box's own top edge.
     *
     * Lays out the child unconstrained on the vertical axis (like
     * `RenderAlign`), queries the child's own
     * `computeDistanceToActualBaseline()`, and offsets the child by
     * `baseline - child_baseline` -- Flutter's exact `RenderBaseline`
     * formula. Falls back to top-alignment (offset 0) if the child reports
     * no baseline of its own.
     *
     * Matches Flutter's `Baseline` widget.
     */
    class RenderBaseline : public RenderBox
    {
    public:
        float        baseline      = 0.0f;
        TextBaseline baseline_type = TextBaseline::alphabetic;

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
    };

} // namespace systems::leal::campello_widgets
