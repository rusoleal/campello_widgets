// ===========================================================================
// campello_widgets DirectX 12 shaders — line pipeline
//
// Mirrors lineVertex in shaders/metal/widgets.metal — an arbitrary-angle
// line segment rendered as a rotated quad. LinePS is a duplicate of
// RectPS/rect.hlsl (Metal's lineFragment reuses rectFragment directly;
// HLSL shader modules are one-entry-point-per-blob on this backend, so the
// same small premultiply is just repeated here rather than shared).
//
// Bindings: LineUniforms — register(b0), vertex stage.
// ===========================================================================

cbuffer LineUniforms : register(b0)
{
    float4 p1;        // xy: start (pixels), zw: unused
    float4 p2;        // xy: end   (pixels), zw: unused
    float4 color;     // r, g, b, a
    float2 viewport;  // framebuffer w, h
    float  stroke_w;  // line thickness (pixels)
    float  _pad;
};

struct LineVertOut
{
    float4 pos   : SV_POSITION;
    float4 color : COLOR0;
};

LineVertOut LineVS(uint vid : SV_VertexID)
{
    float2 dir = p2.xy - p1.xy;
    float  len = length(dir);
    dir = (len > 0.0001) ? dir / len : float2(1.0, 0.0);
    float2 perp = float2(-dir.y, dir.x) * (stroke_w * 0.5);

    // 4 corners: [p1-perp, p1+perp, p2+perp, p2-perp], two CCW triangles.
    float2 c0 = p1.xy - perp;
    float2 c1 = p1.xy + perp;
    float2 c2 = p2.xy + perp;
    float2 c3 = p2.xy - perp;
    float2 corners[6] = { c0, c1, c3, c1, c2, c3 };

    float2 px  = corners[vid];
    float2 ndc = (px / viewport) * 2.0 - 1.0;
    ndc.y = -ndc.y;

    LineVertOut o;
    o.pos   = float4(ndc, 0.0, 1.0);
    o.color = color;
    return o;
}

float4 LinePS(LineVertOut input) : SV_TARGET
{
    return float4(input.color.rgb * input.color.a, input.color.a);
}
