#version 450

// Draws a "template" texture recolored to an arbitrary tint: the source
// texture's own RGB is ignored entirely, and only its alpha channel is
// sampled, used as a stencil for `tint` — the same mechanism as iOS's
// UIImage.withRenderingMode(.alwaysTemplate) and Android's icon tinting.
// Lets one monochrome icon asset serve any theme color. Mirrors
// Metal's iconFragment (shaders/metal/widgets.metal) field-for-field.

layout(location = 0) in vec2  v_uv;
layout(location = 1) in float v_opacity;
layout(location = 2) in vec4  v_tint;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 1) uniform texture2D u_texture;
layout(set = 0, binding = 2) uniform sampler   u_sampler;

void main()
{
    // Output premultiplied, matching every other pipeline's blend state
    // (src=ONE dst=ONE_MINUS_SRC_ALPHA).
    float a = texture(sampler2D(u_texture, u_sampler), v_uv).a * v_tint.a * v_opacity;
    out_color = vec4(v_tint.rgb * a, a);
}
