#version 450

// Icon pipeline — tinted template images. See icon.frag's doc comment.
// Same vertex layout/derivation as quad.vert; the push constant struct is
// larger (adds `tint`) and visible to both stages, so `tint` is forwarded
// through unchanged for icon.frag to read.

layout(push_constant) uniform IconUniforms {
    vec2  viewport;  // w, h (physical pixels)
    float opacity;
    float _pad;
    vec4  tint;      // straight-alpha RGBA recolor
} u;

layout(location = 0) in vec3 in_posw;  // (x, y, w) — projected pixel position + w
layout(location = 1) in vec2 in_uv;

layout(location = 0) out vec2  v_uv;
layout(location = 1) out float v_opacity;
layout(location = 2) out vec4  v_tint;

void main()
{
    // Same perspective-correct clip-space derivation as quad.vert.
    float clip_x = 2.0 * in_posw.x / u.viewport.x - in_posw.z;
    float clip_y = 2.0 * in_posw.y / u.viewport.y - in_posw.z;
    gl_Position = vec4(clip_x, clip_y, 0.0, in_posw.z);
    v_uv      = in_uv;
    v_opacity = u.opacity;
    v_tint    = u.tint;
}
