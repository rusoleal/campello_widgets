#version 450

layout(push_constant) uniform LineUniforms {
    vec4  p1;        // xy: start (pixels), zw: unused
    vec4  p2;        // xy: end   (pixels), zw: unused
    vec4  color;     // r, g, b, a (straight alpha)
    vec2  viewport;  // framebuffer w, h (pixels)
    float stroke_w;  // line thickness (pixels)
    float _pad;
} u;

layout(location = 0) out vec4  v_color;
layout(location = 1) out vec2  v_pos;
layout(location = 2) out vec2  v_p1;
layout(location = 3) out vec2  v_p2;
layout(location = 4) out float v_half_w;

const vec2 kQuadCorners[6] = vec2[](
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
    vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(1.0, 1.0)
);

void main()
{
    vec2 dir = u.p2.xy - u.p1.xy;
    float len = length(dir);
    if (len > 0.0001) dir /= len; else dir = vec2(1.0, 0.0);
    vec2 perp = vec2(-dir.y, dir.x);

    // Inflate both perpendicular to the segment (half-width) and along its
    // axis (past p1/p2) by the AA band -- same reasoning as rrect.vert's
    // `inflate`: the SDF in line.frag still measures distance from the true
    // p1/p2/half_w box, but the rasterized quad must extend past it or
    // there are no fragments left to antialias into (the butt end would
    // clip hard instead of fading).
    const float aa    = 0.5;
    float half_w      = u.stroke_w * 0.5;
    vec2  perp_off    = perp * (half_w + aa);
    vec2  along_off   = dir  * aa;

    int  idx[6]     = int[](0, 1, 3, 1, 2, 3);
    vec2 corners[4] = vec2[](
        u.p1.xy - perp_off - along_off,
        u.p1.xy + perp_off - along_off,
        u.p2.xy + perp_off + along_off,
        u.p2.xy - perp_off + along_off
    );

    vec2 px  = corners[idx[gl_VertexIndex]];
    vec2 ndc = (px / u.viewport) * 2.0 - 1.0;

    gl_Position = vec4(ndc, 0.0, 1.0);
    v_color  = u.color;
    v_pos    = px;
    v_p1     = u.p1.xy;
    v_p2     = u.p2.xy;
    v_half_w = half_w;
}
