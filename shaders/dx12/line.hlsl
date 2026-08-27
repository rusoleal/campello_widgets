// ===========================================================================
// campello_widgets DirectX 12 shaders — line pipeline
//
// Segment body: a butt-ended (r=0) rotated box, antialiased via an SDF --
// caps (round/square) and joins (round/bevel/miter) are separate primitives
// layered on top by the backend (round = a filled circle via shape.hlsl,
// reused as-is; square = the backend extends this segment's own p1/p2 by
// half_w before building it, no shader change needed; bevel/miter = 1-2
// flat triangles via rect.hlsl) -- see D3DDrawBackend::strokePolyline().
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
    float4 pos    : SV_POSITION;
    float4 color  : COLOR0;
    float2 p1out  : TEXCOORD0;
    float2 p2out  : TEXCOORD1;
    float  half_w : TEXCOORD2;
};

LineVertOut LineVS(uint vid : SV_VertexID)
{
    float2 dir = p2.xy - p1.xy;
    float  len = length(dir);
    dir = (len > 0.0001) ? dir / len : float2(1.0, 0.0);
    float2 perp = float2(-dir.y, dir.x);

    // Inflate both perpendicular to the segment (half-width) and along its
    // axis (past p1/p2) by the AA band -- same reasoning as ShapeVS's
    // `inflate` (shape.hlsl): the SDF in LinePS still measures distance
    // from the true p1/p2/half_w box, but the rasterized quad must extend
    // past it or there are no pixels left to antialias into (the butt end
    // would clip hard instead of fading).
    const float aa   = 0.5;
    float half_w     = stroke_w * 0.5;
    float2 perp_off  = perp * (half_w + aa);
    float2 along_off = dir  * aa;

    // 4 corners: [p1-perp-along, p1+perp-along, p2+perp+along, p2-perp+along]
    float2 c0 = p1.xy - perp_off - along_off;
    float2 c1 = p1.xy + perp_off - along_off;
    float2 c2 = p2.xy + perp_off + along_off;
    float2 c3 = p2.xy - perp_off + along_off;
    float2 corners[6] = { c0, c1, c3, c1, c2, c3 };

    float2 px  = corners[vid];
    float2 ndc = (px / viewport) * 2.0 - 1.0;
    ndc.y = -ndc.y;

    LineVertOut o;
    o.pos    = float4(ndc, 0.0, 1.0);
    o.color  = color;
    o.p1out  = p1.xy;
    o.p2out  = p2.xy;
    o.half_w = half_w;
    return o;
}

float4 LinePS(LineVertOut input) : SV_TARGET
{
    float2 dir = input.p2out - input.p1out;
    float  len = length(dir);
    float2 dir_n  = (len > 0.0001) ? dir / len : float2(1.0, 0.0);
    float2 perp_n = float2(-dir_n.y, dir_n.x);

    // Rotate the fragment position (SV_POSITION, framebuffer pixel coords
    // with a 0.5-center offset -- same convention as ShapePS in shape.hlsl)
    // into the segment's local frame (axis = x, perpendicular = y), then
    // evaluate a plain box SDF (the r=0 case of the same rounded-box
    // formula ShapePS uses) -- a butt-ended rectangle of length `len` and
    // width `2*half_w` centered on the segment.
    float2 rel   = input.pos.xy - (input.p1out + input.p2out) * 0.5;
    float  along = dot(rel, dir_n);
    float  perp  = dot(rel, perp_n);

    float2 q = float2(abs(along) - len * 0.5, abs(perp) - input.half_w);
    float  d = length(max(q, float2(0.0, 0.0))) + min(max(q.x, q.y), 0.0);

    const float aa = 0.5;
    float alpha = 1.0 - smoothstep(-aa, aa, d);

    float4 col = input.color;
    col.a *= alpha;
    return float4(col.rgb * col.a, col.a);   // premultiplied output
}
