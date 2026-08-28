// ===========================================================================
// campello_widgets DirectX 12 shaders — rect-AA pipeline
//
// Identical to rect.hlsl, plus a per-vertex alpha (packed as posw_a.w),
// linearly interpolated by the rasterizer and multiplied into RectUniforms'
// color in the pixel shader. Used for drawPath()'s fill antialiasing
// "skirt" -- see src/gpu/path_fill_aa.hpp and D3DDrawBackend::drawPath().
// A separate pipeline (not an addition to rect.hlsl/RectVS/RectPS) so every
// existing rect_pipeline_ call site (drawArc, drawRect fill, stroke-wedge
// fallback) stays completely unchanged.
//
// Bindings: RectUniforms — register(b0), vertex stage (shared with the
// plain rect pipeline — identical layout).
//
// Vertex input layout: see rect.hlsl's doc comment on the mandatory
// TEXCOORD0/TEXCOORD1/... semantic naming convention.
// ===========================================================================

cbuffer RectUniforms : register(b0)
{
    float4 color;     // r, g, b, a (straight alpha)
    float2 viewport;  // width, height (pixels)
    float2 _pad;
};

struct RectAAVertexIn
{
    float4 posw_a : TEXCOORD0;  // (x, y, w, alpha)
};

struct RectAAVertOut
{
    float4 pos   : SV_POSITION;
    float4 color : COLOR0;
    float  alpha : TEXCOORD1;
};

RectAAVertOut RectAAVS(RectAAVertexIn input)
{
    float clip_x =  2.0 * input.posw_a.x / viewport.x - input.posw_a.z;
    float clip_y = -(2.0 * input.posw_a.y / viewport.y - input.posw_a.z);

    RectAAVertOut o;
    o.pos   = float4(clip_x, clip_y, 0.0, input.posw_a.z);
    o.color = color;
    o.alpha = input.posw_a.w;
    return o;
}

float4 RectAAPS(RectAAVertOut input) : SV_TARGET
{
    float4 c = input.color;
    c.a *= input.alpha;
    return float4(c.rgb * c.a, c.a);
}
