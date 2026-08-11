#version 450

layout(push_constant) uniform ShaderMaskUniforms {
    vec2  viewport;       // framebuffer width, height
    float gradient_type;  // 0 = linear, 1 = radial
    float _pad0;
    vec4  gradient_p1;    // linear: begin.xy; radial: center.xy (pixels)
    vec4  gradient_p2;    // linear: end.xy;   radial: radius in .x (pixels)
    float blend_mode;     // 0 = srcIn, 1 = modulate
    float _pad1[3];
} u;

layout(location = 0) in vec3 in_posw;  // (x, y, w) — projected pixel position + w
layout(location = 1) in vec2 in_uv;

layout(location = 0) out vec2  v_uv;
layout(location = 1) flat out float v_gradient_type;
layout(location = 2) flat out vec4  v_gradient_p1;
layout(location = 3) flat out vec4  v_gradient_p2;
layout(location = 4) flat out float v_blend_mode;

void main()
{
    float clip_x = 2.0 * in_posw.x / u.viewport.x - in_posw.z;
    float clip_y = 2.0 * in_posw.y / u.viewport.y - in_posw.z;
    gl_Position     = vec4(clip_x, clip_y, 0.0, in_posw.z);
    v_uv            = in_uv;
    v_gradient_type = u.gradient_type;
    v_gradient_p1   = u.gradient_p1;
    v_gradient_p2   = u.gradient_p2;
    v_blend_mode    = u.blend_mode;
}
