#version 450

layout(push_constant) uniform RRectUniforms {
    vec4  rect;      // x, y, w, h (screen pixels)
    vec4  color;     // r, g, b, a (premultiplied in frag)
    vec2  viewport;  // screen width, height
    float radius;    // corner radius in pixels
    float stroke_w;  // 0 = fill, >0 = stroke width in pixels
} u;

layout(location = 0) out vec2 v_pos;  // pixel-space position for SDF in frag

const vec2 kQuadCorners[6] = vec2[](
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
    vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(1.0, 1.0)
);

void main()
{
    // rrect.frag's stroke is drawn inset from the boundary (the ring
    // between d=0 and d=-stroke_w), so it never itself extends past the
    // logical rect -- but the outer edge's antialiasing band (smoothstep
    // over d in [-0.5, 0.5]) does, and the quad below stops exactly at the
    // rect boundary. Without inflating it, that outward half of the AA
    // falloff never gets rasterized -- not clipped, just outside every
    // triangle this draw call submits, leaving a harder/aliased outer edge
    // instead of the intended smooth falloff. Same root cause as
    // shapeVertex() in shaders/metal/widgets.metal (which has centered,
    // not inset, strokes -- there this also hides half the stroke itself).
    const float aa = 0.5;
    float inflate = (u.stroke_w > 0.0) ? (u.stroke_w * 0.5 + aa) : 0.0;

    vec2 t      = kQuadCorners[gl_VertexIndex];
    vec2 origin = u.rect.xy - inflate;
    vec2 size   = u.rect.zw + inflate * 2.0;
    vec2 px     = origin + t * size;
    vec2 ndc = (px / u.viewport) * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_pos = px;
}
