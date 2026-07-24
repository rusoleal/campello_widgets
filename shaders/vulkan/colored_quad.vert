#version 450

layout(push_constant) uniform ColoredQuadUniforms {
    vec4 rect;      // unused — occupies offset 0 so layout matches RectUniforms
    vec4 color;     // r, g, b, a (straight alpha — frag premultiplies)
    vec2 viewport;  // physical pixel size
    vec2 _pad;
} u;

layout(location = 0) in vec3 in_posw;  // (x, y, w) — projected pixel coords + perspective w

layout(location = 0) out vec4 v_color;

void main()
{
    float clip_x = 2.0 * in_posw.x / u.viewport.x - in_posw.z;
    float clip_y = 2.0 * in_posw.y / u.viewport.y - in_posw.z;
    gl_Position  = vec4(clip_x, clip_y, 0.0, in_posw.z);
    v_color      = u.color;
}
