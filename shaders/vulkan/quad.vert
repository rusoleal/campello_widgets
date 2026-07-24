#version 450

layout(push_constant) uniform QuadUniforms {
    vec2  viewport;  // w, h (physical pixels)
    float opacity;
    float _pad;
} u;

layout(location = 0) in vec3 in_posw;  // (x, y, w) — projected pixel position + w
layout(location = 1) in vec2 in_uv;

layout(location = 0) out vec2  v_uv;
layout(location = 1) out float v_opacity;

void main()
{
    // Perspective-correct clip space. Setting clip.w = in_posw.z lets the
    // GPU's hardware perspective divide reproduce the pixel→NDC mapping when
    // w=1 and genuinely foreshorten when w≠1 (perspective transform).
    // Vulkan NDC: y=-1 is TOP, y=+1 is BOTTOM (positive-height viewport) —
    // no y-flip needed.
    float clip_x = 2.0 * in_posw.x / u.viewport.x - in_posw.z;
    float clip_y = 2.0 * in_posw.y / u.viewport.y - in_posw.z;
    gl_Position = vec4(clip_x, clip_y, 0.0, in_posw.z);
    v_uv      = in_uv;
    v_opacity = u.opacity;
}
