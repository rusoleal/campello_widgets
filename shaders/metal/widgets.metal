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
//   Draws a textured quad (text glyphs, images) — a REAL quad, not
//   necessarily axis-aligned: each of its 4 corners carries its own
//   independently ambient-transform-projected position, so a rotated or
//   perspective-projected Transform renders as a genuinely tilted/
//   trapezoidal shape instead of a resized axis-aligned box. See
//   QuadVertexIn's doc comment below.
//   Vertex data at buffer(0): QuadVertexIn (position+w, uv), 6 per quad.
//   Uniforms at [[buffer(1)]]: QuadUniforms (viewport, opacity)
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

// RectVertexIn.posw carries this corner's real (x, y, w) — see
// QuadVertexIn's doc comment above for the full rationale (same mechanism,
// applied to the solid-color rect pipeline).
struct RectVertexIn {
    float3 posw [[attribute(0)]];  // (x, y, w) in pixel space, pre-clip-space
};

struct RectUniforms {
    float4 color;     // r, g, b, a
    float2 viewport;  // width, height (pixels)
};

struct RectVertOut {
    float4 pos   [[position]];
    float4 color;
};

vertex RectVertOut rectVertex(
    RectVertexIn           in [[stage_in]],
    constant RectUniforms &u  [[buffer(1)]])
{
    // See quadVertex's doc comment for the full derivation — same formula,
    // applied here to a solid-color rect instead of a textured one.
    float clip_x =  2.0 * in.posw.x / u.viewport.x - in.posw.z;
    float clip_y = -(2.0 * in.posw.y / u.viewport.y - in.posw.z);

    RectVertOut out;
    out.pos   = float4(clip_x, clip_y, 0.0, in.posw.z);
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

// QuadVertexIn.posw is the ambient-matrix-transformed pixel position of
// this corner, carrying its real `w` — computed on the CPU side (see
// drawTexturedQuad() in metal_draw_backend.mm) by transforming all four
// corners of the destination rect *independently*, instead of collapsing
// them to an axis-aligned bounding box (dstRect) the way this shader used
// to. This is what makes a rotated/perspective-projected quad actually
// render as a tilted/trapezoidal shape: real per-vertex positions, not a
// single origin + width/height reinterpolated by a hardcoded corner table.
struct QuadVertexIn {
    float3 posw [[attribute(0)]];  // (x, y, w) in pixel space, pre-clip-space
    float2 uv   [[attribute(1)]];
};

struct QuadUniforms {
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
    QuadVertexIn           in [[stage_in]],
    constant QuadUniforms &u  [[buffer(1)]])
{
    // Convert the raw (x, y, w) into clip space so the GPU's hardware
    // perspective divide (clip.xy / clip.w, done automatically during
    // rasterization — NOT something this shader does manually) reproduces
    // this renderer's existing pixel->NDC mapping exactly when w == 1 (the
    // overwhelmingly common case: no perspective term set on the ambient
    // transform), and genuinely foreshortens when w != 1.
    //
    // Derivation: want clip.xy/clip.w == (pos.xy/pos.w)/viewport*2-1 (with
    // y flipped for screen-down vs Metal's y-up NDC). Setting clip.w =
    // pos.w and solving gives clip.xy = 2*pos.xy/viewport -/+ pos.w.
    //
    // Letting the hardware do this divide (rather than dividing manually
    // here and outputting w=1) matters beyond just corner placement: the
    // rasterizer only perspective-correctly interpolates `uv` across the
    // triangle when the *clip-space* w is non-1 going into it. A manual
    // divide would place corners correctly but linearly (screen-space)
    // interpolate the texture across a foreshortened quad — the classic
    // "affine texture warp" artifact — instead of correctly.
    float clip_x =  2.0 * in.posw.x / u.viewport.x - in.posw.z;
    float clip_y = -(2.0 * in.posw.y / u.viewport.y - in.posw.z);

    QuadVertOut out;
    out.pos     = float4(clip_x, clip_y, 0.0, in.posw.z);
    out.uv      = in.uv;
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
// Icon pipeline — tinted template image
//
// Draws a "template" texture (an icon's shape, encoded in its alpha
// channel — the source RGB is ignored entirely) recolored to an arbitrary
// runtime tint, the same mechanism as iOS's
// UIImage.withRenderingMode(.alwaysTemplate) and Android's icon tinting.
// This lets a single monochrome asset per icon serve any theme color
// instead of needing a pre-baked PNG per tint. Reuses QuadVertexIn's
// vertex layout (position+w, uv) so drawTintedTexturedQuad() can build
// geometry identically to drawTexturedQuad() — only the uniforms/pipeline
// differ.
// ---------------------------------------------------------------------------

struct IconUniforms {
    float2 viewport;  // width, height (pixels)
    float  opacity;   // [0, 1] — multiplied into every pixel
    float  _pad;
    float4 tint;      // straight-alpha RGBA recolor
};

struct IconVertOut {
    float4 pos     [[position]];
    float2 uv;
    float  opacity;
    float4 tint;
};

vertex IconVertOut iconVertex(
    QuadVertexIn           in [[stage_in]],
    constant IconUniforms &u  [[buffer(1)]])
{
    // Same clip-space derivation as quadVertex() above.
    float clip_x =  2.0 * in.posw.x / u.viewport.x - in.posw.z;
    float clip_y = -(2.0 * in.posw.y / u.viewport.y - in.posw.z);

    IconVertOut out;
    out.pos     = float4(clip_x, clip_y, 0.0, in.posw.z);
    out.uv      = in.uv;
    out.opacity = u.opacity;
    out.tint    = u.tint;
    return out;
}

fragment float4 iconFragment(
    IconVertOut      in  [[stage_in]],
    texture2d<float> tex [[texture(0)]],
    sampler          smp [[sampler(1)]])
{
    // Ignore the source texture's own RGB; use only its alpha as a
    // stencil for the tint color. Output premultiplied, matching every
    // other pipeline's blend state (src=ONE dst=ONE_MINUS_SRC_ALPHA).
    float a = tex.sample(smp, in.uv).a * in.tint.a * in.opacity;
    return float4(in.tint.rgb * a, a);
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
    // A centered stroke extends stroke_w*0.5 outward from the logical rect
    // boundary (plus shapeFragment()'s antialiasing band, aa), but the quad
    // built from kQuadCorners below only ever spans the rect itself. Without
    // inflating it here, the rasterizer never produces fragments for the
    // outward half of the stroke at all -- it isn't clipped away by
    // anything, it's simply outside every triangle this draw call submits.
    // That reads as the stroke rendering fine on whichever side happens to
    // fall inside the rect and vanishing on the side that falls outside it
    // (e.g. a field's bottom border thinning to nothing while the top
    // renders correctly, purely depending on which way the sub-pixel
    // rounding falls).
    const float aa = 0.5;
    float inflate = (u.stroke_w > 0.0) ? (u.stroke_w * 0.5 + aa) : 0.0;

    float2 t      = kQuadCorners[vid];
    float2 origin = u.rect.xy - inflate;
    float2 size   = u.rect.zw + inflate * 2.0;
    float2 px     = origin + t * size;

    float2 ndc = (px / u.viewport) * 2.0 - 1.0;
    ndc.y = -ndc.y;

    ShapeVertOut out;
    out.pos       = float4(ndc, 0.0, 1.0);
    out.color     = u.color;
    out.rect_data = u.rect; // unchanged -- shapeFragment()'s SDF still
                             // measures distance from the true logical rect,
                             // only the rasterized quad extent grew.
    out.corner_r  = u.corner_r;
    out.stroke_w  = u.stroke_w;
    out.kind      = u.kind;
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
    float4 pos         [[position]];
    float2 uv;
    float  sigma       [[flat]];
    float  horizontal  [[flat]];
    float2 tex_size    [[flat]];
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
    out.pos        = float4(ndc, 0.0, 1.0);
    out.uv         = uv;
    out.sigma      = u.sigma;
    out.horizontal = u.horizontal;
    out.tex_size   = u.tex_size;
    return out;
}

// Reads only [[stage_in]] — no separate uniform buffer binding, mirroring
// shapeFragment() above. runBlurPass() only binds a vertex buffer
// (setVertexBuffer), not a fragment buffer, so a fragment-stage
// `constant ... [[buffer(0)]]` parameter here would read unbound/garbage
// memory instead of the uniforms actually set for this draw.
fragment float4 blurFragment(
    BlurVertOut       in  [[stage_in]],
    texture2d<float>  tex [[texture(0)]],
    sampler           smp [[sampler(1)]])
{
    const float sigma = max(in.sigma, 0.5);
    // Capped at 48 (not the ideal 2.5*sigma) to bound the per-pixel sample
    // count — real iOS Liquid Glass backdrops need a much stronger blur
    // than this shader could reach at the old cap of 24 (itself already
    // bumped once from 12): a real iPadOS 26 navigationRail capture shows
    // an almost featureless wash, while sigma=16-60 all looked identical
    // at RADIUS=24 (2.5*sigma exceeds 24 once sigma > 9.6, so every one
    // of those requests was silently clamped to the same too-sharp
    // result — the actual bug, not the sigma value passed in).
    const int   RADIUS = min(int(ceil(2.5 * sigma)), 96);

    float2 step = (in.horizontal > 0.5)
        ? float2(1.0 / in.tex_size.x, 0.0)
        : float2(0.0, 1.0 / in.tex_size.y);

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
// Liquid Glass pipeline — refracted/tinted/specular composite of the
// pre-blurred backdrop, shaped to a rounded rect via a signed distance
// field (SDF).
//
// The SDF is the same formula as shapeFragment()'s rounded-rect branch
// above (Inigo Quilez's sdRoundedBox), factored out into sdRoundedBox()
// below so it can be evaluated multiple times per pixel — once at the
// pixel itself, twice more offset by an epsilon on each axis, to get a
// central-difference gradient. That gradient is a per-pixel fake surface
// normal: near the shape's interior the SDF is flat (gradient ~0 after
// the rim falloff below zeroes it out), near the rim it points radially
// outward/away from the nearest edge or corner — the corner case falls
// out of the SDF math for free, no special-casing needed. This same
// gradient drives both the refraction (offsets the backdrop-texture UV,
// bending what's behind) and the specular highlight (a Blinn-Phong-style
// term against a fixed light direction) — it's the one piece of "local
// geometry" the whole effect is built on. See shaders/metal/widgets.metal's
// git history / TODO.md for the fuller derivation.
//
// Local geometry is evaluated in the widget's own *local* 0..1 space
// (LiquidGlassVertexIn.local_uv, a plain quad parametrization generated
// on the CPU side — see drawBackdropFilter()'s liquidGlass branch in
// metal_draw_backend.mm) rather than from screen-space fragment position,
// so the shape stays correct under a rotated/perspective ambient
// transform — the same reasoning QuadVertexIn's doc comment above gives
// for carrying real per-corner positions instead of a single dstRect.
//
// Uniforms at [[buffer(1)]]: LiquidGlassUniforms (both vertex and fragment)
// Pre-blurred backdrop texture at [[texture(0)]], sampler at [[sampler(1)]]
// ---------------------------------------------------------------------------

inline float sdRoundedBox(float2 p, float2 half_size, float radius)
{
    float r  = min(radius, min(half_size.x, half_size.y));
    float2 q = abs(p) - half_size + r;
    return length(max(q, float2(0.0))) + min(max(q.x, q.y), 0.0) - r;
}

struct LiquidGlassVertexIn {
    float3 posw     [[attribute(0)]];  // (x, y, w) in pixel space, pre-clip-space
    float2 uv       [[attribute(1)]];  // backdrop-texture sample UV
    float2 local_uv [[attribute(2)]];  // 0..1 across the widget's own rect
};

struct LiquidGlassUniforms {
    float2 viewport;            // framebuffer w, h (pixels)
    float2 size;                // widget's own logical w, h (pixels)
    float4 tint;                // r, g, b, a — straight alpha
    float  corner_radius;       // pixels, in local space
    float  refraction_strength; // pixels of max UV offset at the rim
    float  specular_intensity;  // [0, 1]
    float  _pad;
};

// LiquidGlassUniforms is vertex-stage-only — its fields are forwarded to
// the fragment stage as [[flat]] varyings below, not via a second buffer
// binding. Mirrors shapeFragment()/ShapeVertOut above (see blurFragment's
// doc comment for why: this engine's RenderPassEncoder only exposes
// setVertexBuffer(), no separate fragment-buffer binding call).
struct LiquidGlassVertOut {
    float4 pos                  [[position]];
    float2 uv;
    float2 local_uv;
    float2 viewport             [[flat]];
    float2 size                 [[flat]];
    float4 tint                 [[flat]];
    float  corner_radius        [[flat]];
    float  refraction_strength  [[flat]];
    float  specular_intensity   [[flat]];
};

vertex LiquidGlassVertOut liquidGlassVertex(
    LiquidGlassVertexIn           in [[stage_in]],
    constant LiquidGlassUniforms &u  [[buffer(1)]])
{
    // Same clip-space derivation as quadVertex()/rectVertex() above.
    float clip_x =  2.0 * in.posw.x / u.viewport.x - in.posw.z;
    float clip_y = -(2.0 * in.posw.y / u.viewport.y - in.posw.z);

    LiquidGlassVertOut out;
    out.pos                 = float4(clip_x, clip_y, 0.0, in.posw.z);
    out.uv                  = in.uv;
    out.local_uv            = in.local_uv;
    out.viewport            = u.viewport;
    out.size                = u.size;
    out.tint                = u.tint;
    out.corner_radius       = u.corner_radius;
    out.refraction_strength = u.refraction_strength;
    out.specular_intensity  = u.specular_intensity;
    return out;
}

fragment float4 liquidGlassFragment(
    LiquidGlassVertOut in  [[stage_in]],
    texture2d<float>   tex [[texture(0)]],
    sampler             smp [[sampler(1)]])
{
    float2 hs = in.size * 0.5;
    float2 p  = (in.local_uv - 0.5) * in.size;

    float d = sdRoundedBox(p, hs, in.corner_radius);

    const float eps = 1.0;
    float dpx = sdRoundedBox(p + float2(eps, 0.0), hs, in.corner_radius)
              - sdRoundedBox(p - float2(eps, 0.0), hs, in.corner_radius);
    float dpy = sdRoundedBox(p + float2(0.0, eps), hs, in.corner_radius)
              - sdRoundedBox(p - float2(0.0, eps), hs, in.corner_radius);
    float2 normal = float2(dpx, dpy);
    if (length(normal) > 0.0001) normal = normalize(normal);

    // Real glass bends light in a narrow bevel right at the edge and
    // stays essentially undistorted in the interior — unlike v1, which
    // used one fixed 18px band regardless of shape size (fine for a big
    // demo card, but would swallow a small chip/button's entire
    // interior). Scale both bands off corner_radius instead, clamped to a
    // sane range: refractBand is the wider lensing bevel, rimBand is a
    // much thinner band for the bright edge glint below — real glass
    // visibly bends light across several pixels but only *catches* a
    // crisp highlight in a much thinner strip right at the boundary.
    float refractBand = clamp(in.corner_radius * 0.35, 3.0,  8.0);
    float rimBand      = clamp(in.corner_radius * 0.08, 0.75, 2.0);

    float refract_amt = 1.0 - smoothstep(0.0, refractBand, -d);
    // Peaks at the true edge (d ~ 0), falls off to *both* sides — inward
    // into the refraction band and outward past the visible silhouette
    // (masked by alpha there regardless).
    float rim_amt = 1.0 - smoothstep(0.0, rimBand, abs(d));

    // Chromatic dispersion: real glass bends different wavelengths by
    // slightly different amounts (a prism) — a subtle color fringe right
    // at the refracting edge that reads as unmistakably *glass*, which
    // plain Gaussian blur (or a single-sample refraction) can never
    // produce on its own. Kept small (a couple pixels at most) so it
    // reads as a glassy edge shimmer, not a lens-flare gimmick.
    const float kChromaticAberration = 2.75; // px, at full refraction
    float2 base_offset = normal * (in.refraction_strength * refract_amt) / in.viewport;
    float2 ca_offset   = normal * (kChromaticAberration    * refract_amt) / in.viewport;

    // No discard_fragment() here — deliberately; see the doc comment
    // above liquidGlassFragment's declaration site in this file's git
    // history / TODO.md: combining a divergent discard with a later
    // implicit-derivative tex.sample() is a well-known GPU hazard that
    // produced visible per-frame corruption the first time this was
    // tried. Outside the shape reads transparent purely via `alpha`
    // below. Explicit UV clamping on every sample below is additional,
    // independent hardening against sampling outside the valid
    // captured-backdrop region — matches blurFragment()'s existing clamp.
    float2 uv_r = clamp(in.uv + base_offset + ca_offset, float2(0.0), float2(1.0));
    float2 uv_g = clamp(in.uv + base_offset,             float2(0.0), float2(1.0));
    float2 uv_b = clamp(in.uv + base_offset - ca_offset, float2(0.0), float2(1.0));

    float4 backdrop;
    backdrop.r = tex.sample(smp, uv_r).r;
    backdrop.g = tex.sample(smp, uv_g).g;
    backdrop.b = tex.sample(smp, uv_b).b;
    backdrop.a = 1.0;

    // Saturation boost — cheap luma-based mix (the second ingredient of
    // the effect, alongside blur and the tint below). Pushed past v1's
    // 1.35 (which had been dialed back to 1.25 when chromatic aberration
    // was added) — the interior of a large card reads as near-identical
    // to plain frosted blur without a genuinely vivid boost, since
    // refraction/chromatic aberration/specular only touch a thin rim.
    float  luma      = dot(backdrop.rgb, float3(0.299, 0.587, 0.114));
    float3 saturated = mix(float3(luma), backdrop.rgb, 1.5);

    // Tinted overlay — real Liquid Glass stays close to neutral/clear
    // rather than heavily colored; the material reads mostly through
    // blur + saturation + refraction, with tint as a light finishing wash.
    float3 tinted = mix(saturated, in.tint.rgb, in.tint.a);

    // Rim highlight: a thin bright line around the *whole* edge, not a
    // hard directional spot that goes fully dark on the far side from
    // the light — real glass catches ambient light all along its
    // boundary, brighter toward one side, never fully unlit on the
    // other. Blended toward white (not simply added) so it stays a
    // crisp line instead of blowing out into an over-bright glow. Real
    // Liquid Glass reacts to device motion/content; a fixed light
    // direction is a deliberate v1 simplification — see TODO.md.
    const float2 light_dir  = normalize(float2(-0.35, -1.0));
    float        light_bias = dot(normal, -light_dir) * 0.5 + 0.5; // remapped to [0,1], never 0
    float        highlight  = pow(light_bias, 2.5) * rim_amt * in.specular_intensity;
    float3       result     = mix(tinted, float3(1.0), highlight);

    float alpha = 1.0 - smoothstep(-1.0, 1.0, d); // shape-edge antialiasing

    return float4(result * alpha, alpha); // premultiplied output
}

// ---------------------------------------------------------------------------
// ShaderMask pipeline — composites child texture with a gradient mask
//
// Real per-vertex position(+w)/uv data at buffer(0) — same QuadVertexIn
// shape as the quad/clip-shape pipelines (see drawShaderMaskComposite() for
// why: real corners, not an axis-aligned dstRect, so a rotated/mirrored
// mask's destination shape and sampled child content render correctly).
// The gradient itself (gradient_p1/p2 below) is deliberately still
// evaluated in *screen space* via the fragment shader's `in.pos.xy`,
// unchanged from before — making the gradient rotate/tilt *with* the
// widget under a non-trivial ambient transform (matching a real 3D
// texture-mapped gradient) is a separate, not-yet-addressed piece of work,
// out of scope for this pass.
//
// Uniforms at [[buffer(1)]]: ShaderMaskUniforms
// child texture  at [[texture(0)]]
// gradient LUT   at [[texture(1)]]  (256 × 1 BGRA)
// sampler        at [[sampler(2)]]
// ---------------------------------------------------------------------------

struct ShaderMaskVertexIn {
    float3 posw [[attribute(0)]];
    float2 uv   [[attribute(1)]];
};

struct ShaderMaskUniforms {
    float2 viewport;       // framebuffer width, height
    float  gradient_type;  // 0 = linear, 1 = radial
    float  _pad0;
    float4 gradient_p1;    // linear: begin.xy; radial: center.xy (pixels)
    float4 gradient_p2;    // linear: end.xy;   radial: radius in .x (pixels)
    float  blend_mode;     // 0 = srcIn (child * mask.a), 1 = modulate (* mask.rgb)
    float3 _pad1;
};

struct ShaderMaskVertOut {
    float4 pos           [[position]];
    float2 uv;
    float  gradient_type [[flat]];
    float4 gradient_p1   [[flat]];
    float4 gradient_p2   [[flat]];
    float  blend_mode    [[flat]];
};

vertex ShaderMaskVertOut shaderMaskVertex(
    ShaderMaskVertexIn           in [[stage_in]],
    constant ShaderMaskUniforms &u  [[buffer(1)]])
{
    // See quadVertex's doc comment for the full derivation.
    float clip_x =  2.0 * in.posw.x / u.viewport.x - in.posw.z;
    float clip_y = -(2.0 * in.posw.y / u.viewport.y - in.posw.z);

    ShaderMaskVertOut out;
    out.pos           = float4(clip_x, clip_y, 0.0, in.posw.z);
    out.uv             = in.uv;
    out.gradient_type  = u.gradient_type;
    out.gradient_p1    = u.gradient_p1;
    out.gradient_p2    = u.gradient_p2;
    out.blend_mode     = u.blend_mode;
    return out;
}

// Reads only [[stage_in]] — no separate uniform buffer binding, mirroring
// shapeFragment() above. The composite draw only binds a vertex buffer
// (encoder.setVertexBuffer), not a fragment buffer, so a fragment-stage
// `constant ... [[buffer(0)]]` parameter here would read unbound/garbage
// memory instead of the uniforms actually set for this draw.
fragment float4 shaderMaskFragment(
    ShaderMaskVertOut in    [[stage_in]],
    texture2d<float>  child [[texture(0)]],
    texture2d<float>  lut   [[texture(1)]],
    sampler           smp   [[sampler(2)]])
{
    float4 child_color = child.sample(smp, in.uv);

    // Reconstruct fragment position in viewport pixels.
    float2 pos = in.pos.xy;

    // Compute gradient parameter t.
    float t;
    if (in.gradient_type < 0.5) {
        // Linear gradient
        float2 p1  = in.gradient_p1.xy;
        float2 p2  = in.gradient_p2.xy;
        float2 dir = p2 - p1;
        float  len2 = dot(dir, dir);
        t = (len2 > 0.0001) ? dot(pos - p1, dir) / len2 : 0.0;
    } else {
        // Radial gradient
        float2 center = in.gradient_p1.xy;
        float  radius = in.gradient_p2.x;
        t = (radius > 0.0001) ? length(pos - center) / radius : 0.0;
    }
    t = clamp(t, 0.0, 1.0);

    float4 mask_color = lut.sample(smp, float2(t, 0.5));

    // Apply blend mode.
    if (in.blend_mode < 0.5) {
        // srcIn: output = child * mask.a
        return child_color * mask_color.a;
    } else {
        // modulate: output = child * mask
        return child_color * mask_color;
    }
}

// ---------------------------------------------------------------------------
// ClipShape pipeline — composites a child texture through a rounded-rect or
// ellipse SDF mask (used by ClipRRect / ClipOval).
//
// Real per-vertex position(+w)/uv data at buffer(0) — see ClipShapeVertexIn
// below. Uniforms at [[buffer(1)]]: ClipShapeUniforms
// child texture  at [[texture(0)]]
// sampler        at [[sampler(1)]]
// ---------------------------------------------------------------------------

// Same QuadVertexIn shape as the quad pipeline above — position(+w) is the
// ambient-transform-projected destination corner, uv is this corner's
// [0,1]² coordinate within the shape's own local space. The rounded-rect/
// ellipse SDF below is evaluated entirely in that local space (via uv), so
// it renders correctly regardless of how the destination quad is
// projected — rotated, perspective-foreshortened, or mirrored, no
// special-casing needed for any of those (unlike the axis-aligned-only
// version of this shader, which needed a `flip` workaround for mirroring
// and couldn't represent rotation/perspective in its destination shape at
// all — see TODO.md's "Bug: Transform content vanishes..." and "Real
// per-vertex quad rendering" entries).
struct ClipShapeVertexIn {
    float3 posw [[attribute(0)]];
    float2 uv   [[attribute(1)]];
};

struct ClipShapeUniforms {
    float2 rect_size; // the shape's plain LOGICAL width/height — NOT
                       // physical pixels, NOT transform-scaled. Needed by
                       // the SDF below, which operates in local space.
    float2 viewport;  // framebuffer width, height — for clip-space conversion only
    float  corner_r;  // LOGICAL corner radius (rrect); ignored when kind == 1
    float  kind;      // 0 = rounded rect, 1 = ellipse/oval
    float2 _pad;
};

struct ClipShapeVertOut {
    float4 pos       [[position]];
    float2 uv;
    float2 rect_size [[flat]];
    float  corner_r  [[flat]];
    float  kind      [[flat]];
};

vertex ClipShapeVertOut clipShapeVertex(
    ClipShapeVertexIn           in [[stage_in]],
    constant ClipShapeUniforms &u  [[buffer(1)]])
{
    // See quadVertex's doc comment for the full derivation of this
    // pixel-space-(x,y,w) -> clip-space conversion.
    float clip_x =  2.0 * in.posw.x / u.viewport.x - in.posw.z;
    float clip_y = -(2.0 * in.posw.y / u.viewport.y - in.posw.z);

    ClipShapeVertOut out;
    out.pos       = float4(clip_x, clip_y, 0.0, in.posw.z);
    out.uv        = in.uv;
    out.rect_size = u.rect_size;
    out.corner_r  = u.corner_r;
    out.kind      = u.kind;
    return out;
}

// Reads only [[stage_in]] — no separate uniform buffer binding, mirroring
// shapeFragment() above. The composite draw only binds a vertex buffer
// (encoder.setVertexBuffer), not a fragment buffer, so a fragment-stage
// `constant ... [[buffer(0)]]` parameter here would read unbound/garbage
// memory instead of the uniforms actually set for this draw.
fragment float4 clipShapeFragment(
    ClipShapeVertOut in    [[stage_in]],
    texture2d<float> child [[texture(0)]],
    sampler          smp   [[sampler(1)]])
{
    float4 child_color = child.sample(smp, in.uv);

    // SDF is evaluated in pixel space centered on the shape's bounds.
    float2 hs = in.rect_size * 0.5;
    float2 p  = (in.uv - float2(0.5, 0.5)) * in.rect_size;

    float d;
    if (in.kind < 0.5) {
        // Rounded-rect SDF
        float r  = min(in.corner_r, min(hs.x, hs.y));
        float2 q = abs(p) - hs + r;
        d = length(max(q, float2(0.0))) + min(max(q.x, q.y), 0.0) - r;
    } else {
        // Ellipse SDF (circle when hs.x == hs.y)
        float2 s = p / hs;
        d = (length(s) - 1.0) * min(hs.x, hs.y);
    }

    const float aa    = 0.5;
    const float alpha = 1.0 - smoothstep(-aa, aa, d);
    return child_color * alpha;
}
