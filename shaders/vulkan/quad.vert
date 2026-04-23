#version 450

layout(std140, set = 0, binding = 0) uniform QuadUniforms {
    vec4 dstRect;
    vec4 srcRect;
    vec2 viewport;
    float opacity;
    float _pad;
} u;

layout(location = 0) out vec2 v_uv;
layout(location = 1) out float v_opacity;

const vec2 kQuadCorners[6] = vec2[](
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
    vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(1.0, 1.0)
);

void main()
{
    vec2 t  = kQuadCorners[gl_VertexIndex];
    vec2 px = vec2(u.dstRect.x + t.x * u.dstRect.z,
                   u.dstRect.y + t.y * u.dstRect.w);
    vec2 ndc = (px / u.viewport) * 2.0 - 1.0;
    ndc.y = -ndc.y;

    gl_Position = vec4(ndc, 0.0, 1.0);
    v_uv = vec2(u.srcRect.x + t.x * (u.srcRect.z - u.srcRect.x),
                u.srcRect.y + t.y * (u.srcRect.w - u.srcRect.y));
    v_opacity = u.opacity;
}
