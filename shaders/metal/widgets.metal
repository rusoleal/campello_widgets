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
