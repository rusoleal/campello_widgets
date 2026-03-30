#include <metal_stdlib>
using namespace metal;

// ===========================================================================
// campello_widgets Metal shaders
//
// Rect pipeline (rectVertex / rectFragment):
//   Draws a solid-colored axis-aligned quad.
//   Uniforms at [[buffer(0)]]: RectUniforms (rect, color, viewport)
//   No vertex buffers — geometry is generated from vertex_id.
//
// Quad pipeline (quadVertex / quadFragment):
//   Draws a textured axis-aligned quad (text glyphs, images).
//   Uniforms at [[buffer(0)]]: QuadUniforms (dstRect, srcRect, viewport)
//   Texture at [[texture(0)]], sampler at [[sampler(1)]]
// ===========================================================================

// Shared corner table — two CCW triangles covering the unit square.
constant float2 kQuadCorners[6] = {
    float2(0.0, 0.0), float2(1.0, 0.0), float2(0.0, 1.0),
    float2(0.0, 1.0), float2(1.0, 0.0), float2(1.0, 1.0),
};

// ---------------------------------------------------------------------------
// Rect pipeline
// ---------------------------------------------------------------------------

struct RectUniforms {
    float4 rect;      // x, y, width, height (pixels)
    float4 color;     // r, g, b, a
    float2 viewport;  // width, height (pixels)
};

struct RectVertOut {
    float4 pos   [[position]];
    float4 color;
};

vertex RectVertOut rectVertex(
    uint                  vid [[vertex_id]],
    constant RectUniforms &u  [[buffer(0)]])
{
    float2 t  = kQuadCorners[vid];
    float2 px = float2(u.rect.x + t.x * u.rect.z,
                       u.rect.y + t.y * u.rect.w);
    // pixel coords → NDC (Metal: y+ = up; screen: y+ = down → negate y)
    float2 ndc = (px / u.viewport) * 2.0 - 1.0;
    ndc.y = -ndc.y;

    RectVertOut out;
    out.pos   = float4(ndc, 0.0, 1.0);
    out.color = u.color;
    return out;
}

fragment float4 rectFragment(RectVertOut in [[stage_in]])
{
    // Premultiply alpha so this pipeline can share the same
    // src=ONE dst=ONE_MINUS_SRC_ALPHA blend equation as the quad pipeline.
    return float4(in.color.rgb * in.color.a, in.color.a);
}

// ---------------------------------------------------------------------------
// Quad (textured) pipeline
// ---------------------------------------------------------------------------

struct QuadUniforms {
    float4 dstRect;   // x, y, width, height (pixels)
    float4 srcRect;   // u0, v0, u1, v1 (normalised UV)
    float2 viewport;  // width, height (pixels)
    float  opacity;   // [0, 1] — multiplied into every pixel
    float  _pad;
};

struct QuadVertOut {
    float4 pos     [[position]];
    float2 uv;
    float  opacity; // passed from vertex uniforms
};

vertex QuadVertOut quadVertex(
    uint               vid [[vertex_id]],
    constant QuadUniforms &u [[buffer(0)]])
{
    float2 t  = kQuadCorners[vid];
    float2 px = float2(u.dstRect.x + t.x * u.dstRect.z,
                       u.dstRect.y + t.y * u.dstRect.w);
    float2 ndc = (px / u.viewport) * 2.0 - 1.0;
    ndc.y = -ndc.y;

    float2 uv = float2(u.srcRect.x + t.x * (u.srcRect.z - u.srcRect.x),
                       u.srcRect.y + t.y * (u.srcRect.w - u.srcRect.y));

    QuadVertOut out;
    out.pos     = float4(ndc, 0.0, 1.0);
    out.uv      = uv;
    out.opacity = u.opacity;
    return out;
}

fragment float4 quadFragment(
    QuadVertOut      in  [[stage_in]],
    texture2d<float> tex [[texture(0)]],
    sampler          smp [[sampler(1)]])
{
    // The texture is already premultiplied-alpha; scaling all channels by
    // opacity preserves premultiplication and produces correct blending.
    return tex.sample(smp, in.uv) * in.opacity;
}

// ---------------------------------------------------------------------------
// Shape pipeline — circle, oval, and rounded rect via signed distance field
//
// Uniforms at [[buffer(0)]]: ShapeUniforms (vertex stage only)
// All SDF parameters are forwarded to the fragment stage via flat varyings.
// ---------------------------------------------------------------------------

struct ShapeUniforms {
    float4 rect;       // x, y, w, h  — bounding box (pixels)
    float4 color;      // r, g, b, a  — straight alpha
    float2 viewport;   // framebuffer w, h (pixels)
    float  corner_r;   // corner radius  (rrect);  0 for circle / oval
    float  stroke_w;   // stroke width;  0 = fill
    float  kind;       // 0 = rrect,  1 = circle / oval (ellipse SDF)
    float  _pad0;
    float  _pad1;
    float  _pad2;
};

struct ShapeVertOut {
    float4 pos       [[position]];
    float4 color     [[flat]];    // straight alpha color
    float4 rect_data [[flat]];    // x, y, w, h bounding box (pixels)
    float  corner_r  [[flat]];
    float  stroke_w  [[flat]];
    float  kind      [[flat]];
};

vertex ShapeVertOut shapeVertex(
    uint                 vid [[vertex_id]],
    constant ShapeUniforms &u  [[buffer(0)]])
{
    float2 t  = kQuadCorners[vid];
    float2 px = float2(u.rect.x + t.x * u.rect.z,
                       u.rect.y + t.y * u.rect.w);
    float2 ndc = (px / u.viewport) * 2.0 - 1.0;
    ndc.y = -ndc.y;

    ShapeVertOut out;
    out.pos      = float4(ndc, 0.0, 1.0);
    out.color    = u.color;
    out.rect_data = u.rect;
    out.corner_r = u.corner_r;
    out.stroke_w = u.stroke_w;
    out.kind     = u.kind;
    return out;
}

fragment float4 shapeFragment(ShapeVertOut in [[stage_in]])
{
    // Fragment position (in.pos.xy) is in framebuffer pixel coords with 0.5 center offset.
    float2 center = float2(in.rect_data.x + in.rect_data.z * 0.5,
                           in.rect_data.y + in.rect_data.w * 0.5);
    float2 p  = in.pos.xy - center;
    float2 hs = float2(in.rect_data.z, in.rect_data.w) * 0.5;

    float d;
    if (in.kind < 0.5) {
        // Rounded-rect SDF
        float r  = min(in.corner_r, min(hs.x, hs.y));
        float2 q = abs(p) - hs + r;
        d = length(max(q, float2(0.0))) + min(max(q.x, q.y), 0.0) - r;
    } else {
        // Ellipse SDF  (circle when hs.x == hs.y)
        float2 s = p / hs;
        d = (length(s) - 1.0) * min(hs.x, hs.y);
    }

    const float aa = 0.5;
    float alpha;
    if (in.stroke_w <= 0.0) {
        alpha = 1.0 - smoothstep(-aa, aa, d);
    } else {
        alpha = 1.0 - smoothstep(-aa, aa, abs(d) - in.stroke_w * 0.5);
    }

    float4 col = in.color;
    col.a *= alpha;
    return float4(col.rgb * col.a, col.a);   // premultiplied output
}

// ---------------------------------------------------------------------------
// Line pipeline — arbitrary-angle line segment rendered as a rotated quad
//
// Uniforms at [[buffer(0)]]: LineUniforms
// No vertex buffers — 6 vertices generated from vertex_id.
// ---------------------------------------------------------------------------

struct LineUniforms {
    float4 p1;        // xy: start (pixels),  zw: unused
    float4 p2;        // xy: end   (pixels),  zw: unused
    float4 color;     // r, g, b, a
    float2 viewport;  // framebuffer w, h
    float  stroke_w;  // line thickness (pixels)
    float  _pad;
};

vertex RectVertOut lineVertex(
    uint               vid [[vertex_id]],
    constant LineUniforms &u [[buffer(0)]])
{
    float2 dir = u.p2.xy - u.p1.xy;
    float  len = length(dir);
    if (len > 0.0001) dir /= len; else dir = float2(1.0, 0.0);
    float2 perp = float2(-dir.y, dir.x) * (u.stroke_w * 0.5);

    // 4 corners: [p1-perp, p1+perp, p2+perp, p2-perp]
    uint   idx[6]     = {0, 1, 3, 1, 2, 3};
    float2 corners[4] = {
        u.p1.xy - perp,
        u.p1.xy + perp,
        u.p2.xy + perp,
        u.p2.xy - perp,
    };

    float2 px  = corners[idx[vid]];
    float2 ndc = (px / u.viewport) * 2.0 - 1.0;
    ndc.y = -ndc.y;

    RectVertOut out;
    out.pos   = float4(ndc, 0.0, 1.0);
    out.color = u.color;
    return out;
}
// lineFragment reuses rectFragment — same premultiplied solid-colour output.


// ---------------------------------------------------------------------------
// Blur pipeline — separable Gaussian blur (horizontal or vertical pass)
//
// Uniforms at [[buffer(0)]]: BlurUniforms
// Texture at [[texture(0)]], sampler at [[sampler(1)]]
//
// Two-pass separable Gaussian blur:
//   Pass H:  u.horizontal = 1.0, source → blur_h
//   Pass V:  u.horizontal = 0.0, blur_h  → blurred
// ---------------------------------------------------------------------------

struct BlurUniforms {
    float4 dstRect;      // x, y, w, h (pixels, destination quad)
    float4 srcRect;      // u0, v0, u1, v1 (normalised UV of source region)
    float2 viewport;     // framebuffer width, height
    float  sigma;        // Gaussian sigma (pixels)
    float  horizontal;   // 1.0 = horizontal pass, 0.0 = vertical pass
    float2 tex_size;     // source texture width, height (pixels)
    float2 _pad;
};

struct BlurVertOut {
    float4 pos [[position]];
    float2 uv;
};

vertex BlurVertOut blurVertex(
    uint             vid [[vertex_id]],
    constant BlurUniforms &u [[buffer(0)]])
{
    float2 t  = kQuadCorners[vid];
    float2 px = float2(u.dstRect.x + t.x * u.dstRect.z,
                       u.dstRect.y + t.y * u.dstRect.w);
    float2 ndc = (px / u.viewport) * 2.0 - 1.0;
    ndc.y = -ndc.y;

    float2 uv = float2(u.srcRect.x + t.x * (u.srcRect.z - u.srcRect.x),
                       u.srcRect.y + t.y * (u.srcRect.w - u.srcRect.y));

    BlurVertOut out;
    out.pos = float4(ndc, 0.0, 1.0);
    out.uv  = uv;
    return out;
}

fragment float4 blurFragment(
    BlurVertOut            in  [[stage_in]],
    texture2d<float>       tex [[texture(0)]],
    sampler                smp [[sampler(1)]],
    constant BlurUniforms &u   [[buffer(0)]])
{
    const float sigma = max(u.sigma, 0.5);
    const int   RADIUS = min(int(ceil(2.5 * sigma)), 12);

    float2 step = (u.horizontal > 0.5)
        ? float2(1.0 / u.tex_size.x, 0.0)
        : float2(0.0, 1.0 / u.tex_size.y);

    float4 color = float4(0.0);
    float  wsum  = 0.0;

    for (int i = -RADIUS; i <= RADIUS; ++i)
    {
        float fi     = float(i);
        float weight = exp(-0.5 * (fi / sigma) * (fi / sigma));
        float2 suv   = clamp(in.uv + fi * step, float2(0.0), float2(1.0));
        color += tex.sample(smp, suv) * weight;
        wsum  += weight;
    }

    return color / wsum;
}

// ---------------------------------------------------------------------------
// ShaderMask pipeline — composites child texture with a gradient mask
//
// Uniforms at [[buffer(0)]]: ShaderMaskUniforms
// child texture  at [[texture(0)]]
// gradient LUT   at [[texture(1)]]  (256 × 1 BGRA)
// sampler        at [[sampler(2)]]
// ---------------------------------------------------------------------------

struct ShaderMaskUniforms {
    float4 dstRect;        // bounds in viewport pixels (x,y,w,h)
    float2 viewport;       // framebuffer width, height
    float  gradient_type;  // 0 = linear, 1 = radial
    float  _pad0;
    float4 gradient_p1;    // linear: begin.xy; radial: center.xy (pixels)
    float4 gradient_p2;    // linear: end.xy;   radial: radius in .x (pixels)
    float  blend_mode;     // 0 = srcIn (child * mask.a), 1 = modulate (* mask.rgb)
    float3 _pad1;
};

struct ShaderMaskVertOut {
    float4 pos     [[position]];
    float2 uv;
};

vertex ShaderMaskVertOut shaderMaskVertex(
    uint                       vid [[vertex_id]],
    constant ShaderMaskUniforms &u  [[buffer(0)]])
{
    float2 t  = kQuadCorners[vid];
    float2 px = float2(u.dstRect.x + t.x * u.dstRect.z,
                       u.dstRect.y + t.y * u.dstRect.w);
    float2 ndc = (px / u.viewport) * 2.0 - 1.0;
    ndc.y = -ndc.y;

    ShaderMaskVertOut out;
    out.pos = float4(ndc, 0.0, 1.0);
    out.uv  = t;   // [0,1] across the child texture
    return out;
}

fragment float4 shaderMaskFragment(
    ShaderMaskVertOut          in    [[stage_in]],
    texture2d<float>           child [[texture(0)]],
    texture2d<float>           lut   [[texture(1)]],
    sampler                    smp   [[sampler(2)]],
    constant ShaderMaskUniforms &u   [[buffer(0)]])
{
    float4 child_color = child.sample(smp, in.uv);

    // Reconstruct fragment position in viewport pixels.
    float2 pos = in.pos.xy;

    // Compute gradient parameter t.
    float t;
    if (u.gradient_type < 0.5) {
        // Linear gradient
        float2 p1  = u.gradient_p1.xy;
        float2 p2  = u.gradient_p2.xy;
        float2 dir = p2 - p1;
        float  len2 = dot(dir, dir);
        t = (len2 > 0.0001) ? dot(pos - p1, dir) / len2 : 0.0;
    } else {
        // Radial gradient
        float2 center = u.gradient_p1.xy;
        float  radius = u.gradient_p2.x;
        t = (radius > 0.0001) ? length(pos - center) / radius : 0.0;
    }
    t = clamp(t, 0.0, 1.0);

    float4 mask_color = lut.sample(smp, float2(t, 0.5));

    // Apply blend mode.
    if (u.blend_mode < 0.5) {
        // srcIn: output = child * mask.a
        return child_color * mask_color.a;
    } else {
        // modulate: output = child * mask
        return child_color * mask_color;
    }
}
