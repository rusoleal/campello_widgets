// ===========================================================================
// campello_widgets DirectX 12 shaders — quad (textured) pipeline
//
// Mirrors quadVertex/quadFragment in shaders/metal/widgets.metal. Draws a
// textured quad (text glyphs, images) from real per-vertex (x, y, w, u, v)
// data — each corner carries its own ambient-transform-projected position,
// so a rotated/perspective quad renders as a genuinely tilted shape instead
// of a resized axis-aligned box (see D3DDrawBackend::drawTexturedQuad()).
//
// Bindings — two SEPARATE BindGroupLayouts/root parameters (D3DDrawBackend
// binds them via setBindGroup(0, ...) and setBindGroup(1, ...) respectively):
//   bind group 0: QuadUniforms — register(b0, space0), vertex stage
//   bind group 1: Texture2D — register(t0, space1), SamplerState — register(s1, space1), pixel stage
// The uniform buffer is split from the texture/sampler because the former is
// small numeric data reused via a pooled ring buffer, while the latter
// varies per draw (arbitrary images) — see D3DDrawBackend::drawTexturedQuad().
//
// The explicit spaceN suffixes matter: campello_gpu's DirectX backend
// stamps each BindGroupLayout's descriptor ranges with RegisterSpace equal
// to that layout's index within the pipeline layout (see
// createUniversalRootSignature()'s doc comment in campello_gpu/src/directx/
// device.cpp) — the D3D12 equivalent of Vulkan's per-`set` binding restart.
// Leaving these implicit (defaulting to space0) makes the compiled shader's
// resource bindings not match the root signature the moment a pipeline has
// more than one bind group, which D3D12 rejects at PSO-creation time
// ("Root Signature doesn't match Pixel Shader: ... descriptor range ... is
// not fully bound in root signature").
// ===========================================================================

cbuffer QuadUniforms : register(b0)
{
    float2 viewport;  // width, height (pixels)
    float  opacity;   // [0, 1] — multiplied into every pixel
    float  _pad;
};

Texture2D    gTex : register(t0, space1);
SamplerState gSmp : register(s1, space1);

struct QuadVertexIn
{
    float3 posw : TEXCOORD0;  // (x, y, w) in pixel space, pre-clip-space
    float2 uv   : TEXCOORD1;
};

struct QuadVertOut
{
    float4 pos     : SV_POSITION;
    float2 uv      : TEXCOORD0;
    float  opacity : TEXCOORD1;
};

QuadVertOut QuadVS(QuadVertexIn input)
{
    // See rect.hlsl's RectVS for the clip-space derivation — identical here.
    float clip_x =  2.0 * input.posw.x / viewport.x - input.posw.z;
    float clip_y = -(2.0 * input.posw.y / viewport.y - input.posw.z);

    QuadVertOut o;
    o.pos     = float4(clip_x, clip_y, 0.0, input.posw.z);
    o.uv      = input.uv;
    o.opacity = opacity;
    return o;
}

float4 QuadPS(QuadVertOut input) : SV_TARGET
{
    // The texture is already premultiplied-alpha; scaling all channels by
    // opacity preserves premultiplication and produces correct blending.
    return gTex.Sample(gSmp, input.uv) * input.opacity;
}
