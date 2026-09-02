#include <campello_widgets/ui/render_hero.hpp>
#include <campello_widgets/ui/paint_context.hpp>

namespace systems::leal::campello_widgets
{

    void RenderHero::performLayout()
    {
        if (child_)
        {
            layoutChild(*child_, constraints_);
            size_ = child_->size();
        }
        else
        {
            size_ = constraints_.constrain({0.0f, 0.0f});
        }
    }

    void RenderHero::performPaint(PaintContext& context, const Offset& offset)
    {
        global_rect_ = computeGlobalRect(context, offset);
        if (child_ && !hidden_) paintChild(context, offset);
    }

    void RenderHero::setHidden(bool hidden) noexcept
    {
        if (hidden_ == hidden) return;
        hidden_ = hidden;
        markNeedsPaint();
    }

} // namespace systems::leal::campello_widgets
