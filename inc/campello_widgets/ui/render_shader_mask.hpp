#pragma once

#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/shader.hpp>
#include <campello_widgets/ui/paint.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief RenderObject that composites its child with a gradient mask.
     *
     * @ref ShaderMask is the Widget counterpart.
     *
     * The child is rendered to an offscreen texture by the Renderer, the
     * gradient is evaluated as a mask (via a 256-entry LUT), and the two are
     * composited using `blend_mode`.
     *
     * **Note:** The mask evaluates the gradient in *viewport* coordinates, not
     * local coordinates.  Shift `shader.begin` / `shader.end` (or center/radius
     * for radial) by the widget's offset if local-space behaviour is needed.
     */
    class RenderShaderMask final : public RenderBox
    {
    public:
        explicit RenderShaderMask(
            Shader    shader,
            BlendMode blend_mode = BlendMode::srcIn)
            : shader_(std::move(shader))
            , blend_mode_(blend_mode)
        {}

        void setShader(const Shader& shader) noexcept
        {
            shader_ = shader;
            markNeedsPaint();
        }

        void setBlendMode(BlendMode mode) noexcept
        {
            if (blend_mode_ != mode)
            {
                blend_mode_ = mode;
                markNeedsPaint();
            }
        }

        const Shader&  shader()    const noexcept { return shader_;     }
        BlendMode      blendMode() const noexcept { return blend_mode_; }

        // ------------------------------------------------------------------
        // RenderBox overrides
        // ------------------------------------------------------------------

        void performLayout() override;
        void performPaint(PaintContext& ctx, const Offset& offset) override;
        bool hitTestChildren(HitTestResult& result, const Offset& position) override;

    private:
        Shader    shader_;
        BlendMode blend_mode_;
    };

} // namespace systems::leal::campello_widgets
