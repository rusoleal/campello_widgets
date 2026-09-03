// ===========================================================================
// campello_widgets DirectX 12 shaders — clip-shape composite pipeline
//
// Mirrors clipShapeVertex/clipShapeFragment in shaders/metal/widgets.metal —
// composites a child texture (an offscreen-rendered ClipRRect/ClipOval
// subtree) through a rounded-rect or ellipse SDF mask.
//
// Real per-vertex position(+w)/uv data — same QuadVertex layout as quad.hlsl
// (position is the ambient-transform-projected destination corner; uv is
// this corner's [0,1]^2 coordinate within the shape's own local space, so the
// SDF below renders correctly regardless of rotation/perspective/mirroring).
//
// Bindings — two SEPARATE bind groups (same split as quad.hlsl/blur.hlsl):
//   bind group 0: ClipShapeUniforms — register(b0, space0), vertex stage
//   bind group 1: Texture2D — register(t0, space1), SamplerState — register(s1, space1), pixel stage
// See quad.hlsl's doc comment for why the spaceN suffixes are required.
// ===========================================================================

cbuffer ClipShapeUniforms : register(b0)
{
    float2 rect_size;  // shape's plain LOGICAL width/height — not physical
                        // pixels, not transform-scaled; the SDF operates in
                        // local (uv) space.
    float2 viewport;   // framebuffer width, height — clip-space conversion only
    float  corner_r;   // LOGICAL corner radius (rrect); ignored when kind == 1
    float  kind;       // 0 = rounded rect, 1 = ellipse/oval
    float2 _pad;
};

Texture2D    gTex : register(t0, space1);
SamplerState gSmp : register(s1, space1);

struct ClipShapeVertexIn
{
    float3 posw : TEXCOORD0;  // (x, y, w) in pixel space, pre-clip-space
    float2 uv   : TEXCOORD1;
};

struct ClipShapeVertOut
{
    float4 pos       : SV_POSITION;
    float2 uv        : TEXCOORD0;
    float2 rect_size : TEXCOORD1;
    float  corner_r  : TEXCOORD2;
    float  kind      : TEXCOORD3;
};

ClipShapeVertOut ClipShapeVS(ClipShapeVertexIn input)
{
    // See quad.hlsl's QuadVS for the pixel-space-(x,y,w) -> clip-space derivation.
    float clip_x =  2.0 * input.posw.x / viewport.x - input.posw.z;
    float clip_y = -(2.0 * input.posw.y / viewport.y - input.posw.z);

    ClipShapeVertOut o;
    o.pos       = float4(clip_x, clip_y, 0.0, input.posw.z);
    o.uv        = input.uv;
    o.rect_size = rect_size;
    o.corner_r  = corner_r;
    o.kind      = kind;
    return o;
}

float4 ClipShapePS(ClipShapeVertOut input) : SV_TARGET
{
    float4 child_color = gTex.Sample(gSmp, input.uv);

    // SDF evaluated in logical pixel space, centred on the shape — mirrors
    // Metal's clipShapeFragment exactly.
    float2 hs = input.rect_size * 0.5;
    float2 p  = (input.uv - float2(0.5, 0.5)) * input.rect_size;

    float d;
    if (input.kind < 0.5)
    {
        // Rounded-rect SDF
        float r  = min(input.corner_r, min(hs.x, hs.y));
        float2 q = abs(p) - hs + r;
        d = length(max(q, float2(0.0, 0.0))) + min(max(q.x, q.y), 0.0) - r;
    }
    else
    {
        // Ellipse SDF (circle when hs.x == hs.y)
        float2 s = p / hs;
        d = (length(s) - 1.0) * min(hs.x, hs.y);
    }

    float alpha = 1.0 - smoothstep(-0.5, 0.5, d);
    return child_color * alpha;
}
