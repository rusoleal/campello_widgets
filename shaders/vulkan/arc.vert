#version 450

// Arc pipeline — antialiased circular/elliptical arc via SDF. A dedicated
// pipeline, not an addition to rrect_pipeline_ above: that pipeline is used
// by every drawCircle/drawOval/drawRRect call in the whole app, so giving it
// an optional angular cut would mean every one of those call sites needs a
// new "no clip" sentinel to avoid ever accidentally clipping a shape that
// was never meant to have one. Isolating the new, less-proven arc math to
// its own pipeline means zero risk to that existing, working, well-tested
// path. See arc.frag for the SDF technique (mirrors shaders/metal/
// widgets.metal's arcVertex/arcFragment, verified there via a real pixel
// golden-image test before being ported here).

layout(push_constant) uniform ArcUniforms {
    vec4  rect;        // x, y, w, h — bounding box (pixels)
    vec4  color;       // r, g, b, a — straight alpha
    vec2  viewport;    // framebuffer w, h (pixels)
    float stroke_w;    // 0 = solid wedge fill to center; >0 = arc-band stroke width
    float start_angle; // radians, 0 = 3 o'clock, screen space (y-down)
    float sweep_angle; // radians, positive = clockwise on screen
} u;

layout(location = 0) out vec2 v_pos;  // pixel-space position for SDF in frag

const vec2 kQuadCorners[6] = vec2[](
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
    vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(1.0, 1.0)
);

void main()
{
    // Identical inflation rule to rrect.vert's (see its doc comment) —
    // matched exactly rather than independently re-derived.
    const float aa = 0.5;
    float inflate = (u.stroke_w > 0.0) ? (u.stroke_w * 0.5 + aa) : 0.0;

    vec2 t      = kQuadCorners[gl_VertexIndex];
    vec2 origin = u.rect.xy - inflate;
    vec2 size   = u.rect.zw + inflate * 2.0;
    vec2 px     = origin + t * size;
    vec2 ndc    = (px / u.viewport) * 2.0 - 1.0;

    gl_Position = vec4(ndc, 0.0, 1.0);
    v_pos       = px;
}
