// ===========================================================================
// campello_widgets DirectX 12 shaders — arc pipeline
//
// Antialiased circular/elliptical arc via SDF, mirroring arcVertex/
// arcFragment in shaders/metal/widgets.metal (verified there via a real
// pixel golden-image test) and arc.vert/arc.frag in shaders/vulkan/
// (verified to compile via glslangValidator) before being ported here.
//
// A dedicated pipeline, not an addition to shape_pipeline_ (ShapeVS/ShapePS
// in shape.hlsl): that pipeline is used by every drawCircle/drawOval/
// drawRRect call in the whole app, so giving it an optional angular cut
// would mean every one of those call sites needs a new "no clip" sentinel
// to avoid ever accidentally clipping a shape that was never meant to have
// one. Isolating the new, less-proven arc math to its own pipeline means
// zero risk to that existing, working, well-tested path.
//
// Angular-cut derivation (half-plane distance to the two boundary rays
// through the ellipse's own center, each treated as an infinite line):
//   mid        = start_angle + sweep_angle/2      (bisector, sign-independent)
//   half_angle = |sweep_angle|/2
// For half_angle <= PI/2 the wedge is exactly the intersection (max) of two
// half-plane SDFs, each the signed perpendicular distance to the boundary
// line at theta1=mid-half_angle / theta2=mid+half_angle, oriented so the
// wedge interior is negative. For half_angle > PI/2 (e.g. a 270 degree
// sweep) that same two-half-plane formula isn't valid on its own -- a wedge
// wider than a straight line can't be the AND of two half-planes -- so
// instead compute the SDF of the smaller COMPLEMENTARY wedge (width =
// 2*PI - 2*half_angle < PI, always in the valid domain) using the identical
// formula, then negate: the complement's boundary is the exact same two
// rays, so its signed distance is exactly the negation of the wedge's own,
// not an approximation.
//
// Stroke convention: an INSET ring (band from the outer boundary inward by
// stroke_w), matching this backend's own rrect-equivalent... actually this
// backend's shape.hlsl uses a CENTERED stroke (abs(d) - stroke_w*0.5) for
// its shared shape pipeline -- deliberately NOT mirrored here. This arc
// pipeline uses inset instead, matching the arc's pre-existing
// CPU-tessellated geometry (and Vulkan's rrect.frag/arc.frag convention),
// since this pass is scoped to adding antialiasing only, not moving the
// stroke from where it's always been.
//
// Bindings: ArcUniforms — register(b0), vertex stage only (matching
// shape_bgl_'s visibility) -- every field is therefore forwarded to the
// pixel shader via ArcVertOut, mirroring ShapeVS/ShapePS's own pattern in
// shape.hlsl exactly (not Vulkan's arc.frag, which reads push constants
// directly in both stages -- a Vulkan-specific mechanism that doesn't apply
// here).
// ===========================================================================

cbuffer ArcUniforms : register(b0)
{
    float4 rect;        // x, y, w, h — bounding box (pixels)
    float4 color;       // r, g, b, a — straight alpha
    float2 viewport;    // framebuffer w, h (pixels)
    float  stroke_w;    // 0 = solid wedge fill to center; >0 = arc-band stroke width
    float  start_angle; // radians, 0 = 3 o'clock, screen space (y-down)
    float  sweep_angle; // radians, positive = clockwise on screen
    float3 _pad;
};

static const float2 kQuadCorners[6] = {
    float2(0.0, 0.0), float2(1.0, 0.0), float2(0.0, 1.0),
    float2(0.0, 1.0), float2(1.0, 0.0), float2(1.0, 1.0),
};

struct ArcVertOut
{
    float4 pos         : SV_POSITION;
    float4 color       : COLOR0;
    float4 rect_data   : TEXCOORD0;
    float  stroke_w    : TEXCOORD1;
    float  start_angle : TEXCOORD2;
    float  sweep_angle : TEXCOORD3;
};

ArcVertOut ArcVS(uint vid : SV_VertexID)
{
    // Identical inflation rule to ShapeVS's (shape.hlsl) — matched exactly
    // rather than independently re-derived.
    const float aa = 0.5;
    float inflate = (stroke_w > 0.0) ? (stroke_w * 0.5 + aa) : 0.0;

    float2 t      = kQuadCorners[vid];
    float2 origin = rect.xy - inflate;
    float2 size   = rect.zw + inflate * 2.0;
    float2 px     = origin + t * size;
    float2 ndc    = (px / viewport) * 2.0 - 1.0;
    ndc.y = -ndc.y;

    ArcVertOut o;
    o.pos         = float4(ndc, 0.0, 1.0);
    o.color       = color;
    o.rect_data   = rect;
    o.stroke_w    = stroke_w;
    o.start_angle = start_angle;
    o.sweep_angle = sweep_angle;
    return o;
}

// Signed distance to a wedge (bisector `mid`, half-angle `half_angle`, both
// radians) via the intersection of two half-plane tests — only valid for
// half_angle <= PI/2 on its own; see ArcPS's wide-sweep handling below.
float sdWedgeHalfPlanes(float2 p, float mid, float half_angle)
{
    float theta1 = mid - half_angle;
    float theta2 = mid + half_angle;
    float2 n1 = float2(-sin(theta1),  cos(theta1)); // points into the wedge from theta1
    float2 n2 = float2( sin(theta2), -cos(theta2)); // points into the wedge from theta2
    float d1 = -dot(p, n1);
    float d2 = -dot(p, n2);
    return max(d1, d2);
}

float4 ArcPS(ArcVertOut input) : SV_TARGET
{
    float2 center = float2(input.rect_data.x + input.rect_data.z * 0.5,
                           input.rect_data.y + input.rect_data.w * 0.5);
    float2 p  = input.pos.xy - center;
    float2 hs = float2(input.rect_data.z, input.rect_data.w) * 0.5;

    // Radial SDF — identical to ShapePS's ellipse branch (shape.hlsl).
    float2 s = p / hs;
    float d_radial_fill = (length(s) - 1.0) * min(hs.x, hs.y);

    const float aa = 0.5;
    // Inset ring — see this file's header doc comment for why (deliberately
    // not ShapePS's centered-stroke convention).
    float d_radial = (input.stroke_w <= 0.0)
        ? d_radial_fill
        : max(d_radial_fill, -(d_radial_fill + input.stroke_w));

    // Angular SDF — see this file's header doc comment for the derivation.
    const float kPi = 3.14159265358979323846;
    float mid        = input.start_angle + input.sweep_angle * 0.5;
    float half_angle = abs(input.sweep_angle) * 0.5;

    float d_angle;
    if (half_angle <= kPi * 0.5)
    {
        d_angle = sdWedgeHalfPlanes(p, mid, half_angle);
    }
    else
    {
        float comp_half = kPi - half_angle;
        float comp_mid  = mid + kPi;
        d_angle = -sdWedgeHalfPlanes(p, comp_mid, comp_half);
    }

    float d = max(d_radial, d_angle);
    float alpha = 1.0 - smoothstep(-aa, aa, d);

    float4 col = input.color;
    col.a *= alpha;
    return float4(col.rgb * col.a, col.a);   // premultiplied output
}
