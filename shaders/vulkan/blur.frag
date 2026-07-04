#version 450

layout(location = 0)      in vec2  v_uv;
layout(location = 1) flat in float v_sigma;
layout(location = 2) flat in float v_horizontal;
layout(location = 3) flat in vec2  v_tex_size;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 1) uniform texture2D u_texture;
layout(set = 0, binding = 2) uniform sampler   u_sampler;

void main()
{
    float sigma  = max(v_sigma, 0.5);
    int   RADIUS = min(int(ceil(2.5 * sigma)), 12);

    vec2 step = (v_horizontal > 0.5)
        ? vec2(1.0 / v_tex_size.x, 0.0)
        : vec2(0.0, 1.0 / v_tex_size.y);

    vec4  color = vec4(0.0);
    float wsum  = 0.0;

    for (int i = -RADIUS; i <= RADIUS; ++i)
    {
        float fi     = float(i);
        float weight = exp(-0.5 * (fi / sigma) * (fi / sigma));
        vec2  suv    = clamp(v_uv + fi * step, vec2(0.0), vec2(1.0));
        color += texture(sampler2D(u_texture, u_sampler), suv) * weight;
        wsum  += weight;
    }

    out_color = color / wsum;
}
