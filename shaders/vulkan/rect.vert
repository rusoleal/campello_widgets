#version 450

layout(std140, set = 0, binding = 0) uniform RectUniforms {
    vec4 rect;
    vec4 color;
    vec2 viewport;
    vec2 _pad;
} u;

layout(location = 0) out vec4 v_color;

const vec2 kQuadCorners[6] = vec2[](
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
    vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(1.0, 1.0)
);

void main()
{
    vec2 t  = kQuadCorners[gl_VertexIndex];
    vec2 px = vec2(u.rect.x + t.x * u.rect.z,
                   u.rect.y + t.y * u.rect.w);
    // campello_gpu uses a standard positive-height Vulkan viewport:
    // y=-1 is TOP, y=+1 is BOTTOM. (px/viewport)*2-1 already gives correct NDC.
    vec2 ndc = (px / u.viewport) * 2.0 - 1.0;

    gl_Position = vec4(ndc, 0.0, 1.0);
    v_color = u.color;
}
