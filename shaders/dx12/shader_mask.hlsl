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
    float  gradient_type; // 0 = linear, 1 = radial, 2 = sweep
    float  tile_mode;     // 0 = clamp, 1 = repeated, 2 = mirror
    float4 gradient_p1;   // linear: begin.xy; radial/sweep: center.xy (pixels)
    float4 gradient_p2;   // linear: end.xy; radial: radius in .x; sweep: start/end angle in .xy (radians)
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
    float  tile_mode     : TEXCOORD5;
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
    o.tile_mode     = tile_mode;
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
    else if (input.gradient_type < 1.5)
    {
        float2 center = input.gradient_p1.xy;
        float  radius = input.gradient_p2.x;
        t = (radius > 0.0001) ? length(pos - center) / radius : 0.0;
    }
    else
    {
        // Sweep gradient: angle from center, normalized to [start, end].
        float2 center      = input.gradient_p1.xy;
        float  start_angle = input.gradient_p2.x;
        float  end_angle   = input.gradient_p2.y;
        float2 d           = pos - center;
        float  angle       = atan2(d.y, d.x);
        if (angle < 0.0) angle += 2.0 * 3.14159265358979323846;
        float span = end_angle - start_angle;
        t = (abs(span) > 0.0001) ? (angle - start_angle) / span : 0.0;
    }

    if (input.tile_mode < 0.5)
    {
        t = saturate(t); // clamp
    }
    else if (input.tile_mode < 1.5)
    {
        t = frac(t); // repeated
    }
    else
    {
        float period = t - 2.0 * floor(t * 0.5); // mirror: triangle wave, period 2
        t = (period > 1.0) ? (2.0 - period) : period;
    }

    float4 mask_color = gLutTex.Sample(gSmp, float2(t, 0.5));

    if (input.blend_mode < 0.5)
        return child_color * mask_color.a;
    else
        return child_color * mask_color;
}
