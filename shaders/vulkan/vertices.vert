#version 450

// Canvas.drawVertices() -- an arbitrary triangle mesh where each vertex
// carries its own full RGBA color (already paint-blended on the CPU, see
// Canvas::drawVertices()'s doc comment), linearly interpolated across each
// triangle. Structurally identical to rect_aa.vert minus its single-shared-
// uniform-color specialization: no uniform color at all, since color is
// entirely a per-vertex attribute.

layout(push_constant) uniform VerticesUniforms {
    vec2 viewport;  // physical pixel size
    vec2 _pad;
} u;

layout(location = 0) in vec3 in_posw;   // (x, y, w) -- CPU-transformed clip-space position
layout(location = 1) in vec4 in_color;  // (r, g, b, a) -- straight alpha

layout(location = 0) out vec4 v_color;

void main()
{
    float clip_x = 2.0 * in_posw.x / u.viewport.x - in_posw.z;
    float clip_y = 2.0 * in_posw.y / u.viewport.y - in_posw.z;
    gl_Position  = vec4(clip_x, clip_y, 0.0, in_posw.z);
    v_color      = in_color;
}
