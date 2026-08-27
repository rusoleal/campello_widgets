#include <campello_widgets/ui/render_constrained_box.hpp>

namespace systems::leal::campello_widgets
{

    void RenderConstrainedBox::performLayout()
    {
        // additional_constraints.enforce(constraints_), not a raw per-field
        // min()/max() intersection: enforce() clamps additional_constraints'
        // own bounds *into* constraints_'s [min, max] range, so the result
        // is always valid (min <= max). The naive min/max version breaks
        // when additional_constraints has an infinite bound on an axis
        // constraints_ is bounded on -- e.g. BoxConstraints::expand()
        // (min=max=infinity) intersected with a bounded max produces
        // {min: max(0, inf) = inf, max: min(300, inf) = 300}, an *invalid*
        // min > max constraints that then hits std::clamp's lo <= hi
        // precondition (UB) in the no-child branch below. This is exactly
        // the shape ConstrainedBox{BoxConstraints::expand()} has in
        // LimitedBox's own doc comment / Container's empty-box fallback.
        const BoxConstraints effective = additional_constraints.enforce(constraints_);

        if (child_)
        {
            layoutChild(*child_, effective);
            size_ = constraints_.constrain(child_->size());
            positionChild(*child_, {0.0f, 0.0f});
        }
        else
        {
            size_ = effective.constrain(Size::zero());
        }
    }

    void RenderConstrainedBox::performPaint(PaintContext& context, const Offset& offset)
    {
        paintChild(context, offset);
    }

} // namespace systems::leal::campello_widgets
