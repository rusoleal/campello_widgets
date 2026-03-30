#include <campello_widgets/ui/render_shader_mask.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/hit_test.hpp>

namespace systems::leal::campello_widgets
{

    void RenderShaderMask::performLayout()
    {
        if (child_)
        {
            child_->layout(constraints());
            size_ = child_->size();
        }
        else
        {
            const auto& c = constraints();
            size_ = Size{c.max_width, c.max_height};
        }
    }

    void RenderShaderMask::performPaint(PaintContext& ctx, const Offset& offset)
    {
        const Rect bounds = Rect::fromLTWH(
            offset.x, offset.y, size_.width, size_.height);

        // Open the ShaderMask scope in the draw list.
        ctx.canvas().beginShaderMask(bounds, shader_, blend_mode_);

        if (child_)
            paintChild(ctx, offset);

        ctx.canvas().endShaderMask();
    }

    bool RenderShaderMask::hitTestChildren(
        HitTestResult& result, const Offset& position)
    {
        if (child_)
            return child_->hitTest(result, position);
        return false;
    }

} // namespace systems::leal::campello_widgets
