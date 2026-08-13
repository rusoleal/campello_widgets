#version 450

layout(location = 0)      in vec2  v_uv;
layout(location = 1) flat in float v_gradient_type;
layout(location = 2) flat in vec4  v_gradient_p1;
layout(location = 3) flat in vec4  v_gradient_p2;
layout(location = 4) flat in float v_blend_mode;

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
    } else {
        // Radial gradient
        vec2 center = v_gradient_p1.xy;
        float radius = v_gradient_p2.x;
        t = (radius > 0.0001) ? length(pos - center) / radius : 0.0;
    }
    t = clamp(t, 0.0, 1.0);

    vec4 mask_color = texture(sampler2D(u_lut, u_sampler), vec2(t, 0.5));

    if (v_blend_mode < 0.5) {
        // srcIn: output = child * mask.a
        out_color = child_color * mask_color.a;
    } else {
        // modulate: output = child * mask
        out_color = child_color * mask_color;
    }
}
