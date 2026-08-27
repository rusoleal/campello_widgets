#version 450

// Segment body: a butt-ended (r=0) rotated box, antialiased via an SDF --
// caps (round/square) and joins (round/bevel/miter) are separate primitives
// layered on top by the backend (round = a filled circle via rrect.frag
// reused as-is; square = the backend extends this segment's own p1/p2 by
// half_w before building it, no shader change needed; bevel/miter = 1-2
// flat triangles via rect.frag) -- see VulkanDrawBackend::strokePolyline().

layout(location = 0) in vec4  v_color;
layout(location = 1) in vec2  v_pos;
layout(location = 2) in vec2  v_p1;
layout(location = 3) in vec2  v_p2;
layout(location = 4) in float v_half_w;

layout(location = 0) out vec4 out_color;

void main()
{
    vec2 dir = v_p2 - v_p1;
    float len = length(dir);
    vec2 dir_n  = (len > 0.0001) ? dir / len : vec2(1.0, 0.0);
    vec2 perp_n = vec2(-dir_n.y, dir_n.x);

    // Rotate the fragment position into the segment's local frame (axis =
    // x, perpendicular = y), then evaluate a plain box SDF (r=0 case of the
    // same rounded-box formula rrect.frag uses) -- a butt-ended rectangle
    // of length `len` and width `2*half_w` centered on the segment.
    vec2 rel   = v_pos - (v_p1 + v_p2) * 0.5;
    float along = dot(rel, dir_n);
    float perp  = dot(rel, perp_n);

    vec2 q = vec2(abs(along) - len * 0.5, abs(perp) - v_half_w);
    float d = length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0);

    const float aa = 0.5;
    float alpha = 1.0 - smoothstep(-aa, aa, d);

    if (alpha <= 0.0) discard;

    vec4 c    = v_color;
    out_color = vec4(c.rgb * c.a, c.a) * alpha;
}
