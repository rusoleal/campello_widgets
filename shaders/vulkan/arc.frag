#version 450

// See arc.vert's doc comment for why this is a dedicated pipeline rather
// than an addition to rrect.frag. SDF technique: radial distance to the
// outer circle/ellipse boundary (identical formula to rrect.frag's own
// roundedBox() degenerating to a circle) combined via max() (SDF
// intersection) with an angular-wedge SDF built from two half-plane tests —
// one per cut edge at start_angle and start_angle+sweep_angle — matching
// Skia's own GPU arc renderer (GrOvalOpFactory.cpp's CircleOp) rather than
// an atan2-based angle-range compare.
//
// Angular-cut derivation:
//   mid        = start_angle + sweep_angle/2      (bisector, sign-independent)
//   half_angle = |sweep_angle|/2
// For half_angle <= PI/2 the wedge is exactly the intersection (max) of two
// half-plane SDFs, each the signed perpendicular distance to the boundary
// line at theta1=mid-half_angle / theta2=mid+half_angle, oriented so the
// wedge interior is negative. For half_angle > PI/2 (e.g. a 270 degree
// sweep) that same two-half-plane formula isn't valid on its own — a wedge
// wider than a straight line can't be the AND of two half-planes — so
// instead compute the SDF of the smaller COMPLEMENTARY wedge (width =
// 2*PI - 2*half_angle < PI, always in the valid domain) using the identical
// formula, then negate: the complement's boundary is the exact same two
// rays, so its signed distance is exactly the negation of the wedge's own,
// not an approximation. Verified correct for both the <=PI/2 and >PI/2
// cases via a real rendered pixel comparison against the Metal backend
// (which uses this identical formula) before porting here.
//
// Stroke convention: an INSET ring (band from the outer boundary inward by
// stroke_w), matching this file's own rrect.frag ring technique (and the
// arc's pre-existing CPU-tessellated geometry) — not a centered stroke.

layout(push_constant) uniform ArcUniforms {
    vec4  rect;
    vec4  color;
    vec2  viewport;
    float stroke_w;
    float start_angle;
    float sweep_angle;
} u;

layout(location = 0) in  vec2 v_pos;
layout(location = 0) out vec4 out_color;

const float kPi = 3.14159265358979323846;

// Signed distance to a wedge (bisector `mid`, half-angle `half_angle`, both
// radians) via the intersection of two half-plane tests — only valid for
// half_angle <= PI/2 on its own; see the wide-sweep handling in main().
float sdWedgeHalfPlanes(vec2 p, float mid, float half_angle)
{
    float theta1 = mid - half_angle;
    float theta2 = mid + half_angle;
    vec2  n1 = vec2(-sin(theta1),  cos(theta1)); // points into the wedge from theta1
    vec2  n2 = vec2( sin(theta2), -cos(theta2)); // points into the wedge from theta2
    float d1 = -dot(p, n1);
    float d2 = -dot(p, n2);
    return max(d1, d2);
}

void main()
{
    vec2 center = vec2(u.rect.x + u.rect.z * 0.5, u.rect.y + u.rect.w * 0.5);
    vec2 p      = v_pos - center;
    vec2 hs     = vec2(u.rect.z, u.rect.w) * 0.5;

    // Radial SDF — identical to rrect.frag's roundedBox() degenerating to
    // an exact circle/ellipse (r == min(hs.x, hs.y) case), written directly
    // as the ellipse SDF since arcs are always ellipse-shaped.
    vec2  s = p / hs;
    float d_radial_fill = (length(s) - 1.0) * min(hs.x, hs.y);

    // Inset ring — see this shader's doc comment above.
    float d_radial = (u.stroke_w <= 0.0)
        ? d_radial_fill
        : max(d_radial_fill, -(d_radial_fill + u.stroke_w));

    // Angular SDF.
    float mid        = u.start_angle + u.sweep_angle * 0.5;
    float half_angle = abs(u.sweep_angle) * 0.5;

    float d_angle;
    if (half_angle <= kPi * 0.5) {
        d_angle = sdWedgeHalfPlanes(p, mid, half_angle);
    } else {
        float comp_half = kPi - half_angle;
        float comp_mid  = mid + kPi;
        d_angle = -sdWedgeHalfPlanes(p, comp_mid, comp_half);
    }

    float d = max(d_radial, d_angle);

    const float aa    = 0.5;
    float       alpha = 1.0 - smoothstep(-aa, aa, d);

    // Same "discard zero-coverage fragments" reasoning as rrect.frag — see
    // its doc comment for why this matters for non-srcOver blend modes.
    if (alpha <= 0.0) discard;

    vec4 c    = u.color;
    out_color = vec4(c.rgb * c.a, c.a) * alpha;
}
