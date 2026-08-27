#version 450

layout(location = 0)      in vec2  v_uv;
layout(location = 1) flat in float v_gradient_type;
layout(location = 2) flat in vec4  v_gradient_p1;
layout(location = 3) flat in vec4  v_gradient_p2;
layout(location = 4) flat in float v_blend_mode;
layout(location = 5) flat in float v_tile_mode;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 1) uniform texture2D u_child;
layout(set = 0, binding = 2) uniform texture2D u_lut;
layout(set = 0, binding = 3) uniform sampler   u_sampler;

void main()
{
    vec4 child_color = texture(sampler2D(u_child, u_sampler), v_uv);

    // Reconstruct fragment position in viewport pixels.
    vec2 pos = gl_FragCoord.xy;

    float t;
    if (v_gradient_type < 0.5) {
        // Linear gradient
        vec2 p1  = v_gradient_p1.xy;
        vec2 p2  = v_gradient_p2.xy;
        vec2 dir = p2 - p1;
        float len2 = dot(dir, dir);
        t = (len2 > 0.0001) ? dot(pos - p1, dir) / len2 : 0.0;
    } else if (v_gradient_type < 1.5) {
        // Radial gradient
        vec2 center = v_gradient_p1.xy;
        float radius = v_gradient_p2.x;
        t = (radius > 0.0001) ? length(pos - center) / radius : 0.0;
    } else {
        // Sweep gradient: angle from center, normalized to [start, end].
        vec2  center      = v_gradient_p1.xy;
        float start_angle = v_gradient_p2.x;
        float end_angle   = v_gradient_p2.y;
        vec2  d           = pos - center;
        float angle       = atan(d.y, d.x);
        if (angle < 0.0) angle += 2.0 * 3.14159265358979323846;
        float span = end_angle - start_angle;
        t = (abs(span) > 0.0001) ? (angle - start_angle) / span : 0.0;
    }

    // Apply tile mode.
    if (v_tile_mode < 0.5) {
        t = clamp(t, 0.0, 1.0); // clamp
    } else if (v_tile_mode < 1.5) {
        t = fract(t); // repeated
    } else {
        float period = t - 2.0 * floor(t * 0.5); // mirror: triangle wave, period 2
        t = (period > 1.0) ? (2.0 - period) : period;
    }

    vec4 mask_color = texture(sampler2D(u_lut, u_sampler), vec2(t, 0.5));

    if (v_blend_mode < 0.5) {
        // srcIn: output = child * mask.a
        out_color = child_color * mask_color.a;
    } else {
        // modulate: output = child * mask
        out_color = child_color * mask_color;
    }
}
