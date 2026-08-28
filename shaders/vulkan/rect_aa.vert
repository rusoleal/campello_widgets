#version 450

// Identical to colored_quad.vert, plus a per-vertex alpha passed straight
// through as an interpolant -- see rect_aa.frag and
// src/gpu/path_fill_aa.hpp's doc comment. Shares RectUniforms/rect_layout_
// with the plain rect/colored-quad pipelines (same push-constant layout).

layout(push_constant) uniform RectAAUniforms {
    vec4 rect;      // unused — occupies offset 0 so layout matches RectUniforms
    vec4 color;     // r, g, b, a (straight alpha — frag premultiplies)
    vec2 viewport;  // physical pixel size
    vec2 _pad;
} u;

layout(location = 0) in vec4 in_posw_a;  // (x, y, w, alpha)

layout(location = 0) out vec4  v_color;
layout(location = 1) out float v_alpha;

void main()
{
    float clip_x = 2.0 * in_posw_a.x / u.viewport.x - in_posw_a.z;
    float clip_y = 2.0 * in_posw_a.y / u.viewport.y - in_posw_a.z;
    gl_Position  = vec4(clip_x, clip_y, 0.0, in_posw_a.z);
    v_color      = u.color;
    v_alpha      = in_posw_a.w;
}
