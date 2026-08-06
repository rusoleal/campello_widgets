#version 450

// Axis-aligned textured quad — push-constant driven, no vertex buffer.
// Position rect and UV rect are packed into push constants so that
// axis-aligned draws (all normal UI images and text) skip the per-draw
// vertex-pool acquire and setVertexBuffer call entirely.

layout(push_constant) uniform QuadAAUniforms {
    vec2  viewport;  // physical pixel size (w, h)
    float opacity;
    float _pad;
    vec4  pos;       // pixel-space rect: x, y, w, h
    vec4  uv;        // texture UV rect: u0, v0, u1, v1
} u;

layout(location = 0) out vec2  v_uv;
layout(location = 1) out float v_opacity;

const vec2 kCorners[6] = vec2[](
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
    vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(1.0, 1.0)
);

void main()
{
    vec2 t  = kCorners[gl_VertexIndex];
    vec2 px = vec2(u.pos.x + t.x * u.pos.z,
                   u.pos.y + t.y * u.pos.w);
    vec2 ndc = (px / u.viewport) * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);

    v_uv      = vec2(u.uv.x + t.x * (u.uv.z - u.uv.x),
                     u.uv.y + t.y * (u.uv.w - u.uv.y));
    v_opacity = u.opacity;
}
