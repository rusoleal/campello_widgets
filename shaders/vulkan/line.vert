#version 450

layout(push_constant) uniform LineUniforms {
    vec4  p1;        // xy: start (pixels), zw: unused
    vec4  p2;        // xy: end   (pixels), zw: unused
    vec4  color;     // r, g, b, a (straight alpha)
    vec2  viewport;  // framebuffer w, h (pixels)
    float stroke_w;  // line thickness (pixels)
    float _pad;
} u;

layout(location = 0) out vec4 v_color;

const vec2 kQuadCorners[6] = vec2[](
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
    vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(1.0, 1.0)
);

void main()
{
    vec2 dir = u.p2.xy - u.p1.xy;
    float len = length(dir);
    if (len > 0.0001) dir /= len; else dir = vec2(1.0, 0.0);
    vec2 perp = vec2(-dir.y, dir.x) * (u.stroke_w * 0.5);

    // 4 corners: [p1-perp, p1+perp, p2+perp, p2-perp]
    int   idx[6]     = int[](0, 1, 3, 1, 2, 3);
    vec2  corners[4] = vec2[](
        u.p1.xy - perp,
        u.p1.xy + perp,
        u.p2.xy + perp,
        u.p2.xy - perp
    );

    vec2 px  = corners[idx[gl_VertexIndex]];
    vec2 ndc = (px / u.viewport) * 2.0 - 1.0;

    gl_Position = vec4(ndc, 0.0, 1.0);
    v_color     = u.color;
}
