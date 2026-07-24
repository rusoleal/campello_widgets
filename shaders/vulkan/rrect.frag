#version 450

layout(push_constant) uniform RRectUniforms {
    vec4  rect;
    vec4  color;
    vec2  viewport;
    float radius;
    float stroke_w;  // 0 = fill, >0 = stroke width in pixels
} u;

layout(location = 0) in  vec2 v_pos;
layout(location = 0) out vec4 out_color;

// Signed distance to an axis-aligned rounded rectangle centred at origin.
// b = half-extents, r = corner radius.
float roundedBox(vec2 p, vec2 b, float r)
{
    vec2 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main()
{
    vec2  center    = vec2(u.rect.x + u.rect.z * 0.5,
                           u.rect.y + u.rect.w * 0.5);
    vec2  half_size = vec2(u.rect.z, u.rect.w) * 0.5;
    float r         = clamp(u.radius, 0.0, min(half_size.x, half_size.y));

    float d = roundedBox(v_pos - center, half_size, r);

    float alpha;
    if (u.stroke_w > 0.0) {
        // Stroke: ring between the outer edge (d=0) and inner edge (d=-stroke_w).
        float outer = 1.0 - smoothstep(-0.5, 0.5, d);
        float inner = 1.0 - smoothstep(-0.5, 0.5, d + u.stroke_w);
        alpha = outer - inner;
    } else {
        alpha = 1.0 - smoothstep(-0.5, 0.5, d);
    }

    // Premultiplied-alpha output, matching rect.frag convention.
    vec4 c    = u.color;
    out_color = vec4(c.rgb * c.a, c.a) * alpha;
}
