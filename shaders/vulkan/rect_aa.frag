#version 450

// Identical to rect.frag, plus multiplying in the per-vertex (interpolated)
// alpha from rect_aa.vert -- see src/gpu/path_fill_aa.hpp's doc comment.

layout(location = 0) in vec4  v_color;
layout(location = 1) in float v_alpha;
layout(location = 0) out vec4 out_color;

void main()
{
    vec4 c = v_color;
    c.a *= v_alpha;
    out_color = vec4(c.rgb * c.a, c.a);
}
