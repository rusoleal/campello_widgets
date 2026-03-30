#include <cmath>
#include <campello_widgets/ui/render_intrinsic_width.hpp>

namespace systems::leal::campello_widgets
{

    void RenderIntrinsicWidth::performLayout()
    {
        if (!child_)
        {
            size_ = constraints_.constrain(Size::zero());
            return;
        }

        // First pass: measure child's natural width with no width constraint.
        const BoxConstraints measure{
            0.0f,
            std::numeric_limits<float>::infinity(),
            constraints_.min_height,
            constraints_.max_height,
        };
        layoutChild(*child_, measure);
        float w = child_->size().width;

        // Snap to step_width if requested.
        if (step_width > 0.0f)
            w = std::ceil(w / step_width) * step_width;

        // Clamp to incoming constraints.
        w = std::clamp(w, constraints_.min_width, constraints_.max_width);

        // Second pass: tight-constrain to computed width.
        layoutChild(*child_, BoxConstraints{w, w, constraints_.min_height, constraints_.max_height});
        size_ = {w, child_->size().height};
        positionChild(*child_, {0.0f, 0.0f});
    }

    void RenderIntrinsicWidth::performPaint(PaintContext& context, const Offset& offset)
    {
        paintChild(context, offset);
    }

} // namespace systems::leal::campello_widgets
