// ===========================================================================
// campello_widgets DirectX 12 shaders — icon (tinted template image) pipeline
//
// Draws a "template" texture recolored to an arbitrary tint: the source
// texture's own RGB is ignored entirely, and only its alpha channel is
// sampled, used as a stencil for `tint` — the same mechanism as iOS's
// UIImage.withRenderingMode(.alwaysTemplate) and Android's icon tinting.
// Lets one monochrome icon asset serve any theme color. Mirrors Metal's
// iconVertex/iconFragment (shaders/metal/widgets.metal) and Vulkan's
// icon.vert/icon.frag field-for-field.
//
// Same binding shape as quad.hlsl (see its doc comment) — IconUniforms is
// simply larger (adds `tint`), and read from *both* the vertex and pixel
// stages (unlike QuadUniforms, vertex-only), since IconPS itself reads
// `tint`.
//   bind group 0: IconUniforms — register(b0), vertex + pixel stage
//   bind group 1: Texture2D — register(t0), SamplerState — register(s1), pixel stage
// ===========================================================================

cbuffer IconUniforms : register(b0)
{
    float2 viewport;  // width, height (pixels)
    float  opacity;   // [0, 1] — multiplied into the final alpha
    float  _pad;
    float4 tint;      // straight-alpha RGBA recolor
};

Texture2D    gTex : register(t0);
SamplerState gSmp : register(s1);

struct IconVertexIn
{
    float3 posw : TEXCOORD0;  // (x, y, w) in pixel space, pre-clip-space
    float2 uv   : TEXCOORD1;
};

struct IconVertOut
{
    float4 pos     : SV_POSITION;
    float2 uv      : TEXCOORD0;
    float  opacity : TEXCOORD1;
    float4 tint    : TEXCOORD2;
};

IconVertOut IconVS(IconVertexIn input)
{
    // Same clip-space derivation as QuadVS in quad.hlsl.
    float clip_x =  2.0 * input.posw.x / viewport.x - input.posw.z;
    float clip_y = -(2.0 * input.posw.y / viewport.y - input.posw.z);

    IconVertOut o;
    o.pos     = float4(clip_x, clip_y, 0.0, input.posw.z);
    o.uv      = input.uv;
    o.opacity = opacity;
    o.tint    = tint;
    return o;
}

float4 IconPS(IconVertOut input) : SV_TARGET
{
    // Ignore the source texture's own RGB; use only its alpha as a
    // stencil for the tint color. Output premultiplied, matching every
    // other pipeline's blend state (src=ONE dst=INV_SRC_ALPHA).
    float a = gTex.Sample(gSmp, input.uv).a * input.tint.a * input.opacity;
    return float4(input.tint.rgb * a, a);
}
