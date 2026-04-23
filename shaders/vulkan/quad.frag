#version 450

layout(location = 0) in vec2 v_uv;
layout(location = 1) in float v_opacity;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 1) uniform texture2D u_texture;
layout(set = 0, binding = 2) uniform sampler   u_sampler;

void main()
{
    out_color = texture(sampler2D(u_texture, u_sampler), v_uv) * v_opacity;
}
