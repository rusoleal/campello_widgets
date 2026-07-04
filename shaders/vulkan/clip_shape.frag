#version 450

layout(location = 0)      in vec2  v_uv;
layout(location = 1) flat in vec2  v_rect_size;
layout(location = 2) flat in float v_corner_r;
layout(location = 3) flat in float v_kind;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 1) uniform texture2D u_texture;
layout(set = 0, binding = 2) uniform sampler   u_sampler;

void main()
{
    vec4 child_color = texture(sampler2D(u_texture, u_sampler), v_uv);

    // SDF evaluated in logical pixel space, centred on the shape.
    // Mirrors Metal's clipShapeFragment exactly.
    vec2  hs = v_rect_size * 0.5;
    vec2  p  = (v_uv - vec2(0.5)) * v_rect_size;

    float d;
    if (v_kind < 0.5) {
        // Rounded-rect SDF
        float r = min(v_corner_r, min(hs.x, hs.y));
        vec2  q = abs(p) - hs + r;
        d = length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - r;
    } else {
        // Ellipse SDF (circle when hs.x == hs.y)
        vec2 s = p / hs;
        d = (length(s) - 1.0) * min(hs.x, hs.y);
    }

    float alpha = 1.0 - smoothstep(-0.5, 0.5, d);
    out_color = child_color * alpha;
}
