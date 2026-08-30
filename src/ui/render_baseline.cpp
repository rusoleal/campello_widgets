#include <campello_widgets/ui/render_baseline.hpp>

namespace systems::leal::campello_widgets
{

    void RenderBaseline::performLayout()
    {
        if (child_)
        {
            const Size child_size = layoutChild(*child_, constraints_.loosen());
            const auto measured = child_->computeDistanceToActualBaseline(baseline_type);
            // Falls back to the child's own height when it reports no
            // baseline of its own -- matches Flutter's RenderBaseline exactly.
            const float child_baseline_dist = measured.value_or(child_size.height);
            const float top = baseline - child_baseline_dist;

            positionChild(*child_, {0.0f, top});
            size_ = constraints_.constrain({child_size.width, top + child_size.height});
        }
        else
        {
            size_ = constraints_.constrain(Size::zero());
        }
    }

    void RenderBaseline::performPaint(PaintContext& context, const Offset& offset)
    {
        paintChild(context, offset);
    }

} // namespace systems::leal::campello_widgets
