#include <campello_widgets/ui/render_limited_box.hpp>
#include <algorithm>
#include <cmath>
#include <campello_widgets/ui/paint_context.hpp>

namespace systems::leal::campello_widgets
{

    BoxConstraints RenderLimitedBox::limitConstraints(const BoxConstraints& constraints) const noexcept
    {
        const float inf = std::numeric_limits<float>::infinity();
        return BoxConstraints{
            constraints.min_width,
            std::isinf(constraints.max_width)
                ? std::clamp(max_width, constraints.min_width, inf)
                : constraints.max_width,
            constraints.min_height,
            std::isinf(constraints.max_height)
                ? std::clamp(max_height, constraints.min_height, inf)
                : constraints.max_height,
        };
    }

    void RenderLimitedBox::performLayout()
    {
        if (child_)
        {
            const Size child_size = layoutChild(*child_, limitConstraints(constraints_));
            size_ = constraints_.constrain(child_size);
            positionChild(*child_, {0.0f, 0.0f});
        }
        else
        {
            size_ = limitConstraints(constraints_).constrain(Size::zero());
        }
    }

    void RenderLimitedBox::performPaint(PaintContext& context, const Offset& offset)
    {
        paintChild(context, offset);
    }

} // namespace systems::leal::campello_widgets
