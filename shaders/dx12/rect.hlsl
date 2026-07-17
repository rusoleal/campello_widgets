// ===========================================================================
// campello_widgets DirectX 12 shaders — rect pipeline
//
// Mirrors rectVertex/rectFragment in shaders/metal/widgets.metal (the
// source-of-truth signature). Draws a solid-colored quad from real
// per-vertex (x, y, w) positions (computed on the CPU by ambient-transforming
// each corner independently — see D3DDrawBackend::drawFilledQuad()).
//
// Bindings (must match the BindGroupLayout built in D3DDrawBackend's
// constructor — register number == EntryObject::binding):
//   RectUniforms  — register(b0), vertex stage
//
// Vertex input layout: campello_gpu's DirectX backend always assigns the
// semantic name "TEXCOORD" with SemanticIndex = shaderLocation (see
// toDXGIVertexFormat()/createRenderPipeline() in campello_gpu's device.cpp),
// so vertex inputs here MUST use TEXCOORD0/TEXCOORD1/... regardless of what
// the attribute conceptually represents.
// ===========================================================================

cbuffer RectUniforms : register(b0)
{
    float4 color;     // r, g, b, a (straight alpha)
    float2 viewport;  // width, height (pixels)
    float2 _pad;
};

struct RectVertexIn
{
    float3 posw : TEXCOORD0;  // (x, y, w) in pixel space, pre-clip-space
};

struct RectVertOut
{
    float4 pos   : SV_POSITION;
    float4 color : COLOR0;
};

RectVertOut RectVS(RectVertexIn input)
{
    // Same derivation as quadVertex in widgets.metal: setting clip.w = pos.w
    // and letting the hardware perspective divide (clip.xy / clip.w) run
    // reproduces the existing pixel->NDC mapping when w == 1, and genuinely
    // foreshortens when w != 1 (rotated/perspective ambient transforms).
    float clip_x =  2.0 * input.posw.x / viewport.x - input.posw.z;
    float clip_y = -(2.0 * input.posw.y / viewport.y - input.posw.z);

    RectVertOut o;
    o.pos   = float4(clip_x, clip_y, 0.0, input.posw.z);
    o.color = color;
    return o;
}

float4 RectPS(RectVertOut input) : SV_TARGET
{
    // Premultiply alpha so this pipeline can share the same
    // src=ONE dst=ONE_MINUS_SRC_ALPHA blend equation as the quad pipeline.
    return float4(input.color.rgb * input.color.a, input.color.a);
}
