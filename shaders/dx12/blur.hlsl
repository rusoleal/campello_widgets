// ===========================================================================
// campello_widgets DirectX 12 shaders — blur pipeline
//
// Mirrors blurVertex/blurFragment in shaders/metal/widgets.metal — a
// separable two-pass Gaussian blur (horizontal pass, then vertical pass).
// No vertex buffers — geometry is generated from SV_VertexID.
//
// Bindings — two SEPARATE bind groups (same split as quad.hlsl, and for the
// same reason: BlurUniforms is pooled/reused, the source texture is not):
//   bind group 0: BlurUniforms — register(b0, space0), vertex stage
//   bind group 1: Texture2D — register(t0, space1), SamplerState — register(s1, space1), pixel stage
// See quad.hlsl's doc comment for why the spaceN suffixes are required.
// ===========================================================================

cbuffer BlurUniforms : register(b0)
{
    float4 dstRect;    // x, y, w, h (pixels, destination quad)
    float4 srcRect;    // u0, v0, u1, v1 (normalised UV of source region)
    float2 viewport;   // framebuffer width, height
    float  sigma;      // Gaussian sigma (pixels)
    float  horizontal; // 1.0 = horizontal pass, 0.0 = vertical pass
    float2 tex_size;   // source texture width, height (pixels)
    float2 _pad;
};

Texture2D    gTex : register(t0, space1);
SamplerState gSmp : register(s1, space1);

static const float2 kQuadCorners[6] = {
    float2(0.0, 0.0), float2(1.0, 0.0), float2(0.0, 1.0),
    float2(0.0, 1.0), float2(1.0, 0.0), float2(1.0, 1.0),
};

struct BlurVertOut
{
    float4 pos        : SV_POSITION;
    float2 uv         : TEXCOORD0;
    float  sigma      : TEXCOORD1;
    float  horizontal : TEXCOORD2;
    float2 tex_size   : TEXCOORD3;
};

BlurVertOut BlurVS(uint vid : SV_VertexID)
{
    float2 t  = kQuadCorners[vid];
    float2 px = float2(dstRect.x + t.x * dstRect.z, dstRect.y + t.y * dstRect.w);
    float2 ndc = (px / viewport) * 2.0 - 1.0;
    ndc.y = -ndc.y;

    float2 uv = float2(srcRect.x + t.x * (srcRect.z - srcRect.x),
                       srcRect.y + t.y * (srcRect.w - srcRect.y));

    BlurVertOut o;
    o.pos        = float4(ndc, 0.0, 1.0);
    o.uv         = uv;
    o.sigma      = sigma;
    o.horizontal = horizontal;
    o.tex_size   = tex_size;
    return o;
}

float4 BlurPS(BlurVertOut input) : SV_TARGET
{
    const float s = max(input.sigma, 0.5);
    const int RADIUS = min(int(ceil(2.5 * s)), 12);

    float2 step = (input.horizontal > 0.5)
        ? float2(1.0 / input.tex_size.x, 0.0)
        : float2(0.0, 1.0 / input.tex_size.y);

    float4 color = float4(0.0, 0.0, 0.0, 0.0);
    float  wsum  = 0.0;

    for (int i = -RADIUS; i <= RADIUS; ++i)
    {
        float fi     = float(i);
        float weight = exp(-0.5 * (fi / s) * (fi / s));
        float2 suv   = clamp(input.uv + fi * step, float2(0.0, 0.0), float2(1.0, 1.0));
        // SampleLevel (explicit LOD), not Sample: the latter needs
        // screen-space derivatives, which are undefined/disallowed inside a
        // dynamic-trip-count loop and force fxc to (fail to) unroll it.
        color += gTex.SampleLevel(gSmp, suv, 0.0) * weight;
        wsum  += weight;
    }

    return color / wsum;
}
