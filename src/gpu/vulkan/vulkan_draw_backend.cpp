#include "vulkan_draw_backend.hpp"
#include "text_rasterizer.hpp"
#include <campello_widgets/ui/draw_command.hpp>
#include <campello_widgets/ui/text_span.hpp>
#include <campello_widgets/ui/text_style.hpp>
#include <campello_widgets/ui/rect.hpp>

#include <campello_gpu/device.hpp>
#include <campello_gpu/command_encoder.hpp>
#include <campello_gpu/render_pipeline.hpp>
#include <campello_gpu/bind_group_layout.hpp>
#include <campello_gpu/pipeline_layout.hpp>
#include <campello_gpu/sampler.hpp>
#include <campello_gpu/texture.hpp>
#include <campello_gpu/texture_view.hpp>
#include <campello_gpu/render_pass_encoder.hpp>
#include <campello_gpu/buffer.hpp>
#include <campello_gpu/constants/buffer_usage.hpp>
#include <campello_gpu/constants/texture_type.hpp>
#include <campello_gpu/constants/texture_usage.hpp>
#include <campello_gpu/constants/shader_stage.hpp>
#include <campello_gpu/constants/primitive_topology.hpp>
#include <campello_gpu/descriptors/begin_render_pass_descriptor.hpp>

#include <shaders/vulkan_widgets.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace systems::leal::campello_widgets
{

namespace GPU = ::systems::leal::campello_gpu;

// ---------------------------------------------------------------------------
// Uniform structs (must match std140 layout in GLSL)
// ---------------------------------------------------------------------------
struct alignas(16) RectUniforms
{
    float rect[4];      // x, y, w, h
    float color[4];     // r, g, b, a
    float viewport[2];  // w, h
    float _pad[2];
};

struct alignas(16) RRectUniforms
{
    float rect[4];      // x, y, w, h
    float color[4];     // r, g, b, a
    float viewport[2];  // w, h
    float radius;
    float stroke_w;     // 0 = fill, >0 = stroke width in pixels
};

struct alignas(16) QuadUniforms
{
    float viewport[2];  // w, h (physical pixels)
    float opacity;
    float _pad;
};

// Per-vertex data uploaded in a vertex buffer — (x,y,w) projected pixel
// position with perspective-correct w, plus (u,v) texture coordinate.
// Matches layout(location=0) in vec3 in_posw / layout(location=1) in vec2 in_uv.
struct QuadVertex
{
    float x, y, w;  // projected pixel pos + w
    float u, v;     // UV
};

struct alignas(16) ClipShapeUniforms
{
    float rect_size[2];  // logical w, h (for SDF)
    float viewport[2];   // physical w, h
    float corner_r;      // logical corner radius
    float kind;          // 0 = rrect, 1 = oval
    float _pad[2];
};

struct alignas(16) BlurUniforms
{
    float dstRect[4];    // x, y, w, h  (pixels, destination quad)
    float srcRect[4];    // u0, v0, u1, v1 (normalised UV of source region)
    float viewport[2];   // framebuffer w, h
    float sigma;         // Gaussian sigma (pixels)
    float horizontal;    // 1.0 = H pass, 0.0 = V pass
    float tex_size[2];   // source texture w, h (pixels)
    float _pad[2];
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::shared_ptr<GPU::ShaderModule> loadSpv(
    std::shared_ptr<GPU::Device> device,
    const unsigned char* data,
    unsigned int size)
{
    return device->createShaderModule(data, size);
}

VulkanDrawBackend::~VulkanDrawBackend() = default;

static GPU::BlendState premultipliedAlphaBlend()
{
    GPU::BlendState bs{};
    bs.color.srcFactor = GPU::BlendFactor::one;
    bs.color.dstFactor = GPU::BlendFactor::oneMinusSrcAlpha;
    bs.color.operation = GPU::BlendOperation::add;
    bs.alpha.srcFactor = GPU::BlendFactor::one;
    bs.alpha.dstFactor = GPU::BlendFactor::oneMinusSrcAlpha;
    bs.alpha.operation = GPU::BlendOperation::add;
    return bs;
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

VulkanDrawBackend::VulkanDrawBackend(
    std::shared_ptr<GPU::Device> device,
    Color                        bg_color,
    GPU::PixelFormat             pixel_format)
    : device_(std::move(device))
    , bg_color_(bg_color)
    , pixel_format_(pixel_format)
{
    // Load SPIR-V shader modules
    auto rect_vert         = loadSpv(device_, shaders::krect_vert_spv,         shaders::krect_vert_spvSize);
    auto rect_frag         = loadSpv(device_, shaders::krect_frag_spv,         shaders::krect_frag_spvSize);
    auto colored_quad_vert = loadSpv(device_, shaders::kcolored_quad_vert_spv, shaders::kcolored_quad_vert_spvSize);
    auto rrect_vert        = loadSpv(device_, shaders::krrect_vert_spv,        shaders::krrect_vert_spvSize);
    auto rrect_frag        = loadSpv(device_, shaders::krrect_frag_spv,        shaders::krrect_frag_spvSize);
    auto quad_vert         = loadSpv(device_, shaders::kquad_vert_spv,         shaders::kquad_vert_spvSize);
    auto quad_frag         = loadSpv(device_, shaders::kquad_frag_spv,         shaders::kquad_frag_spvSize);

    if (!rect_vert || !rect_frag || !colored_quad_vert || !rrect_vert || !rrect_frag || !quad_vert || !quad_frag) {
        std::fprintf(stderr, "[VulkanDrawBackend] shader load failed: rect_v=%d rect_f=%d cq_v=%d rr_v=%d rr_f=%d q_v=%d q_f=%d\n",
            !!rect_vert, !!rect_frag, !!colored_quad_vert, !!rrect_vert, !!rrect_frag, !!quad_vert, !!quad_frag);
        return;
    }

    // rect/colored_quad/rrect no longer need a bind group layout at all —
    // their uniform data (RectUniforms/RRectUniforms, both 48 bytes) now
    // rides Vulkan push constants instead of a descriptor set (see
    // rect_layout_/rrect_layout_ below and drawRect()/drawRRect()/
    // drawCircle()). This eliminates a fresh vkAllocateDescriptorSets +
    // vkUpdateDescriptorSets on every single rect/rrect/circle draw call —
    // the dominant cost identified via Renderer::printRasterSubPhaseTimings
    // (rect alone was ~57 draws/frame). Metal already avoided this
    // (MetalDrawBackend::drawTexturedQuad binds equivalent per-draw data
    // through a plain vertex buffer slot, not a bind group), which is why
    // it measurably outperformed Vulkan on the same content.

    // Bind group layout for quad/clip_shape/blur: texture @ 1, sampler @ 2
    // (frag). Their uniform data (QuadUniforms/ClipShapeUniforms/
    // BlurUniforms — 16/32/64 bytes) now rides a push constant instead of
    // binding 0 here (see quad_layout_ below) — this bind group is now
    // texture+sampler only, and therefore genuinely reusable across draws
    // and frames for the same texture (unlike before, when it also carried
    // per-draw-varying uniform data, forcing a fresh vkAllocateDescriptorSets
    // on every draw regardless of caller intent — see drawTexturedQuad()'s
    // cached_bind_group handling, now able to actually honor it).
    {
        GPU::EntryObject tex_entry{};
        tex_entry.binding    = 1;
        tex_entry.visibility = GPU::ShaderStage::fragment;
        tex_entry.type       = GPU::EntryObjectType::texture;
        tex_entry.data.texture.multisampled  = false;
        tex_entry.data.texture.sampleType    = GPU::EntryObjectTextureType::ttFloat;
        tex_entry.data.texture.viewDimension = GPU::TextureType::tt2d;

        GPU::EntryObject smp_entry{};
        smp_entry.binding    = 2;
        smp_entry.visibility = GPU::ShaderStage::fragment;
        smp_entry.type       = GPU::EntryObjectType::sampler;
        smp_entry.data.sampler.type = GPU::EntryObjectSamplerType::filtering;

        GPU::BindGroupLayoutDescriptor desc{};
        desc.entries = { tex_entry, smp_entry };
        quad_bgl_ = device_->createBindGroupLayout(desc);
    }


    // Linear sampler
    {
        GPU::SamplerDescriptor desc{};
        desc.magFilter = GPU::FilterMode::fmLinear;
        desc.minFilter = GPU::FilterMode::fmLinear;
        desc.addressModeU = GPU::WrapMode::clampToEdge;
        desc.addressModeV = GPU::WrapMode::clampToEdge;
        desc.addressModeW = GPU::WrapMode::clampToEdge;
        desc.lodMinClamp = 0.0;
        desc.lodMaxClamp = 1000.0;
        desc.maxAnisotropy = 1.0;
        linear_sampler_ = device_->createSampler(desc);
    }

    // Text rasterizer (platform-specific: JNI on Android, FreeType+HarfBuzz on Linux)
    text_rasterizer_ = createPlatformTextRasterizer();

    // Shared blend state
    auto blend = premultipliedAlphaBlend();

    // Pipeline layout: no bind groups — RectUniforms rides a push constant
    // instead (rect + colored_quad pipelines, both use rect_layout_. See
    // the removed uniforms_bgl_'s replacement comment above for why).
    // Stored as a member so the VkPipelineLayout stays valid for the
    // lifetime of the backend (vkCmdPushConstants requires it).
    {
        GPU::PipelineLayoutDescriptor desc{};
        desc.pushConstantRanges = { {
            static_cast<GPU::ShaderStage>(
                static_cast<int>(GPU::ShaderStage::vertex) |
                static_cast<int>(GPU::ShaderStage::fragment)),
            0, sizeof(RectUniforms) } };
        rect_layout_ = device_->createPipelineLayout(desc);
    }

    // Pipeline layout: no bind groups — RRectUniforms (same 48-byte size
    // as RectUniforms) rides a push constant instead (rrect pipeline).
    {
        GPU::PipelineLayoutDescriptor desc{};
        desc.pushConstantRanges = { {
            static_cast<GPU::ShaderStage>(
                static_cast<int>(GPU::ShaderStage::vertex) |
                static_cast<int>(GPU::ShaderStage::fragment)),
            0, sizeof(RRectUniforms) } };
        rrect_layout_ = device_->createPipelineLayout(desc);
    }

    // Pipeline layout: Set 0 = texture+sampler (quad/clip_shape/blur
    // pipelines, all share this layout). Push-constant range sized to the
    // largest of the three vertex-stage-only uniform structs sharing it
    // (QuadUniforms=16, ClipShapeUniforms=32, BlurUniforms=64 bytes) — a
    // smaller struct simply uses a prefix of the same range; each shader
    // only reads the fields it declares.
    {
        GPU::PipelineLayoutDescriptor desc{};
        desc.bindGroupLayouts   = { quad_bgl_ };
        desc.pushConstantRanges = { { GPU::ShaderStage::vertex, 0, sizeof(BlurUniforms) } };
        quad_layout_ = device_->createPipelineLayout(desc);
    }

    // Rect pipeline
    {
        GPU::RenderPipelineDescriptor desc{};
        desc.layout      = rect_layout_;
        desc.topology    = GPU::PrimitiveTopology::triangleList;
        desc.cullMode    = GPU::CullMode::none;
        desc.frontFace   = GPU::FrontFace::ccw;

        desc.vertex.module     = rect_vert;
        desc.vertex.entryPoint = "main";

        GPU::FragmentDescriptor frag{};
        frag.module     = rect_frag;
        frag.entryPoint = "main";
        frag.targets    = { GPU::ColorState{ pixel_format_, GPU::ColorWrite::all, blend } };
        desc.fragment   = frag;

        rect_pipeline_ = device_->createRenderPipeline(desc);
        if (!rect_pipeline_) std::fprintf(stderr, "[VulkanDrawBackend] rect_pipeline_ creation FAILED\n");
    }

    // Colored-quad pipeline — per-vertex (x,y,w) positions, uniform color.
    // Used by drawRect(fill) to render genuinely rotated/transformed quads
    // without collapsing to an AABB. Reuses rect_layout_ (same BGL) and rect_frag.
    {
        // Vertex layout: location 0 = vec3 (x,y,w), stride 12 bytes.
        GPU::VertexLayout cq_vtx_layout{};
        cq_vtx_layout.arrayStride = 3 * sizeof(float);
        cq_vtx_layout.stepMode    = GPU::StepMode::vertex;
        {
            GPU::VertexAttribute posAttr{};
            posAttr.componentType  = GPU::ComponentType::ctFloat;
            posAttr.accessorType   = GPU::AccessorType::acVec3;
            posAttr.offset         = 0;
            posAttr.shaderLocation = 0;
            cq_vtx_layout.attributes = { posAttr };
        }

        GPU::RenderPipelineDescriptor desc{};
        desc.layout      = rect_layout_;
        desc.topology    = GPU::PrimitiveTopology::triangleList;
        desc.cullMode    = GPU::CullMode::none;
        desc.frontFace   = GPU::FrontFace::ccw;

        desc.vertex.module     = colored_quad_vert;
        desc.vertex.entryPoint = "main";
        desc.vertex.buffers    = { cq_vtx_layout };

        GPU::FragmentDescriptor frag{};
        frag.module     = rect_frag;
        frag.entryPoint = "main";
        frag.targets    = { GPU::ColorState{ pixel_format_, GPU::ColorWrite::all, blend } };
        desc.fragment   = frag;

        colored_quad_pipeline_ = device_->createRenderPipeline(desc);
        if (!colored_quad_pipeline_) std::fprintf(stderr, "[VulkanDrawBackend] colored_quad_pipeline_ creation FAILED\n");
    }

    // RRect pipeline (SDF rounded rectangle)
    {
        GPU::RenderPipelineDescriptor desc{};
        desc.layout      = rrect_layout_;
        desc.topology    = GPU::PrimitiveTopology::triangleList;
        desc.cullMode    = GPU::CullMode::none;
        desc.frontFace   = GPU::FrontFace::ccw;

        desc.vertex.module     = rrect_vert;
        desc.vertex.entryPoint = "main";

        GPU::FragmentDescriptor frag{};
        frag.module     = rrect_frag;
        frag.entryPoint = "main";
        frag.targets    = { GPU::ColorState{ pixel_format_, GPU::ColorWrite::all, blend } };
        desc.fragment   = frag;

        rrect_pipeline_ = device_->createRenderPipeline(desc);
        if (!rrect_pipeline_) std::fprintf(stderr, "[VulkanDrawBackend] rrect_pipeline_ creation FAILED\n");
    }

    // Vertex layout shared by the quad and clip_shape pipelines:
    // binding 0, stride = sizeof(QuadVertex) = 20 bytes, per-vertex.
    //   location 0: vec3 (x, y, w) at offset 0
    //   location 1: vec2 (u, v)    at offset 12
    GPU::VertexLayout quad_vtx_layout{};
    quad_vtx_layout.arrayStride = sizeof(QuadVertex);
    quad_vtx_layout.stepMode    = GPU::StepMode::vertex;
    {
        GPU::VertexAttribute posAttr{};
        posAttr.componentType  = GPU::ComponentType::ctFloat;
        posAttr.accessorType   = GPU::AccessorType::acVec3;
        posAttr.offset         = offsetof(QuadVertex, x);
        posAttr.shaderLocation = 0;
        GPU::VertexAttribute uvAttr{};
        uvAttr.componentType   = GPU::ComponentType::ctFloat;
        uvAttr.accessorType    = GPU::AccessorType::acVec2;
        uvAttr.offset          = offsetof(QuadVertex, u);
        uvAttr.shaderLocation  = 1;
        quad_vtx_layout.attributes = { posAttr, uvAttr };
    }

    // Quad pipeline
    {
        GPU::RenderPipelineDescriptor desc{};
        desc.layout      = quad_layout_;
        desc.topology    = GPU::PrimitiveTopology::triangleList;
        desc.cullMode    = GPU::CullMode::none;
        desc.frontFace   = GPU::FrontFace::ccw;

        desc.vertex.module     = quad_vert;
        desc.vertex.entryPoint = "main";
        desc.vertex.buffers    = { quad_vtx_layout };

        GPU::FragmentDescriptor frag{};
        frag.module     = quad_frag;
        frag.entryPoint = "main";
        frag.targets    = { GPU::ColorState{ pixel_format_, GPU::ColorWrite::all, blend } };
        desc.fragment   = frag;

        quad_pipeline_ = device_->createRenderPipeline(desc);
        if (!quad_pipeline_) std::fprintf(stderr, "[VulkanDrawBackend] quad_pipeline_ creation FAILED\n");
    }

    // ClipShape pipeline — composites ClipRRect/ClipOval offscreen child
    // texture back to the main pass through an SDF rounded-rect/oval mask.
    // Reuses quad_layout_ (same BGL: texture@1, sampler@2; uniform data rides a push constant).
    {
        auto cs_vert = loadSpv(device_, shaders::kclip_shape_vert_spv,  shaders::kclip_shape_vert_spvSize);
        auto cs_frag = loadSpv(device_, shaders::kclip_shape_frag_spv,  shaders::kclip_shape_frag_spvSize);
        if (cs_vert && cs_frag)
        {
            GPU::RenderPipelineDescriptor desc{};
            desc.layout      = quad_layout_;
            desc.topology    = GPU::PrimitiveTopology::triangleList;
            desc.cullMode    = GPU::CullMode::none;
            desc.frontFace   = GPU::FrontFace::ccw;

            desc.vertex.module     = cs_vert;
            desc.vertex.entryPoint = "main";
            desc.vertex.buffers    = { quad_vtx_layout };

            GPU::FragmentDescriptor frag{};
            frag.module     = cs_frag;
            frag.entryPoint = "main";
            frag.targets    = { GPU::ColorState{ pixel_format_, GPU::ColorWrite::all, blend } };
            desc.fragment   = frag;

            clip_shape_pipeline_ = device_->createRenderPipeline(desc);
        }
    }

    // Blur pipeline — separable Gaussian blur (H pass then V pass).
    // Uses gl_VertexIndex to generate corners (no vertex buffer needed).
    // Reuses quad_layout_ (same BGL: texture@1, sampler@2; uniform data rides a push constant).
    {
        auto bv = loadSpv(device_, shaders::kblur_vert_spv, shaders::kblur_vert_spvSize);
        auto bf = loadSpv(device_, shaders::kblur_frag_spv, shaders::kblur_frag_spvSize);
        if (bv && bf)
        {
            GPU::RenderPipelineDescriptor desc{};
            desc.layout      = quad_layout_;
            desc.topology    = GPU::PrimitiveTopology::triangleList;
            desc.cullMode    = GPU::CullMode::none;
            desc.frontFace   = GPU::FrontFace::ccw;
            desc.vertex.module     = bv;
            desc.vertex.entryPoint = "main";

            GPU::FragmentDescriptor frag{};
            frag.module     = bf;
            frag.entryPoint = "main";
            frag.targets    = { GPU::ColorState{ pixel_format_, GPU::ColorWrite::all, blend } };
            desc.fragment   = frag;

            blur_pipeline_ = device_->createRenderPipeline(desc);
        }
    }

    std::fprintf(stderr, "[VulkanDrawBackend] pipelines: rect=%d cq=%d rrect=%d quad=%d clip=%d blur=%d\n",
        !!rect_pipeline_, !!colored_quad_pipeline_, !!rrect_pipeline_,
        !!quad_pipeline_, !!clip_shape_pipeline_, !!blur_pipeline_);
}

// ---------------------------------------------------------------------------
// Scissor
// ---------------------------------------------------------------------------

void VulkanDrawBackend::applyScissor(
    const Rect& clip,
    GPU::RenderPassEncoder& encoder)
{
    // `clip` is in logical points (render-tree coordinates); vp_w_/vp_h_ and
    // the Vulkan scissor rect are physical pixels — convert before clamping,
    // mirroring Metal's applyScissor.
    // Prevents vkCmdSetScissor with extents exceeding maxViewportDimensions.
    float x  = std::max(0.0f, clip.left()   * dpr_);
    float y  = std::max(0.0f, clip.top()    * dpr_);
    float rx = std::min(clip.right()  * dpr_,  vp_w_);
    float by = std::min(clip.bottom() * dpr_, vp_h_);
    float w  = std::max(0.0f, rx - x);
    float h  = std::max(0.0f, by - y);

    // Vulkan requires extent.width >= 1 and extent.height >= 1.
    // A degenerate clip (empty intersection) maps to a 1×1 box at the origin.
    if (w < 1.0f || h < 1.0f) { x = 0.0f; y = 0.0f; w = 1.0f; h = 1.0f; }

    if (x == last_scissor_x_ && y == last_scissor_y_ &&
        w == last_scissor_w_ && h == last_scissor_h_)
    {
        return;
    }

    last_scissor_x_ = x;
    last_scissor_y_ = y;
    last_scissor_w_ = w;
    last_scissor_h_ = h;

    encoder.setScissorRect(x, y, w, h);
}

// ---------------------------------------------------------------------------
// drawRect
// ---------------------------------------------------------------------------

void VulkanDrawBackend::drawRect(
    const DrawRectCmd&               cmd,
    const Matrix4&                   transform,
    const Rect&                      clip,
    GPU::RenderPassEncoder&          encoder)
{
    if (!rect_pipeline_) return;

    namespace vm = systems::leal::vector_math;

    // Transform the four corners and use their axis-aligned bounding box.
    auto c00 = transform * vm::Vector4<float>(cmd.rect.left(),  cmd.rect.top(),    0.0f, 1.0f);
    auto c10 = transform * vm::Vector4<float>(cmd.rect.right(), cmd.rect.top(),    0.0f, 1.0f);
    auto c01 = transform * vm::Vector4<float>(cmd.rect.left(),  cmd.rect.bottom(), 0.0f, 1.0f);
    auto c11 = transform * vm::Vector4<float>(cmd.rect.right(), cmd.rect.bottom(), 0.0f, 1.0f);

    const float min_x = std::min({c00.x(), c10.x(), c01.x(), c11.x()});
    const float min_y = std::min({c00.y(), c10.y(), c01.y(), c11.y()});
    const float max_x = std::max({c00.x(), c10.x(), c01.x(), c11.x()});
    const float max_y = std::max({c00.y(), c10.y(), c01.y(), c11.y()});

    const float w = max_x - min_x;
    const float h = max_y - min_y;
    if (w <= 0.0f || h <= 0.0f) return;

    // Stroke: decompose into 4 thin filled edge rects.
    if (cmd.paint.style == PaintStyle::stroke) {
        const float sw = std::max(1.0f, cmd.paint.stroke_width);
        const struct { float x, y, w, h; } edges[4] = {
            { min_x,        min_y,        w,  sw }, // top
            { min_x,        max_y - sw,   w,  sw }, // bottom
            { min_x,        min_y,        sw, h  }, // left
            { max_x - sw,   min_y,        sw, h  }, // right
        };
        for (const auto& e : edges) {
            if (e.w <= 0.0f || e.h <= 0.0f) continue;
            RectUniforms u{};
            u.rect[0] = e.x; u.rect[1] = e.y; u.rect[2] = e.w; u.rect[3] = e.h;
            u.color[0] = cmd.paint.color.r; u.color[1] = cmd.paint.color.g;
            u.color[2] = cmd.paint.color.b; u.color[3] = cmd.paint.color.a;
            u.viewport[0] = vp_w_; u.viewport[1] = vp_h_;
            applyScissor(clip, encoder);
            encoder.setPipeline(rect_pipeline_);
            encoder.setPushConstants(
                static_cast<GPU::ShaderStage>(
                    static_cast<int>(GPU::ShaderStage::vertex) |
                    static_cast<int>(GPU::ShaderStage::fragment)),
                0, sizeof(RectUniforms), &u);
            encoder.draw(6);
        }
        return;
    }

    // Fill: use the colored_quad_pipeline_ with actual per-vertex corner positions
    // so that rotation/perspective transforms render as genuine quads, not AABBs.
    if (!colored_quad_pipeline_) return;

    // 6 vertices: two triangles covering the transformed quad (CCW winding)
    struct ColoredQuadVertex { float x, y, w; };
    ColoredQuadVertex verts[6] = {
        { c00.x(), c00.y(), c00.w() },  // TL
        { c10.x(), c10.y(), c10.w() },  // TR
        { c01.x(), c01.y(), c01.w() },  // BL
        { c01.x(), c01.y(), c01.w() },  // BL
        { c10.x(), c10.y(), c10.w() },  // TR
        { c11.x(), c11.y(), c11.w() },  // BR
    };
    auto vbuf = device_->createBuffer(sizeof(verts),
        static_cast<GPU::BufferUsage>(static_cast<int>(GPU::BufferUsage::vertex) |
                                      static_cast<int>(GPU::BufferUsage::copyDst)),
        verts);
    if (!vbuf) return;
    frame_buffers_.push_back(vbuf);

    // Uniform: RectUniforms layout — only color and viewport are read by the shader.
    RectUniforms u{};
    u.color[0] = cmd.paint.color.r;
    u.color[1] = cmd.paint.color.g;
    u.color[2] = cmd.paint.color.b;
    u.color[3] = cmd.paint.color.a;
    u.viewport[0] = vp_w_;
    u.viewport[1] = vp_h_;

    applyScissor(clip, encoder);
    encoder.setPipeline(colored_quad_pipeline_);
    encoder.setPushConstants(
        static_cast<GPU::ShaderStage>(
            static_cast<int>(GPU::ShaderStage::vertex) |
            static_cast<int>(GPU::ShaderStage::fragment)),
        0, sizeof(RectUniforms), &u);
    encoder.setVertexBuffer(0, vbuf);
    encoder.draw(6);
}

// ---------------------------------------------------------------------------
// drawRRect
// ---------------------------------------------------------------------------

void VulkanDrawBackend::drawRRect(
    const DrawRRectCmd&              cmd,
    const Matrix4&                   transform,
    const Rect&                      clip,
    GPU::RenderPassEncoder&          encoder)
{
    if (!rrect_pipeline_) return;

    namespace vm = systems::leal::vector_math;

    const Rect& r = cmd.rrect.rect;
    auto c00 = transform * vm::Vector4<float>(r.left(),  r.top(),    0.0f, 1.0f);
    auto c10 = transform * vm::Vector4<float>(r.right(), r.top(),    0.0f, 1.0f);
    auto c01 = transform * vm::Vector4<float>(r.left(),  r.bottom(), 0.0f, 1.0f);
    auto c11 = transform * vm::Vector4<float>(r.right(), r.bottom(), 0.0f, 1.0f);

    const float min_x = std::min({c00.x(), c10.x(), c01.x(), c11.x()});
    const float min_y = std::min({c00.y(), c10.y(), c01.y(), c11.y()});
    const float max_x = std::max({c00.x(), c10.x(), c01.x(), c11.x()});
    const float max_y = std::max({c00.y(), c10.y(), c01.y(), c11.y()});

    const float w = max_x - min_x;
    const float h = max_y - min_y;
    if (w <= 0.0f || h <= 0.0f) return;

    RRectUniforms u{};
    u.rect[0] = min_x;
    u.rect[1] = min_y;
    u.rect[2] = w;
    u.rect[3] = h;
    u.color[0] = cmd.paint.color.r;
    u.color[1] = cmd.paint.color.g;
    u.color[2] = cmd.paint.color.b;
    u.color[3] = cmd.paint.color.a;
    u.viewport[0] = vp_w_;
    u.viewport[1] = vp_h_;
    // Scale factor for radius (magnitude of x-axis under transform)
    auto tv = transform * vm::Vector4<float>(1.0f, 0.0f, 0.0f, 0.0f);
    float scale = std::sqrt(tv.x() * tv.x() + tv.y() * tv.y());

    u.radius   = std::min(cmd.rrect.radius_x, cmd.rrect.radius_y) * scale;
    u.stroke_w = (cmd.paint.style == PaintStyle::stroke) ? cmd.paint.stroke_width * scale : 0.0f;

    applyScissor(clip, encoder);
    encoder.setPipeline(rrect_pipeline_);
    encoder.setPushConstants(
        static_cast<GPU::ShaderStage>(
            static_cast<int>(GPU::ShaderStage::vertex) |
            static_cast<int>(GPU::ShaderStage::fragment)),
        0, sizeof(RRectUniforms), &u);
    encoder.draw(6);
}

// ---------------------------------------------------------------------------
// drawCircle
// ---------------------------------------------------------------------------

void VulkanDrawBackend::drawCircle(
    const DrawCircleCmd&             cmd,
    const Matrix4&                   transform,
    const Rect&                      clip,
    GPU::RenderPassEncoder&          encoder)
{
    if (!rrect_pipeline_) return;

    namespace vm = systems::leal::vector_math;

    auto tc    = transform * vm::Vector4<float>(cmd.center.x, cmd.center.y, 0.0f, 1.0f);
    auto tv    = transform * vm::Vector4<float>(1.0f, 0.0f, 0.0f, 0.0f);
    float scale = std::sqrt(tv.x() * tv.x() + tv.y() * tv.y());
    float r     = cmd.radius * scale;
    float sw    = (cmd.paint.style == PaintStyle::stroke) ? cmd.paint.stroke_width * scale : 0.0f;

    RRectUniforms u{};
    u.rect[0]    = tc.x() - r;
    u.rect[1]    = tc.y() - r;
    u.rect[2]    = r * 2.0f;
    u.rect[3]    = r * 2.0f;
    u.color[0]   = cmd.paint.color.r;
    u.color[1]   = cmd.paint.color.g;
    u.color[2]   = cmd.paint.color.b;
    u.color[3]   = cmd.paint.color.a;
    u.viewport[0] = vp_w_;
    u.viewport[1] = vp_h_;
    u.radius     = r;    // corner radius = r → perfect circle via roundedBox SDF
    u.stroke_w   = sw;

    applyScissor(clip, encoder);
    encoder.setPipeline(rrect_pipeline_);
    encoder.setPushConstants(
        static_cast<GPU::ShaderStage>(
            static_cast<int>(GPU::ShaderStage::vertex) |
            static_cast<int>(GPU::ShaderStage::fragment)),
        0, sizeof(RRectUniforms), &u);
    encoder.draw(6);
}

// ---------------------------------------------------------------------------
// drawOval
// ---------------------------------------------------------------------------

void VulkanDrawBackend::drawOval(
    const DrawOvalCmd&               cmd,
    const Matrix4&                   transform,
    const Rect&                      clip,
    GPU::RenderPassEncoder&          encoder)
{
    if (!rrect_pipeline_) return;

    namespace vm = systems::leal::vector_math;

    auto tl = transform * vm::Vector4<float>(cmd.rect.left(),  cmd.rect.top(),    0.0f, 1.0f);
    auto br = transform * vm::Vector4<float>(cmd.rect.right(), cmd.rect.bottom(), 0.0f, 1.0f);
    float w  = br.x() - tl.x();
    float h  = br.y() - tl.y();
    if (w <= 0.0f || h <= 0.0f) return;

    float sw = (cmd.paint.style == PaintStyle::stroke) ? cmd.paint.stroke_width : 0.0f;

    RRectUniforms u{};
    u.rect[0]    = tl.x();
    u.rect[1]    = tl.y();
    u.rect[2]    = w;
    u.rect[3]    = h;
    u.color[0]   = cmd.paint.color.r;
    u.color[1]   = cmd.paint.color.g;
    u.color[2]   = cmd.paint.color.b;
    u.color[3]   = cmd.paint.color.a;
    u.viewport[0] = vp_w_;
    u.viewport[1] = vp_h_;
    u.radius     = std::min(w, h) * 0.5f;  // max corner radius = ellipse approximation
    u.stroke_w   = sw;

    applyScissor(clip, encoder);
    encoder.setPipeline(rrect_pipeline_);
    encoder.setPushConstants(
        static_cast<GPU::ShaderStage>(
            static_cast<int>(GPU::ShaderStage::vertex) |
            static_cast<int>(GPU::ShaderStage::fragment)),
        0, sizeof(RRectUniforms), &u);
    encoder.draw(6);
}

// ---------------------------------------------------------------------------
// Internal textured-quad helper
// ---------------------------------------------------------------------------

std::shared_ptr<GPU::BindGroup> VulkanDrawBackend::drawTexturedQuad(
    std::shared_ptr<GPU::Texture>   texture,
    const QuadCorner&               c00,
    const QuadCorner&               c10,
    const QuadCorner&               c01,
    const QuadCorner&               c11,
    float                           opacity,
    const Rect&                     clip,
    GPU::RenderPassEncoder&         encoder,
    std::shared_ptr<GPU::BindGroup> cached_bind_group)
{
    if (!quad_pipeline_ || !quad_bgl_ || !linear_sampler_) return nullptr;
    if (!texture) return nullptr;

    // Two CCW triangles: (00,10,01), (01,10,11)
    QuadVertex verts[6] = {
        {c00.x, c00.y, c00.w, c00.u, c00.v},
        {c10.x, c10.y, c10.w, c10.u, c10.v},
        {c01.x, c01.y, c01.w, c01.u, c01.v},
        {c01.x, c01.y, c01.w, c01.u, c01.v},
        {c10.x, c10.y, c10.w, c10.u, c10.v},
        {c11.x, c11.y, c11.w, c11.u, c11.v},
    };
    auto vbuf = device_->createBuffer(sizeof(verts),
        static_cast<GPU::BufferUsage>(
            static_cast<int>(GPU::BufferUsage::vertex) |
            static_cast<int>(GPU::BufferUsage::copyDst)),
        verts);
    if (!vbuf) return cached_bind_group;
    frame_buffers_.push_back(vbuf);

    // NOT safe to honor `cached_bind_group` across frames here, even
    // though the bind group is now texture+sampler only (uniforms moved to
    // a push constant — see quad_layout_'s doc comment): every
    // createBindGroup() call allocates from the CURRENT frame-ring
    // generation's descriptor pool (descriptorPools[currentFrameGen], see
    // DeviceData::beginFrameRing() in campello_gpu), which gets wholesale
    // vkResetDescriptorPool'd roughly every 2 frames. A bind group cached
    // by Renderer::text_texture_cache_ and reused several frames later
    // ends up pointing at a descriptor slot that's since been reset and
    // reallocated to some unrelated draw — the exact cause of a real bug
    // seen here (glyph quads rendering other textures' content). A correct
    // fix needs bind groups meant for cross-frame caching to come from a
    // separate, never-reset descriptor pool; until that exists, always
    // rebuild. `cached_bind_group` is accepted for interface parity with
    // Windows/Metal (see IDrawBackend::drawTextTexture()) but intentionally
    // unused here.
    GPU::BindGroupDescriptor bg_desc{};
    bg_desc.layout  = quad_bgl_;
    bg_desc.entries = {
        { 1, texture },
        { 2, linear_sampler_ }
    };
    auto bind_group = device_->createBindGroup(bg_desc);
    if (!bind_group) return nullptr;

    QuadUniforms u{};
    u.viewport[0] = vp_w_;
    u.viewport[1] = vp_h_;
    u.opacity     = opacity;

    applyScissor(clip, encoder);
    encoder.setPipeline(quad_pipeline_);
    encoder.setBindGroup(0, bind_group);
    encoder.setPushConstants(GPU::ShaderStage::vertex, 0, sizeof(QuadUniforms), &u);
    encoder.setVertexBuffer(0, vbuf);
    encoder.draw(6);
    return bind_group;
}

// ---------------------------------------------------------------------------
// drawImage
// ---------------------------------------------------------------------------

void VulkanDrawBackend::drawImage(
    const DrawImageCmd&              cmd,
    const Matrix4&                   transform,
    const Rect&                      clip,
    GPU::RenderPassEncoder&          encoder)
{
    namespace vm = systems::leal::vector_math;

    // Transform destination corners independently — real per-vertex quad,
    // not an AABB — so rotation and perspective render correctly.
    auto c00 = transform * vm::Vector4<float>(cmd.dst_rect.left(),  cmd.dst_rect.top(),    0.0f, 1.0f);
    auto c10 = transform * vm::Vector4<float>(cmd.dst_rect.right(), cmd.dst_rect.top(),    0.0f, 1.0f);
    auto c01 = transform * vm::Vector4<float>(cmd.dst_rect.left(),  cmd.dst_rect.bottom(), 0.0f, 1.0f);
    auto c11 = transform * vm::Vector4<float>(cmd.dst_rect.right(), cmd.dst_rect.bottom(), 0.0f, 1.0f);

    const float su0 = cmd.src_rect.left(),  sv0 = cmd.src_rect.top();
    const float su1 = cmd.src_rect.right(), sv1 = cmd.src_rect.bottom();

    drawTexturedQuad(
        cmd.texture,
        {c00.x(), c00.y(), c00.w(), su0, sv0},
        {c10.x(), c10.y(), c10.w(), su1, sv0},
        {c01.x(), c01.y(), c01.w(), su0, sv1},
        {c11.x(), c11.y(), c11.w(), su1, sv1},
        cmd.opacity,
        clip,
        encoder);
}

// ---------------------------------------------------------------------------
// measureText
// ---------------------------------------------------------------------------

Size VulkanDrawBackend::measureText(const TextSpan& span) const
{
    if (text_rasterizer_ && text_rasterizer_->isAvailable()) {
        return text_rasterizer_->measure(span);
    }
    return IDrawBackend::measureText(span);
}

// ---------------------------------------------------------------------------
// rasterizeText — FreeType+HarfBuzz glyph rasterization only, no caching
// (Renderer's text_texture_cache_ owns that — see its doc comment). Unlike
// the old drawText(), does NOT push the texture into frame_textures_ — its
// lifetime is now owned by Renderer's cache entry, not this per-frame list.
// ---------------------------------------------------------------------------

std::shared_ptr<GPU::Texture> VulkanDrawBackend::rasterizeText(
    const TextSpan& span, float /*dpr*/,
    uint32_t& out_width, uint32_t& out_height)
{
    if (!text_rasterizer_ || !text_rasterizer_->isAvailable()) return nullptr;
    if (span.text.empty()) return nullptr;

    // Rasterise text to CPU bitmap
    auto bitmap = text_rasterizer_->rasterize(span);
    if (bitmap.width <= 0 || bitmap.height <= 0) return nullptr;

    // Upload to GPU texture (BGRA8)
    auto texture = device_->createTexture(
        GPU::TextureType::tt2d,
        GPU::PixelFormat::bgra8unorm,
        static_cast<uint32_t>(bitmap.width),
        static_cast<uint32_t>(bitmap.height),
        1, 1, 1,
        static_cast<GPU::TextureUsage>(
            static_cast<int>(GPU::TextureUsage::textureBinding) |
            static_cast<int>(GPU::TextureUsage::copyDst)));
    if (!texture) return nullptr;

    texture->upload(0, bitmap.pixels.size(), bitmap.pixels.data());

    out_width  = static_cast<uint32_t>(bitmap.width);
    out_height = static_cast<uint32_t>(bitmap.height);
    return texture;
}

// ---------------------------------------------------------------------------
// drawTextTexture — draws an already-rasterized text texture (from
// rasterizeText(), cached or fresh) as a quad.
// ---------------------------------------------------------------------------

std::shared_ptr<GPU::BindGroup> VulkanDrawBackend::drawTextTexture(
    std::shared_ptr<GPU::Texture>   texture,
    std::shared_ptr<GPU::BindGroup> cached_bind_group,
    uint32_t width, uint32_t height,
    const Offset&            origin,
    const Matrix4&           transform,
    const Rect&              clip,
    GPU::RenderPassEncoder&  encoder)
{
    if (!texture) return nullptr;

    // Transform origin to physical pixels. Text quads always use w=1 (no
    // perspective foreshortening on rasterized glyphs), matching Metal.
    namespace vm = systems::leal::vector_math;
    auto t_origin = transform * vm::Vector4<float>(origin.x, origin.y, 0.0f, 1.0f);
    const float x0 = t_origin.x(), y0 = t_origin.y();
    const float x1 = x0 + static_cast<float>(width);
    const float y1 = y0 + static_cast<float>(height);

    return drawTexturedQuad(
        texture,
        {x0, y0, 1.0f, 0.0f, 0.0f},
        {x1, y0, 1.0f, 1.0f, 0.0f},
        {x0, y1, 1.0f, 0.0f, 1.0f},
        {x1, y1, 1.0f, 1.0f, 1.0f},
        1.0f,   // opacity baked into texture
        clip,
        encoder,
        cached_bind_group);
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Blur support
// ---------------------------------------------------------------------------

void VulkanDrawBackend::runBlurPass(
    std::shared_ptr<GPU::Texture> src,
    std::shared_ptr<GPU::Texture> dst,
    float sigma, bool horizontal,
    GPU::CommandEncoder& encoder)
{
    if (!blur_pipeline_ || !quad_bgl_ || !linear_sampler_ || !src || !dst) return;

    const uint32_t tw = static_cast<uint32_t>(dst->getWidth());
    const uint32_t th = static_cast<uint32_t>(dst->getHeight());

    auto dst_view = dst->createView(pixel_format_, 1);
    if (!dst_view) return;
    frame_views_.push_back(dst_view);

    GPU::ColorAttachment ca{};
    ca.view          = dst_view;
    ca.loadOp        = GPU::LoadOp::clear;
    ca.storeOp       = GPU::StoreOp::store;
    ca.clearValue[0] = 0.0f; ca.clearValue[1] = 0.0f;
    ca.clearValue[2] = 0.0f; ca.clearValue[3] = 0.0f;

    GPU::BeginRenderPassDescriptor desc{};
    desc.colorAttachments = {ca};
    auto rpe = encoder.beginRenderPass(desc);
    if (!rpe) return;

    BlurUniforms u{};
    u.dstRect[0]  = 0.0f; u.dstRect[1] = 0.0f;
    u.dstRect[2]  = static_cast<float>(tw);
    u.dstRect[3]  = static_cast<float>(th);
    u.srcRect[0]  = 0.0f; u.srcRect[1] = 0.0f;
    u.srcRect[2]  = 1.0f; u.srcRect[3] = 1.0f;
    u.viewport[0] = static_cast<float>(tw);
    u.viewport[1] = static_cast<float>(th);
    u.sigma       = sigma;
    u.horizontal  = horizontal ? 1.0f : 0.0f;
    u.tex_size[0] = static_cast<float>(src->getWidth());
    u.tex_size[1] = static_cast<float>(src->getHeight());

    GPU::BindGroupDescriptor bg{};
    bg.layout  = quad_bgl_;
    bg.entries = {
        { 1, src },
        { 2, linear_sampler_ }
    };
    auto bind_group = device_->createBindGroup(bg);
    if (!bind_group) { rpe->end(); return; }

    rpe->setViewport(0.0f, 0.0f, static_cast<float>(tw), static_cast<float>(th), 0.0f, 1.0f);
    rpe->setPipeline(blur_pipeline_);
    rpe->setBindGroup(0, bind_group);
    rpe->setPushConstants(GPU::ShaderStage::vertex, 0, sizeof(BlurUniforms), &u);
    rpe->draw(6);
    rpe->end();
}

std::shared_ptr<GPU::Texture> VulkanDrawBackend::blurTexture(
    std::shared_ptr<GPU::Texture> source,
    float sigma_x, float sigma_y,
    GPU::CommandEncoder& encoder)
{
    if (!source || !blur_pipeline_) return nullptr;

    const uint32_t tw = static_cast<uint32_t>(source->getWidth());
    const uint32_t th = static_cast<uint32_t>(source->getHeight());

    auto blurUsage = static_cast<GPU::TextureUsage>(
        static_cast<int>(GPU::TextureUsage::renderTarget) |
        static_cast<int>(GPU::TextureUsage::textureBinding));

    if (!blur_h_tex_ || blur_tex_w_ != tw || blur_tex_h_ != th)
    {
        // blur_h_tex_/blur_v_tex_ are reused across calls within a frame
        // purely as a size-match fast path (skip reallocating when
        // consecutive shadows/blurs happen to be the same size) — but nothing
        // about that reuse is safe to treat as "this texture is done with"
        // the moment a *different*-sized call comes along. Every draw this
        // frame shares one command buffer submitted once at frame end, so
        // an earlier call's composite draw (referencing the texture we're
        // about to overwrite here) is still unsubmitted, not just "still
        // rendering" — reassigning shared_ptr members straight to fresh
        // textures would drop the last reference and run ~Texture()
        // (vkDestroyImage, synchronous, no GPU-fence wait) on a resource an
        // already-recorded-but-not-yet-submitted draw still points to.
        // Moving the old ones into frame_textures_ first keeps them alive
        // exactly as long as any other per-frame offscreen resource.
        if (blur_h_tex_) frame_textures_.push_back(std::move(blur_h_tex_));
        if (blur_v_tex_) frame_textures_.push_back(std::move(blur_v_tex_));

        blur_h_tex_ = device_->createTexture(GPU::TextureType::tt2d, pixel_format_,
                                              tw, th, 1, 1, 1, blurUsage);
        blur_v_tex_ = device_->createTexture(GPU::TextureType::tt2d, pixel_format_,
                                              tw, th, 1, 1, 1, blurUsage);
        blur_tex_w_ = tw;
        blur_tex_h_ = th;
    }

    runBlurPass(source,      blur_h_tex_, sigma_x, /*horizontal=*/true,  encoder);
    runBlurPass(blur_h_tex_, blur_v_tex_, sigma_y, /*horizontal=*/false, encoder);

    return blur_v_tex_;
}

void VulkanDrawBackend::drawBackdropFilter(
    const DrawBackdropFilterBeginCmd&      cmd,
    std::shared_ptr<GPU::Texture>          blurred_source,
    const Matrix4&                         transform,
    const Rect&                            clip,
    GPU::RenderPassEncoder&                encoder)
{
    if (!blurred_source) return;
    namespace vm = systems::leal::vector_math;

    auto c00 = transform * vm::Vector4<float>(cmd.bounds.left(),  cmd.bounds.top(),    0.0f, 1.0f);
    auto c10 = transform * vm::Vector4<float>(cmd.bounds.right(), cmd.bounds.top(),    0.0f, 1.0f);
    auto c01 = transform * vm::Vector4<float>(cmd.bounds.left(),  cmd.bounds.bottom(), 0.0f, 1.0f);
    auto c11 = transform * vm::Vector4<float>(cmd.bounds.right(), cmd.bounds.bottom(), 0.0f, 1.0f);

    const float src_w = static_cast<float>(blurred_source->getWidth());
    const float src_h = static_cast<float>(blurred_source->getHeight());

    // UV for each corner: the blurred source is a full-viewport capture, so
    // the UV is the corner's screen position normalised by the source texture.
    auto uv = [&](const vm::Vector4<float>& c) {
        return std::make_pair(c.x() / c.w() / src_w, c.y() / c.w() / src_h);
    };
    auto [u00x, u00y] = uv(c00);
    auto [u10x, u10y] = uv(c10);
    auto [u01x, u01y] = uv(c01);
    auto [u11x, u11y] = uv(c11);

    drawTexturedQuad(
        blurred_source,
        {c00.x(), c00.y(), c00.w(), u00x, u00y},
        {c10.x(), c10.y(), c10.w(), u10x, u10y},
        {c01.x(), c01.y(), c01.w(), u01x, u01y},
        {c11.x(), c11.y(), c11.w(), u11x, u11y},
        1.0f, clip, encoder);
}

// Offscreen compositing support
// ---------------------------------------------------------------------------

std::shared_ptr<GPU::Texture> VulkanDrawBackend::createOffscreenTexture(
    uint32_t width, uint32_t height)
{
    auto tex = device_->createTexture(
        GPU::TextureType::tt2d,
        pixel_format_,
        width, height, 1, 1, 1,
        static_cast<GPU::TextureUsage>(
            static_cast<int>(GPU::TextureUsage::renderTarget) |
            static_cast<int>(GPU::TextureUsage::textureBinding)));
    if (tex)
        frame_textures_.push_back(tex);
    return tex;
}

std::shared_ptr<GPU::RenderPassEncoder> VulkanDrawBackend::beginOffscreenPass(
    std::shared_ptr<GPU::Texture> tex,
    GPU::CommandEncoder&          encoder)
{
    if (!tex) return nullptr;
    auto view = tex->createView(pixel_format_, 1);
    // Keep the view alive until the frame is submitted: vkCmdBeginRenderingKHR
    // records the raw VkImageView and it must not be destroyed before
    // vkQueueSubmit — and, since Device::submit() no longer blocks until the
    // GPU is done, not until it's actually finished executing either.
    // setViewport()'s doc comment covers the two-generation defer that
    // keeps this (and frame_buffers_/frame_textures_) alive long enough.
    frame_views_.push_back(view);

    GPU::ColorAttachment ca{};
    ca.view          = view;
    ca.loadOp        = GPU::LoadOp::clear;
    ca.storeOp       = GPU::StoreOp::store;
    ca.clearValue[0] = 0.0f;
    ca.clearValue[1] = 0.0f;
    ca.clearValue[2] = 0.0f;
    ca.clearValue[3] = 0.0f;

    GPU::BeginRenderPassDescriptor desc{};
    desc.colorAttachments = {ca};
    return encoder.beginRenderPass(desc);
}

void VulkanDrawBackend::drawClipShapeComposite(
    std::shared_ptr<GPU::Texture> child_tex,
    const Rect&                   bounds,
    float                         corner_radius,
    bool                          is_oval,
    const Matrix4&                transform,
    const Rect&                   clip,
    GPU::RenderPassEncoder&       encoder)
{
    if (!clip_shape_pipeline_ || !quad_bgl_ || !linear_sampler_) return;
    if (!child_tex) return;

    namespace vm = systems::leal::vector_math;

    // Transform all four corners independently — real per-vertex quad, not AABB.
    auto c00 = transform * vm::Vector4<float>(bounds.left(),  bounds.top(),    0.0f, 1.0f);
    auto c10 = transform * vm::Vector4<float>(bounds.right(), bounds.top(),    0.0f, 1.0f);
    auto c01 = transform * vm::Vector4<float>(bounds.left(),  bounds.bottom(), 0.0f, 1.0f);
    auto c11 = transform * vm::Vector4<float>(bounds.right(), bounds.bottom(), 0.0f, 1.0f);

    QuadVertex verts[6] = {
        {c00.x(), c00.y(), c00.w(), 0.0f, 0.0f},
        {c10.x(), c10.y(), c10.w(), 1.0f, 0.0f},
        {c01.x(), c01.y(), c01.w(), 0.0f, 1.0f},
        {c01.x(), c01.y(), c01.w(), 0.0f, 1.0f},
        {c10.x(), c10.y(), c10.w(), 1.0f, 0.0f},
        {c11.x(), c11.y(), c11.w(), 1.0f, 1.0f},
    };
    auto vbuf = device_->createBuffer(sizeof(verts),
        static_cast<GPU::BufferUsage>(
            static_cast<int>(GPU::BufferUsage::vertex) |
            static_cast<int>(GPU::BufferUsage::copyDst)),
        verts);
    if (!vbuf) return;
    frame_buffers_.push_back(vbuf);

    ClipShapeUniforms u{};
    u.rect_size[0] = bounds.width;
    u.rect_size[1] = bounds.height;
    u.viewport[0]  = vp_w_;
    u.viewport[1]  = vp_h_;
    u.corner_r     = corner_radius;
    u.kind         = is_oval ? 1.0f : 0.0f;

    GPU::BindGroupDescriptor bg_desc{};
    bg_desc.layout = quad_bgl_;
    bg_desc.entries = {
        { 1, child_tex },
        { 2, linear_sampler_ }
    };
    auto bind_group = device_->createBindGroup(bg_desc);
    if (!bind_group) return;

    applyScissor(clip, encoder);
    encoder.setPipeline(clip_shape_pipeline_);
    encoder.setBindGroup(0, bind_group);
    encoder.setPushConstants(GPU::ShaderStage::vertex, 0, sizeof(ClipShapeUniforms), &u);
    encoder.setVertexBuffer(0, vbuf);
    encoder.draw(6);
}

} // namespace systems::leal::campello_widgets
