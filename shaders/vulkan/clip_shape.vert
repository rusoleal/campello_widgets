#version 450

layout(std140, set = 0, binding = 0) uniform ClipShapeUniforms {
    vec2  rect_size;  // logical w, h — used for SDF evaluation
    vec2  viewport;   // physical w, h
    float corner_r;   // logical corner radius (ignored when kind == 1)
    float kind;       // 0 = rounded rect, 1 = ellipse/oval
    vec2  _pad;
} u;

layout(location = 0) in vec3 in_posw;  // (x, y, w) — projected pixel position + w
layout(location = 1) in vec2 in_uv;

layout(location = 0)      out vec2  v_uv;
layout(location = 1) flat out vec2  v_rect_size;
layout(location = 2) flat out float v_corner_r;
layout(location = 3) flat out float v_kind;

void main()
{
    float clip_x = 2.0 * in_posw.x / u.viewport.x - in_posw.z;
    float clip_y = 2.0 * in_posw.y / u.viewport.y - in_posw.z;
    gl_Position = vec4(clip_x, clip_y, 0.0, in_posw.z);
    v_uv        = in_uv;
    v_rect_size = u.rect_size;
    v_corner_r  = u.corner_r;
    v_kind      = u.kind;
}
