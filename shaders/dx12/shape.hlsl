// ===========================================================================
// campello_widgets DirectX 12 shaders — shape pipeline
//
// Mirrors shapeVertex/shapeFragment in shaders/metal/widgets.metal — a
// signed-distance-field renderer for circle, oval, and rounded-rect shapes.
// No vertex buffers — geometry is generated from SV_VertexID against the
// bounding rect carried in ShapeUniforms.
//
// Bindings: ShapeUniforms — register(b0), vertex stage.
// ===========================================================================

cbuffer ShapeUniforms : register(b0)
{
    float4 rect;      // x, y, w, h  — bounding box (pixels)
    float4 color;     // r, g, b, a  — straight alpha
    float2 viewport;  // framebuffer w, h (pixels)
    float  corner_r;  // corner radius  (rrect);  0 for circle / oval
    float  stroke_w;  // stroke width;  0 = fill
    float  kind;      // 0 = rrect,  1 = circle / oval (ellipse SDF)
    float3 _pad;
};

static const float2 kQuadCorners[6] = {
    float2(0.0, 0.0), float2(1.0, 0.0), float2(0.0, 1.0),
    float2(0.0, 1.0), float2(1.0, 0.0), float2(1.0, 1.0),
};

struct ShapeVertOut
{
    float4 pos       : SV_POSITION;
    float4 color     : COLOR0;
    float4 rect_data : TEXCOORD0;
    float  corner_r  : TEXCOORD1;
    float  stroke_w  : TEXCOORD2;
    float  kind      : TEXCOORD3;
};

ShapeVertOut ShapeVS(uint vid : SV_VertexID)
{
    float2 t  = kQuadCorners[vid];
    float2 px = float2(rect.x + t.x * rect.z, rect.y + t.y * rect.w);
    float2 ndc = (px / viewport) * 2.0 - 1.0;
    ndc.y = -ndc.y;

    ShapeVertOut o;
    o.pos       = float4(ndc, 0.0, 1.0);
    o.color     = color;
    o.rect_data = rect;
    o.corner_r  = corner_r;
    o.stroke_w  = stroke_w;
    o.kind      = kind;
    return o;
}

float4 ShapePS(ShapeVertOut input) : SV_TARGET
{
    // Fragment position (input.pos.xy) is in framebuffer pixel coords with a
    // 0.5-center offset — same convention as Metal's [[position]].
    float2 center = float2(input.rect_data.x + input.rect_data.z * 0.5,
                           input.rect_data.y + input.rect_data.w * 0.5);
    float2 p  = input.pos.xy - center;
    float2 hs = float2(input.rect_data.z, input.rect_data.w) * 0.5;

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

    const float aa = 0.5;
    float alpha;
    if (input.stroke_w <= 0.0)
        alpha = 1.0 - smoothstep(-aa, aa, d);
    else
        alpha = 1.0 - smoothstep(-aa, aa, abs(d) - input.stroke_w * 0.5);

    float4 col = input.color;
    col.a *= alpha;
    return float4(col.rgb * col.a, col.a);   // premultiplied output
}
