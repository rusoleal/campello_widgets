#include <algorithm>
#include <campello_widgets/ui/render_constrained_box.hpp>

namespace systems::leal::campello_widgets
{

    void RenderConstrainedBox::performLayout()
    {
        // Intersect parent constraints with additional_constraints.
        const BoxConstraints effective{
            std::max(constraints_.min_width,  additional_constraints.min_width),
            std::min(constraints_.max_width,  additional_constraints.max_width),
            std::max(constraints_.min_height, additional_constraints.min_height),
            std::min(constraints_.max_height, additional_constraints.max_height),
        };

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
