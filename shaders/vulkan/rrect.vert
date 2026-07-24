#version 450

layout(push_constant) uniform RRectUniforms {
    vec4  rect;      // x, y, w, h (screen pixels)
    vec4  color;     // r, g, b, a (premultiplied in frag)
    vec2  viewport;  // screen width, height
    float radius;    // corner radius in pixels
    float stroke_w;  // 0 = fill, >0 = stroke width in pixels
} u;

layout(location = 0) out vec2 v_pos;  // pixel-space position for SDF in frag

const vec2 kQuadCorners[6] = vec2[](
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
    vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(1.0, 1.0)
);

void main()
{
    vec2 t  = kQuadCorners[gl_VertexIndex];
    vec2 px = vec2(u.rect.x + t.x * u.rect.z,
                   u.rect.y + t.y * u.rect.w);
    vec2 ndc = (px / u.viewport) * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_pos = px;
}
