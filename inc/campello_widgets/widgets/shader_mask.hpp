#pragma once

#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/shader.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <campello_widgets/ui/render_shader_mask.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Applies a gradient mask to its child widget.
     *
     * The child is composited with the evaluated gradient via `blend_mode`.
     * Common blend modes:
     *  - `BlendMode::srcIn`   — child is visible only where the gradient is opaque.
     *  - `BlendMode::modulate` — child is multiplied by the gradient color.
     *
     * @code
     * ShaderMask{
     *     .shader     = LinearGradient{
     *                       .begin  = {0, 0},
     *                       .end    = {0, 200},
     *                       .colors = {Color::white(), Color::transparent()},
     *                   },
     *     .blend_mode = BlendMode::srcIn,
     *     .child      = SomeWidget{},
     * }
     * @endcode
     */
    struct ShaderMask : SingleChildRenderObjectWidget
    {
        Shader    shader = LinearGradient{
            {0.0f, 0.0f}, {100.0f, 0.0f},
            {Color::white(), Color::black()}, {}
        };
        BlendMode blend_mode = BlendMode::srcIn;


        std::shared_ptr<RenderObject> createRenderObject() const override
        {
            return std::make_shared<RenderShaderMask>(shader, blend_mode);
        }

        void updateRenderObject(RenderObject& ro) const override
        {
            auto& mask = static_cast<RenderShaderMask&>(ro);
            mask.setShader(shader);
            mask.setBlendMode(blend_mode);
        }
    };

} // namespace systems::leal::campello_widgets
