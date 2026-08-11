// ===========================================================================
// campello_widgets DirectX 12 shaders — ShaderMask composite pipeline
//
// Mirrors shaderMaskVertex/shaderMaskFragment in shaders/metal/widgets.metal.
// Composites an offscreen child texture with a gradient mask (linear or
// radial) sampled from a 256x1 LUT texture.
//
// Bindings:
//   bind group 0: ShaderMaskUniforms — register(b0), vertex stage
//   bind group 1: Texture2D child — register(t0)
//                 Texture2D lut   — register(t1)
//                 SamplerState    — register(s2), pixel stage
// ===========================================================================

cbuffer ShaderMaskUniforms : register(b0)
{
    float2 viewport;      // framebuffer width, height
    float  gradient_type; // 0 = linear, 1 = radial
    float  _pad0;
    float4 gradient_p1;   // linear: begin.xy; radial: center.xy (pixels)
    float4 gradient_p2;   // linear: end.xy;   radial: radius in .x (pixels)
    float  blend_mode;    // 0 = srcIn, 1 = modulate
    float3 _pad1;
};

Texture2D    gChildTex : register(t0);
Texture2D    gLutTex   : register(t1);
SamplerState gSmp      : register(s2);

struct ShaderMaskVertexIn
{
    float3 posw : TEXCOORD0;  // (x, y, w) in pixel space, pre-clip-space
    float2 uv   : TEXCOORD1;
};

struct ShaderMaskVertOut
{
    float4 pos           : SV_POSITION;
    float2 uv            : TEXCOORD0;
    float  gradient_type : TEXCOORD1;
    float4 gradient_p1   : TEXCOORD2;
    float4 gradient_p2   : TEXCOORD3;
    float  blend_mode    : TEXCOORD4;
};

ShaderMaskVertOut ShaderMaskVS(ShaderMaskVertexIn input)
{
    float clip_x =  2.0 * input.posw.x / viewport.x - input.posw.z;
    float clip_y = -(2.0 * input.posw.y / viewport.y - input.posw.z);

    ShaderMaskVertOut o;
    o.pos           = float4(clip_x, clip_y, 0.0, input.posw.z);
    o.uv            = input.uv;
    o.gradient_type = gradient_type;
    o.gradient_p1   = gradient_p1;
    o.gradient_p2   = gradient_p2;
    o.blend_mode    = blend_mode;
    return o;
}

float4 ShaderMaskPS(ShaderMaskVertOut input) : SV_TARGET
{
    float4 child_color = gChildTex.Sample(gSmp, input.uv);

    float2 pos = input.pos.xy;
    float t;
    if (input.gradient_type < 0.5)
    {
        float2 p1  = input.gradient_p1.xy;
        float2 p2  = input.gradient_p2.xy;
        float2 dir = p2 - p1;
        float  len2 = dot(dir, dir);
        t = (len2 > 0.0001) ? dot(pos - p1, dir) / len2 : 0.0;
    }
    else
    {
        float2 center = input.gradient_p1.xy;
        float  radius = input.gradient_p2.x;
        t = (radius > 0.0001) ? length(pos - center) / radius : 0.0;
    }
    t = saturate(t);

    float4 mask_color = gLutTex.Sample(gSmp, float2(t, 0.5));

    if (input.blend_mode < 0.5)
        return child_color * mask_color.a;
    else
        return child_color * mask_color;
}
