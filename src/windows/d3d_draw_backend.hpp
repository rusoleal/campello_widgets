#pragma once

#include <campello_widgets/ui/draw_backend.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/text_span.hpp>
#include <campello_gpu/constants/pixel_format.hpp>
#include <campello_gpu/constants/texture_usage.hpp>
#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace systems::leal::campello_gpu
{
    class Device;
    class RenderPipeline;
    class BindGroupLayout;
    class BindGroup;
    class Sampler;
    class Texture;
    class Buffer;
    class CommandEncoder;
    class RenderPassEncoder;
}

// Windows forward declaration — avoids pulling <windows.h> into this header
// just for the font cache below.
struct HFONT__;
typedef HFONT__* HFONT;

namespace systems::leal::campello_widgets
{

// ---------------------------------------------------------------------------
// D3DDrawBackend
//
// IDrawBackend implementation for Windows/DirectX 12. widgets/dx12.metal's
// Metal source (shaders/metal/widgets.metal) is the source-of-truth for the
// shader math and uniform-struct field layout — see shaders/dx12/*.hlsl,
// which mirror it entry-point-for-entry-point. The C++ binding plumbing
// below instead follows the Vulkan backend's pattern (uniform data delivered
// via BindGroupLayout/BindGroup CBVs), since DirectX 12 has no equivalent of
// Metal's "raw buffer index reinterpreted by the shader" trick — vertex
// buffer slots on this backend are always strided per-vertex-index fetches.
//
// Pipelines:
//   rect_pipeline_  — solid-coloured filled quads   (RectVS/RectPS)
//   quad_pipeline_  — textured quads (text, images)  (QuadVS/QuadPS)
//   shape_pipeline_ — circle/oval/rounded-rect SDF   (ShapeVS/ShapePS)
//   line_pipeline_  — arbitrary-angle line segments  (LineVS/LinePS)
//
// Text is rasterized via GDI (white-on-black, used as a premultiplied-alpha
// mask against the paint color) into a cached GPU texture, then drawn
// through the quad pipeline — analogous to Metal's CoreText pre-compositing.
//
// Call setViewport(w, h) once per frame before Renderer::renderFrame().
// ---------------------------------------------------------------------------
class D3DDrawBackend final : public IDrawBackend
{
public:
    D3DDrawBackend(
        std::shared_ptr<campello_gpu::Device> device,
        Color                                 bg_color,
        campello_gpu::PixelFormat             pixel_format);

    ~D3DDrawBackend() override;

    void drawRect(
        const DrawRectCmd&               cmd,
        const Matrix4&                   transform,
        const Rect&                      clip,
        campello_gpu::RenderPassEncoder& encoder) override;

    void drawCircle(
        const DrawCircleCmd&             cmd,
        const Matrix4&                   transform,
        const Rect&                      clip,
        campello_gpu::RenderPassEncoder& encoder) override;

    void drawOval(
        const DrawOvalCmd&               cmd,
        const Matrix4&                   transform,
        const Rect&                      clip,
        campello_gpu::RenderPassEncoder& encoder) override;

    void drawRRect(
        const DrawRRectCmd&              cmd,
        const Matrix4&                   transform,
        const Rect&                      clip,
        campello_gpu::RenderPassEncoder& encoder) override;

    void drawLine(
        const DrawLineCmd&               cmd,
        const Matrix4&                   transform,
        const Rect&                      clip,
        campello_gpu::RenderPassEncoder& encoder) override;

    void drawArc(
        const DrawArcCmd&                cmd,
        const Matrix4&                   transform,
        const Rect&                      clip,
        campello_gpu::RenderPassEncoder& encoder) override;

    void drawPath(
        const DrawPathCmd&               cmd,
        const Matrix4&                   transform,
        const Rect&                      clip,
        campello_gpu::RenderPassEncoder& encoder) override;

    void drawPoints(
        const DrawPointsCmd&             cmd,
        const Matrix4&                   transform,
        const Rect&                      clip,
        campello_gpu::RenderPassEncoder& encoder) override;

    std::shared_ptr<campello_gpu::Texture> rasterizeText(
        const TextSpan& span, float dpr,
        uint32_t& out_width, uint32_t& out_height) override;

    std::shared_ptr<campello_gpu::BindGroup> drawTextTexture(
        std::shared_ptr<campello_gpu::Texture>   texture,
        std::shared_ptr<campello_gpu::BindGroup> cached_bind_group,
        uint32_t width, uint32_t height,
        const Offset&                     origin,
        const Matrix4&                    transform,
        const Rect&                       clip,
        campello_gpu::RenderPassEncoder&  encoder) override;

    void drawImage(
        const DrawImageCmd&              cmd,
        const Matrix4&                   transform,
        const Rect&                      clip,
        campello_gpu::RenderPassEncoder& encoder) override;

    void drawTintedImage(
        const DrawTintedImageCmd&        cmd,
        const Matrix4&                   transform,
        const Rect&                      clip,
        campello_gpu::RenderPassEncoder& encoder) override;

    Size measureText(const TextSpan& span) const override;

    campello_gpu::PixelFormat offscreenPixelFormat() const noexcept override
    {
        return pixel_format_;
    }

    // ------------------------------------------------------------------
    // Offscreen / compositing (BackdropFilter)
    // ------------------------------------------------------------------

    std::shared_ptr<campello_gpu::Texture> createOffscreenTexture(
        uint32_t width, uint32_t height) override;

    std::shared_ptr<campello_gpu::Texture> createDedicatedOffscreenTexture(
        uint32_t width, uint32_t height) override;

    std::shared_ptr<campello_gpu::RenderPassEncoder> beginOffscreenPass(
        std::shared_ptr<campello_gpu::Texture> tex,
        campello_gpu::CommandEncoder&          encoder,
        bool                                    preserve_content = false) override;

    std::shared_ptr<campello_gpu::Texture> blurTexture(
        std::shared_ptr<campello_gpu::Texture> source,
        float sigma_x, float sigma_y,
        campello_gpu::CommandEncoder& encoder) override;

    void drawBackdropFilter(
        const DrawBackdropFilterBeginCmd&      cmd,
        std::shared_ptr<campello_gpu::Texture> blurred_source,
        const Matrix4&                         transform,
        const Rect&                            clip,
        campello_gpu::RenderPassEncoder&       encoder) override;

    void drawClipShapeComposite(
        std::shared_ptr<campello_gpu::Texture> child_tex,
        const Rect&                             bounds,
        float                                   corner_radius,
        bool                                    is_oval,
        const Matrix4&                          transform,
        const Rect&                             clip,
        campello_gpu::RenderPassEncoder&        encoder) override;

    void saveLayerComposite(
        std::shared_ptr<campello_gpu::Texture> child_tex,
        const SaveLayerCmd&                     cmd,
        const Matrix4&                          transform,
        const Rect&                             clip,
        campello_gpu::RenderPassEncoder&        encoder) override;

    void drawShaderMaskComposite(
        std::shared_ptr<campello_gpu::Texture> child_tex,
        const DrawShaderMaskBeginCmd&           cmd,
        const Matrix4&                          transform,
        const Rect&                             clip,
        campello_gpu::RenderPassEncoder&        encoder) override;

    void setViewport(float w, float h) noexcept override
    {
        setViewportSize(w, h);
        ++frame_counter_;
        rect_uniform_pool_.beginFrame();
        shape_uniform_pool_.beginFrame();
        line_uniform_pool_.beginFrame();
        quad_uniform_pool_.beginFrame();
        rect_vertex_pool_.beginFrame();
        quad_vertex_pool_.beginFrame();
        blur_uniform_pool_.beginFrame();
        clip_shape_uniform_pool_.beginFrame();
        shader_mask_uniform_pool_.beginFrame();
        offscreen_texture_pool_.beginFrame();
        offscreen_texture_pool_.evictStale(frame_counter_);
        blur_texture_pool_.beginFrame();
        blur_texture_pool_.evictStale(frame_counter_);
        evictStaleSourceBindGroups();
    }

    void setViewportSize(float w, float h) noexcept override
    {
        vp_w_ = w;
        vp_h_ = h;
        last_scissor_x_ = last_scissor_y_ = last_scissor_w_ = last_scissor_h_ = -1.0f;
        viewport_dirty_ = true;
    }

    // Unlike Metal/Vulkan (which get a default full-target viewport for free
    // when a render pass begins), campello_gpu's DirectX 12 beginRenderPass()
    // never calls RSSetViewports at all — without an explicit call every draw
    // rasterizes against an undefined/zero viewport, i.e. nothing visible.
    // onBeginFlush() runs once per flushDrawList(), which always follows a
    // beginRenderPass(); marking the viewport dirty here (in addition to
    // setViewportSize() above) guarantees the first draw of every pass
    // re-emits it — see applyScissor(), the one choke point all draw*()
    // overrides call before touching the encoder.
    void onBeginFlush() noexcept override { viewport_dirty_ = true; }

    void setDevicePixelRatio(float dpr) noexcept override { dpr_ = dpr; }

    /** Returns true if all render pipelines were successfully compiled. */
    bool isValid() const noexcept { return rect_pipeline_ != nullptr; }

private:
    bool applyScissor(const Rect& clip, campello_gpu::RenderPassEncoder& encoder);

    // One ambient-transform-projected corner: (x, y, w) is the result of
    // `transform * Vector4(local_x, local_y, 0, 1)`, kept as-is — never
    // collapsed to an axis-aligned bounding box. (u, v) is this corner's own
    // texture-space sample coordinate. See MetalDrawBackend::ProjectedCorner
    // (widgets.metal's QuadVertexIn doc comment) for the full rationale.
    struct ProjectedCorner { float x, y, w, u, v; };

    // Same layout used by drawFilledVertices(); defined in the header so
    // callers (and the Windows visual-test factory) can build the vertex
    // vector without depending on the .cpp implementation.
    struct RectVertex { float x, y, w; };

    // Same layout as RectVertex plus a per-vertex alpha, for
    // rect_aa_pipeline_ — see src/gpu/path_fill_aa.hpp's doc comment and
    // drawFillAA() below.
    struct RectAAVertex { float x, y, w, a; };

    void drawFilledQuad(
        const ProjectedCorner& c00, const ProjectedCorner& c10,
        const ProjectedCorner& c01, const ProjectedCorner& c11,
        const Color& color,
        campello_gpu::RenderPassEncoder& encoder);

    // Variable-length triangle batch for drawArc / drawPath. `verts` uses the
    // same RectVertex layout as drawFilledQuad; `count` must be a multiple of 3.
    void drawFilledVertices(
        const std::vector<RectVertex>& verts,
        const Color& color,
        campello_gpu::RenderPassEncoder& encoder);

    // Same shape as drawFilledVertices(), but via rect_aa_pipeline_: each
    // vertex also carries its own alpha, multiplied into `color`'s alpha
    // and linearly interpolated by the rasterizer. Used for drawPath()'s
    // fill antialiasing skirt — see src/gpu/path_fill_aa.hpp.
    void drawFillAA(
        const std::vector<RectAAVertex>& verts,
        const Color& color,
        campello_gpu::RenderPassEncoder& encoder);

    void drawShape(
        float x, float y, float w, float h,
        float corner_r, float stroke_w, float kind,
        const Color& color,
        campello_gpu::RenderPassEncoder& encoder);

    // ------------------------------------------------------------------
    // Stroke primitives — GPU-side cap/join expansion (see
    // src/gpu/stroke_geometry.hpp's doc comment for the overall design:
    // buildStrokeGeometry() does O(1)-per-element CPU vector math to decide
    // *what* to draw; these methods do the actual antialiased GPU rendering).
    // Points are in local (pre-transform) space, matching drawLine()/
    // drawRect()/drawPath()'s existing convention. Mirrors
    // MetalDrawBackend's methods of the same name field-for-field — D3D's
    // line_pipeline_/shape_pipeline_/rect_pipeline_ each use a single
    // hardcoded blend state (like Metal, unlike Vulkan), so no blend-mode
    // selection is needed here.
    // ------------------------------------------------------------------

    // One antialiased, butt-ended segment body via line_pipeline_.
    void drawStrokeSegmentBody(
        const Offset& p0, const Offset& p1, float half_width,
        const Color& color, const Matrix4& transform,
        campello_gpu::RenderPassEncoder& encoder);

    // One antialiased round cap/join, as a filled circle via drawShape().
    void drawStrokeRoundPrimitive(
        const Offset& center, float half_width,
        const Color& color, const Matrix4& transform,
        campello_gpu::RenderPassEncoder& encoder);

    // Strokes a polyline as individual GPU draw calls (segment bodies +
    // round caps/joins via drawShape() + bevel/miter wedges via
    // drawFilledVertices()). For drawLine()/drawRect() stroke, where point
    // count is always small — same "one shape = a few draw calls" cost
    // class as every other shape primitive in this backend.
    void strokePolyline(
        const std::vector<Offset>& points, bool closed,
        const Paint& paint, const Rect& clip, const Matrix4& transform,
        campello_gpu::RenderPassEncoder& encoder);

    // Same decomposition as strokePolyline(), but appends flat (non-AA)
    // triangles to `verts` instead of issuing per-primitive draw calls —
    // preserves drawPath()'s existing "one draw call regardless of segment
    // count" property for potentially long flattened curves. Round caps/
    // joins are approximated as a small triangle fan (no SDF available in
    // this batched context).
    void appendStrokePolylineBatched(
        const std::vector<Offset>& points, bool closed,
        const Paint& paint, const Matrix4& transform,
        std::vector<RectVertex>& verts);

    // Returns the BindGroup actually used — either `cached_bind_group`
    // passed straight through, or a freshly built one (nullptr if drawing
    // was aborted, e.g. missing pipeline). Callers that want to skip
    // rebuilding it next time should keep it and pass it back in.
    // `sampler`: nullptr (default) uses quad_sampler_ (linear), matching
    // every existing call site's behavior unchanged. drawImage() passes
    // nearest_sampler_ explicitly when the caller requests
    // FilterQuality::none. Only takes effect when `cached_bind_group` is
    // null (a supplied cache already has its own sampler baked in).
    std::shared_ptr<campello_gpu::BindGroup> drawTexturedQuad(
        std::shared_ptr<campello_gpu::Texture>  texture,
        const ProjectedCorner& c00, const ProjectedCorner& c10,
        const ProjectedCorner& c01, const ProjectedCorner& c11,
        float opacity,
        campello_gpu::RenderPassEncoder&        encoder,
        std::shared_ptr<campello_gpu::BindGroup> cached_bind_group = nullptr,
        std::shared_ptr<campello_gpu::Sampler>   sampler = nullptr);

    // Mirrors drawTexturedQuad() exactly, but binds icon_pipeline_ (which
    // samples only the source texture's alpha channel and recolors with
    // `tint` — see IconPS in icon.hlsl) instead of quad_pipeline_. Kept as
    // its own method rather than adding a tint parameter to
    // drawTexturedQuad() so every existing call site is untouched — same
    // rationale as MetalDrawBackend::drawTintedTexturedQuad().
    // `sampler`: see drawTexturedQuad()'s doc comment.
    void drawTintedTexturedQuad(
        std::shared_ptr<campello_gpu::Texture>  texture,
        const ProjectedCorner& c00, const ProjectedCorner& c10,
        const ProjectedCorner& c01, const ProjectedCorner& c11,
        const Color& tint,
        float opacity,
        campello_gpu::RenderPassEncoder&        encoder,
        std::shared_ptr<campello_gpu::Sampler>   sampler = nullptr);

    // Runs a single-pass blur render (horizontal or vertical) from src into
    // dst — see blurTexture(), which chains two of these. `src_bind_group`
    // must be a texture@0/sampler@1 BindGroup for `src` against
    // quad_tex_bgl_ — callers resolve it via lookupOrCreateSourceBindGroup()
    // so it's never rebuilt from scratch on every frame.
    void runBlurPass(
        std::shared_ptr<campello_gpu::Texture> src,
        std::shared_ptr<campello_gpu::Texture> dst,
        float sigma,
        bool  horizontal,
        campello_gpu::CommandEncoder& encoder,
        std::shared_ptr<campello_gpu::BindGroup> src_bind_group);

    // Build a 256×1 RGBA LUT texture from gradient colors/stops.
    std::shared_ptr<campello_gpu::Texture> buildGradientLUT(
        const std::vector<Color>& colors,
        const std::vector<float>& stops);

    // ------------------------------------------------------------------
    // Plain vertex-buffer pool — real per-vertex (x, y, w[, u, v]) data for
    // the rect/quad pipelines (see drawFilledQuad()/drawTexturedQuad()).
    // No bind group involved, so a fresh Buffer per ring slot is enough
    // (IASetVertexBuffers doesn't consume shader-visible heap descriptors).
    // ------------------------------------------------------------------

    class VertexBufferPool
    {
    public:
        std::shared_ptr<campello_gpu::Buffer> acquire(
            campello_gpu::Device& device, uint64_t size, const void* data);

        void beginFrame() noexcept;

    private:
        static constexpr size_t kGenerations = 4;
        std::array<std::vector<std::shared_ptr<campello_gpu::Buffer>>, kGenerations> generations_;
        std::array<size_t, kGenerations>                                              next_index_{};
        size_t                                                                        current_generation_ = 0;
    };

    // ------------------------------------------------------------------
    // Uniform buffer + bind group pool
    //
    // Unlike Metal (which reinterprets a plain vertex-buffer-slot pointer as
    // a "constant" uniform with no separate binding step), DirectX 12
    // delivers uniform data via a CBV inside a BindGroup, and a BindGroup's
    // descriptor is baked to a specific GPU buffer address at creation time
    // (Device::createBindGroup). Creating a fresh BindGroup on every single
    // draw call would exhaust campello_gpu's fixed-size shader-visible
    // CBV/SRV/UAV heap (1024 descriptors) within a few frames. Instead, each
    // ring slot's Buffer+BindGroup pair is created once and reused —
    // re-upload()ing new contents into the same Buffer is enough to update
    // what the existing BindGroup's CBV reads, since re-upload doesn't move
    // the buffer's GPU address.
    // ------------------------------------------------------------------

    class UniformBindGroupPool
    {
    public:
        // Returns the buffer to upload this draw's data into and the
        // bind group to bind via setBindGroup(0, ...) — building the
        // BindGroup (and its backing Buffer) once per ring slot.
        struct Slot
        {
            std::shared_ptr<campello_gpu::Buffer>    buffer;
            std::shared_ptr<campello_gpu::BindGroup> bind_group;
        };

        Slot acquire(
            campello_gpu::Device& device,
            const std::shared_ptr<campello_gpu::BindGroupLayout>& layout,
            uint64_t size, const void* data);

        void beginFrame() noexcept;

    private:
        static constexpr size_t kGenerations = 4;
        std::array<std::vector<Slot>, kGenerations> generations_;
        std::array<size_t, kGenerations>            next_index_{};
        size_t                                       current_generation_ = 0;
    };

    // ------------------------------------------------------------------
    // Offscreen texture pool — see MetalDrawBackend's identical rationale:
    // ClipRRect/ClipOval/ShaderMask/BackdropFilter composites call
    // createOffscreenTexture() every frame for continuously-animating
    // content; this amortizes the GPU texture allocation, keyed by
    // (width, height) since offscreen bounds vary per widget, with the same
    // kGenerations-deep ring so a slot isn't reused until enough frames have
    // passed that the GPU is done reading it from its last use. A distinct
    // size stops being requested when a widget resizes/leaves the tree, so
    // unused buckets are evicted after kMaxAgeFrames unused.
    // ------------------------------------------------------------------

    class OffscreenTexturePool
    {
    public:
        std::shared_ptr<campello_gpu::Texture> acquire(
            campello_gpu::Device& device, uint32_t width, uint32_t height,
            campello_gpu::PixelFormat format, campello_gpu::TextureUsage usage,
            uint64_t current_frame);

        void beginFrame() noexcept;
        void evictStale(uint64_t current_frame);

    private:
        struct SizeKey
        {
            uint32_t width  = 0;
            uint32_t height = 0;
            bool operator==(const SizeKey&) const noexcept = default;
        };
        struct SizeKeyHash
        {
            size_t operator()(const SizeKey& k) const noexcept
            {
                return (static_cast<size_t>(k.width) << 32) ^ static_cast<size_t>(k.height);
            }
        };

        // 2, not 4: campello_gpu's DirectX backend now pipelines exactly
        // DeviceData::kFramesInFlight (= 2) frames — Device::
        // createCommandEncoder() fence-gates reuse of a ring slot until that
        // specific slot's own frame is confirmed GPU-idle (see
        // DeviceData::beginFrameRing()'s doc comment in campello_gpu), so a
        // texture from generation g is guaranteed safe to hand out again
        // once this pool has rotated back to g — no need to track more
        // generations than the GPU pipeline itself is deep. (This used to be
        // 1, back when Device::submit() blocked synchronously and every
        // resource was unconditionally GPU-idle by the next frame — see
        // campello_gpu's CHANGELOG for the frame-pacing fix that changed
        // this.) Each of these is a real GPU-memory render-target texture
        // (unlike the small CPU-side uniform/vertex buffer pools elsewhere,
        // which keep kGenerations=4) — on this Intel iGPU's limited
        // dedicated VRAM, 4 generations × many differently-sized
        // ClipRRect/ClipOval widgets (see kMaxSizeBuckets below) was enough
        // to exhaust graphics memory and crash deep inside the driver during
        // CreateCommittedResource, confirmed via a real minidump stack
        // trace — 2 generations only doubles (not quadruples) this pool's
        // footprint relative to the crash-causing configuration.
        static constexpr size_t   kGenerations   = 2;
        static constexpr uint64_t kMaxAgeFrames  = 120;
        // Hard cap on distinct (width, height) buckets tracked at once, on
        // top of the time-based eviction above. Renderer::applyClipShape()
        // calls createOffscreenTexture() for EVERY ClipRRect/ClipOval widget
        // (rounded avatars, buttons, thumbnails — common and varied in size
        // across a real UI), not just BackdropFilter's blur capture. Without
        // this cap, a UI with many differently-sized rounded-corner widgets
        // all recently visible (all within kMaxAgeFrames of each other, so
        // none age out) grows one full-size GPU texture × kGenerations per
        // distinct size, unboundedly — which exhausted VRAM on this Intel
        // iGPU's limited dedicated memory and crashed deep inside the
        // driver/D3D12 runtime with wildly inconsistent timing (depending on
        // how many distinct sizes happened to be exercised).
        static constexpr size_t   kMaxSizeBuckets = 24;

        std::array<std::unordered_map<SizeKey, std::vector<std::shared_ptr<campello_gpu::Texture>>, SizeKeyHash>, kGenerations> generations_;
        std::array<std::unordered_map<SizeKey, size_t, SizeKeyHash>, kGenerations>                                              next_index_;
        std::unordered_map<SizeKey, uint64_t, SizeKeyHash>                                                                      last_used_frame_;
        size_t current_generation_ = 0;

        // Evicts the single least-recently-used bucket (excluding `keep`,
        // the bucket just touched by the current acquire() call).
        void evictLeastRecentlyUsed(const SizeKey& keep);
    };

    uint64_t frame_counter_ = 0;

    // ------------------------------------------------------------------
    // GDI font cache — both measureText() (layout) and
    // rasterizeText() (paint) independently need an HFONT for
    // the same TextStyle; without this, CreateFontIndirectW() (plus a fresh
    // CreateCompatibleDC/DeleteDC pair) ran on EVERY call from EITHER path,
    // even though a typical UI reuses a small, fixed set of text styles
    // across many spans. Reduced to the same fields createFontForStyle()
    // actually uses (rounded integer height, not raw float, so e.g. 14.0f
    // and 14.2f share one cached font).
    // ------------------------------------------------------------------

    struct FontCacheKey
    {
        std::wstring family;
        int          height = 0;
        bool         bold   = false;
        bool         italic = false;
        bool operator==(const FontCacheKey&) const noexcept = default;
    };
    struct FontCacheKeyHash
    {
        size_t operator()(const FontCacheKey& k) const noexcept;
    };

    // mutable: measureText() is const (it's a pure query from the caller's
    // point of view) but still needs to populate this cache on a miss.
    HFONT getOrCreateFont(const TextStyle& style) const;
    mutable std::unordered_map<FontCacheKey, HFONT, FontCacheKeyHash> font_cache_;

    UniformBindGroupPool rect_uniform_pool_;
    UniformBindGroupPool shape_uniform_pool_;
    UniformBindGroupPool line_uniform_pool_;
    UniformBindGroupPool quad_uniform_pool_;
    // Icon pipeline's uniforms (viewport/opacity/tint — larger than
    // QuadUniforms, so it needs its own pool rather than sharing
    // quad_uniform_pool_'s ring).
    UniformBindGroupPool icon_uniform_pool_;
    UniformBindGroupPool blur_uniform_pool_;
    UniformBindGroupPool clip_shape_uniform_pool_;
    UniformBindGroupPool shader_mask_uniform_pool_;

    // Real per-vertex position(+uv) data for the rect/quad pipelines —
    // separate from the uniform pools above (see VertexBufferPool's doc
    // comment: different buffer usage, no bind group).
    VertexBufferPool rect_vertex_pool_;
    VertexBufferPool quad_vertex_pool_;

    OffscreenTexturePool offscreen_texture_pool_;

    std::shared_ptr<campello_gpu::Device>          device_;
    Color                                           bg_color_;
    campello_gpu::PixelFormat                       pixel_format_;

    std::shared_ptr<campello_gpu::RenderPipeline>   rect_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>   rect_aa_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>   shape_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>   line_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>   quad_pipeline_;
    // Icon pipeline (tinted template images — see icon.hlsl).
    std::shared_ptr<campello_gpu::RenderPipeline>   icon_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>   blur_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>   clip_shape_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>   shader_mask_pipeline_;

    std::shared_ptr<campello_gpu::BindGroupLayout>  rect_bgl_;
    std::shared_ptr<campello_gpu::BindGroupLayout>  shape_bgl_;
    std::shared_ptr<campello_gpu::BindGroupLayout>  line_bgl_;
    std::shared_ptr<campello_gpu::BindGroupLayout>  quad_uniform_bgl_;  // uniform@0 (bind group 0)
    std::shared_ptr<campello_gpu::BindGroupLayout>  quad_tex_bgl_;      // texture@0, sampler@1 (bind group 1)
    // Icon pipeline's uniform bind group (bind group 0) — icon_layout's
    // bind group 1 (texture+sampler) reuses quad_tex_bgl_ directly.
    std::shared_ptr<campello_gpu::BindGroupLayout>  icon_uniform_bgl_;
    std::shared_ptr<campello_gpu::BindGroupLayout>  blur_uniform_bgl_;  // uniform@0 (bind group 0)
    // blur's and clip-shape's texture+sampler (bind group 1) both reuse
    // quad_tex_bgl_ — same texture@0/sampler@1 structure, no need for
    // additional identical layouts.
    std::shared_ptr<campello_gpu::BindGroupLayout>  clip_shape_uniform_bgl_;  // uniform@0 (bind group 0)
    std::shared_ptr<campello_gpu::BindGroupLayout>  shader_mask_uniform_bgl_; // uniform@0 (bind group 0)
    std::shared_ptr<campello_gpu::BindGroupLayout>  shader_mask_bgl_;         // child@0, lut@1, sampler@2 (bind group 1)

    std::shared_ptr<campello_gpu::Sampler>          quad_sampler_;

    // Nearest-neighbor counterpart to quad_sampler_ (linear), selected by
    // drawImage()/drawTintedImage() only, per-draw, when the caller passes
    // FilterQuality::none.
    std::shared_ptr<campello_gpu::Sampler>          nearest_sampler_;

    // Blur scratch textures — see blurTexture(). Acquired per-call from a
    // dedicated OffscreenTexturePool (same size-keyed/generation-rotated
    // pool used for BackdropFilter/ClipRRect captures — see that class'
    // doc comment for why kGenerations=2 is exactly right for campello_gpu's
    // DirectX backend's 2-deep frame pipelining) instead of a single fixed
    // (width, height) pair.
    //
    // A single shared pair used to be enough back when Device::submit()
    // blocked synchronously, but campello_gpu's DirectX backend now
    // pipelines up to kFramesInFlight (=2) frames — and a UI can easily ask
    // blurTexture() for more than one distinct size in the same session
    // (e.g. two differently-sized DrawShadowCmds), or even the same frame.
    // A single-pair-per-backend design has to blow away EVERY generation
    // and recreate at the new size the moment any caller asks for a
    // different (width, height) than last time — including whichever
    // generation the GPU might still be mid-flight reading from a *prior*
    // frame's blur (the ~2-frame pipeline hasn't confirmed it idle yet).
    // Destroying that texture out from under an unfinished command list is
    // exactly the D3D12 resource-lifetime violation the debug layer flags
    // as "deleted prior to closing/executing the command list", escalating
    // to a driver-level corruption abort under sustained load (confirmed
    // via a real minidump: the crash's stack terminates in
    // D3D12SDKLayers!ReportCorruption, called from campello_gpu::Texture's
    // destructor, called from this old resize path). A size-keyed pool
    // sidesteps this the same way it already does for every other offscreen
    // consumer: a new size gets its own bucket instead of evicting
    // in-flight generations of an unrelated size.
    OffscreenTexturePool blur_texture_pool_;

    // Cache for any texture@0/sampler@1 BindGroup read by the blur/backdrop
    // path (each blur_texture_pool_-sourced h/v scratch texture, AND the
    // rotating OffscreenTexturePool "source" capture texture) OR by
    // drawClipShapeComposite()'s child_tex (also OffscreenTexturePool-sourced).
    // A per-Texture-object cache works uniformly for all three cases, keyed
    // by raw Texture* — whenever the pool hands back the same object it
    // handed out before (common once a given size's rotation warms up),
    // this is a cache hit; a churned/new object is just a cache miss, never
    // a correctness issue.
    // Without it, a fresh BindGroup every frame permanently leaks one
    // shader-visible descriptor-heap slot (campello_gpu's DirectX backend
    // never reclaims them) — with a 65536-slot heap and ~60fps continuous
    // BackdropFilter animation, that's ~18 minutes to exhaustion, which is
    // exactly the delayed crash an earlier, narrower version of this cache
    // (single slots for only the old fixed blur pair, before they were
    // double-buffered) missed.
    // Keyed by raw Texture* for lookup, but the map ALSO holds a strong
    // shared_ptr<Texture> reference so a cached entry can never dangle even
    // if OffscreenTexturePool's own bookkeeping considers that texture
    // evicted.
    //
    // Eviction is age-based (kMaxSourceBindGroupCacheAgeFrames), NOT a
    // hard-size clear like this cache used to do. A size-triggered
    // `.clear()` mid-frame is a real correctness bug, not just a perf
    // tradeoff: campello_gpu's DirectX BindGroup::~BindGroup() frees its
    // shader-visible descriptor slot for *reuse by a later allocation in
    // the very same not-yet-submitted command list* (descriptor tables are
    // read by the GPU at execution time, not at recording time — see
    // DeviceData::allocSrvIndex()'s doc comment upstream). A GridView with
    // many same-sized ClipRRect cells calls this once per cell, all within
    // one frame — clearing at a small fixed count (this used to be 16)
    // destroys still-referenced BindGroups whose composite draw call was
    // already recorded earlier in this same frame but hasn't executed yet,
    // so a later cell's fresh BindGroup can steal that freed slot before
    // the GPU ever reads the earlier cell's descriptor — the earlier cell
    // then renders whatever the later cell wrote there instead of its own
    // content. Confirmed via tracing: CPU-side bounds/color/text/texture
    // were all correct and unique per cell, but on-screen content past the
    // eviction threshold repeated an earlier cell's — i.e. a live-slot
    // stomp, not a logic error in what was recorded. Age-based eviction
    // (matching clip_shape_gpu_cache_/OffscreenTexturePool elsewhere in
    // this codebase) only ever drops entries unused for many frames, so it
    // can never evict something a same-frame draw call still depends on.
    struct SourceBindGroupCacheEntry
    {
        std::shared_ptr<campello_gpu::Texture>   texture;
        std::shared_ptr<campello_gpu::BindGroup> bind_group;
        uint64_t                                 last_used_frame = 0;
    };
    static constexpr uint64_t kMaxSourceBindGroupCacheAgeFrames = 120;
    std::unordered_map<const campello_gpu::Texture*, SourceBindGroupCacheEntry> blur_source_bind_group_cache_;

    // Drops any blur_source_bind_group_cache_ entry unused for more than
    // kMaxSourceBindGroupCacheAgeFrames frames — called once per real frame
    // (setViewport()), mirroring OffscreenTexturePool::evictStale().
    void evictStaleSourceBindGroups();

    // Looks up (or builds and caches) the texture@0/sampler@1 BindGroup for
    // `tex` against quad_tex_bgl_. Never evicts synchronously — see
    // blur_source_bind_group_cache_'s doc comment for why a same-frame
    // size-triggered clear was a real correctness bug, not just a perf
    // tradeoff. Stale entries are dropped only by evictStaleSourceBindGroups().
    std::shared_ptr<campello_gpu::BindGroup> lookupOrCreateSourceBindGroup(
        const std::shared_ptr<campello_gpu::Texture>& tex);

    float vp_w_ = 800.0f;
    float vp_h_ = 600.0f;
    float dpr_  = 1.0f;

    float last_scissor_x_ = -1.0f;
    float last_scissor_y_ = -1.0f;
    float last_scissor_w_ = -1.0f;
    float last_scissor_h_ = -1.0f;

    // See onBeginFlush()'s doc comment above.
    bool viewport_dirty_ = true;
};

} // namespace systems::leal::campello_widgets
