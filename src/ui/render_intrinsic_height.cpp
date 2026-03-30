#include <cmath>
#include <campello_widgets/ui/render_intrinsic_height.hpp>

namespace systems::leal::campello_widgets
{

    void RenderIntrinsicHeight::performLayout()
    {
        if (!child_)
        {
            size_ = constraints_.constrain(Size::zero());
            return;
        }

        // First pass: measure child's natural height with no height constraint.
        const BoxConstraints measure{
            constraints_.min_width,
            constraints_.max_width,
            0.0f,
            std::numeric_limits<float>::infinity(),
        };
        layoutChild(*child_, measure);
        float h = child_->size().height;

        // Snap to step_height if requested.
        if (step_height > 0.0f)
            h = std::ceil(h / step_height) * step_height;

        // Clamp to incoming constraints.
        h = std::clamp(h, constraints_.min_height, constraints_.max_height);

        // Second pass: tight-constrain to computed height.
        layoutChild(*child_, BoxConstraints{constraints_.min_width, constraints_.max_width, h, h});
        size_ = {child_->size().width, h};
        positionChild(*child_, {0.0f, 0.0f});
    }

    void RenderIntrinsicHeight::performPaint(PaintContext& context, const Offset& offset)
    {
        paintChild(context, offset);
    }

} // namespace systems::leal::campello_widgets
