#version 450

// Outputs the GPU-interpolated per-vertex color directly -- it's already
// paint-blended (see vertices.vert / Canvas::drawVertices()'s doc comment),
// so no further blend math is needed here, unlike rect_aa.frag which still
// multiplies in a separate per-vertex alpha.

layout(location = 0) in vec4 v_color;
layout(location = 0) out vec4 out_color;

void main()
{
    vec4 c = v_color;
    out_color = vec4(c.rgb * c.a, c.a);
}
