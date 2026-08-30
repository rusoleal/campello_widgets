#include <campello_widgets/ui/render_constraints_transform_box.hpp>

namespace systems::leal::campello_widgets
{

    void RenderConstraintsTransformBox::performLayout()
    {
        // Own size comes only from the constraints given to *this* box --
        // deliberately independent of the child's actual (possibly larger,
        // overflowing) size, unlike RenderConstrainedBox.
        size_ = constraints_.constrain({constraints_.max_width, constraints_.max_height});

        if (child_)
        {
            const BoxConstraints child_constraints = constraints_transform
                ? constraints_transform(constraints_)
                : constraints_;
            layoutChild(*child_, child_constraints);
            positionChild(*child_, alignment.inscribe(child_->size(), size_));
        }
    }

    void RenderConstraintsTransformBox::performPaint(PaintContext& context, const Offset& offset)
    {
        paintChild(context, offset);
    }

} // namespace systems::leal::campello_widgets
