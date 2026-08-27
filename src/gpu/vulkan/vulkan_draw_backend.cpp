#include "vulkan_draw_backend.hpp"
#include "gpu/path_tessellation.hpp"
#include "gpu/stroke_geometry.hpp"
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
#include <map>
#include <numeric>
#include <optional>

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

struct alignas(16) LineUniforms
{
    float p1[4];        // xy: start (pixels), zw: unused
    float p2[4];        // xy: end   (pixels), zw: unused
    float color[4];     // r, g, b, a
    float viewport[2];  // w, h
    float stroke_w;     // line thickness (pixels)
    float _pad;
};

struct alignas(16) QuadUniforms
{
    float viewport[2];  // w, h (physical pixels)
    float opacity;
    float _pad;
};

// Mirrors icon.vert/icon.frag's IconUniforms field-for-field. Visible to
// both stages (see icon_layout_) since icon.frag reads `tint`.
struct alignas(16) IconUniforms
{
    float viewport[2];  // w, h (physical pixels)
    float opacity;
    float _pad;
    float tint[4];      // straight-alpha RGBA recolor
};

// Push constants for the axis-aligned fast path — no vertex buffer needed.
struct alignas(16) QuadAAUniforms
{
    float viewport[2];  // physical pixel size
    float opacity;
    float _pad;
    float pos[4];       // pixel-space rect: x, y, w, h
    float uv[4];        // texture UV rect: u0, v0, u1, v1
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

struct alignas(16) ShaderMaskUniforms
{
    float viewport[2];    // framebuffer width, height
    float gradient_type;  // 0 = linear, 1 = radial, 2 = sweep
    float tile_mode;      // 0 = clamp, 1 = repeated, 2 = mirror
    float gradient_p1[4]; // linear: begin.xy; radial/sweep: center.xy (pixels)
    float gradient_p2[4]; // linear: end.xy; radial: radius in .x; sweep: start/end angle in .xy (radians)
    float blend_mode;     // 0 = srcIn, 1 = modulate
    float _pad1[3];
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

VulkanDrawBackend::~VulkanDrawBackend()
{
    // Device::submit() is pipelined (kFramesInFlight-deep, doesn't block —
    // see its doc comment in campello_gpu/src/vulkan/device.cpp), so the
    // last couple of submitted frames' command buffers may still be
    // executing on the GPU when this destructor runs. Every member below
    // (pipelines, samplers, ...) destroys its underlying Vulkan handle
    // immediately and unconditionally, with no in-flight check — unlike
    // Texture/TextureView, which defer via DeviceData::PendingTextureDestroy.
    // Wait for the GPU to finish first so those immediate destroys are safe,
    // matching what Device::~Device() itself does before its own teardown.
    if (device_)
        device_->waitForIdle();
}

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

// Builds a BlendState whose color and alpha components both use the same
// (srcFactor, dstFactor) pair — valid for premultiplied-alpha Porter-Duff
// compositing, since substituting the alpha channel's own value for "color"
// reproduces the standard `Cr = Cs*Fs + Cd*Fd` formula for both channels.
static GPU::BlendState porterDuffBlend(GPU::BlendFactor srcFactor, GPU::BlendFactor dstFactor)
{
    GPU::BlendState bs{};
    bs.color = { srcFactor, dstFactor, GPU::BlendOperation::add };
    bs.alpha = { srcFactor, dstFactor, GPU::BlendOperation::add };
    return bs;
}

static std::optional<GPU::BlendState> blendStateForBlendMode(BlendMode mode)
{
    using BF = GPU::BlendFactor;
    switch (mode) {
        case BlendMode::srcOver:
            return premultipliedAlphaBlend();
        case BlendMode::modulate: {
            GPU::BlendState bs{};
            bs.color = { GPU::BlendFactor::dstColor, GPU::BlendFactor::zero, GPU::BlendOperation::add };
            bs.alpha = { GPU::BlendFactor::dstAlpha, GPU::BlendFactor::zero, GPU::BlendOperation::add };
            return bs;
        }
        case BlendMode::plus:
            return porterDuffBlend(BF::one, BF::one);

        // Standard Porter-Duff compositing operators (premultiplied alpha).
        // See https://www.w3.org/TR/compositing-1/#advancedcompositing.
        case BlendMode::clear:
            return porterDuffBlend(BF::zero, BF::zero);
        case BlendMode::src:
            return porterDuffBlend(BF::one, BF::zero);
        case BlendMode::dst:
            return porterDuffBlend(BF::zero, BF::one);
        case BlendMode::dstOver:
            return porterDuffBlend(BF::oneMinusDstAlpha, BF::one);
        case BlendMode::srcIn:
            return porterDuffBlend(BF::dstAlpha, BF::zero);
        case BlendMode::dstIn:
            return porterDuffBlend(BF::zero, BF::srcAlpha);
        case BlendMode::srcOut:
            return porterDuffBlend(BF::oneMinusDstAlpha, BF::zero);
        case BlendMode::dstOut:
            return porterDuffBlend(BF::zero, BF::oneMinusSrcAlpha);
        case BlendMode::srcATop:
            return porterDuffBlend(BF::dstAlpha, BF::oneMinusSrcAlpha);
        case BlendMode::dstATop:
            return porterDuffBlend(BF::oneMinusDstAlpha, BF::srcAlpha);
        case BlendMode::xorMode:
            return porterDuffBlend(BF::oneMinusDstAlpha, BF::oneMinusSrcAlpha);

        default:
            return premultipliedAlphaBlend();
    }
}

std::shared_ptr<GPU::RenderPipeline> VulkanDrawBackend::pipelineForBlendMode(
    BlendMode mode,
    const std::shared_ptr<GPU::RenderPipeline>& base,
    const std::map<BlendMode, std::shared_ptr<GPU::RenderPipeline>>& variants) const
{
    if (mode == BlendMode::srcOver) return base;
    auto it = variants.find(mode);
    return (it != variants.end() && it->second) ? it->second : base;
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
    auto line_vert         = loadSpv(device_, shaders::kline_vert_spv,         shaders::kline_vert_spvSize);
    auto line_frag         = loadSpv(device_, shaders::kline_frag_spv,         shaders::kline_frag_spvSize);
    auto quad_vert         = loadSpv(device_, shaders::kquad_vert_spv,         shaders::kquad_vert_spvSize);
    auto quad_frag         = loadSpv(device_, shaders::kquad_frag_spv,         shaders::kquad_frag_spvSize);
    auto shader_mask_vert  = loadSpv(device_, shaders::kshader_mask_vert_spv,  shaders::kshader_mask_vert_spvSize);
    auto shader_mask_frag  = loadSpv(device_, shaders::kshader_mask_frag_spv,  shaders::kshader_mask_frag_spvSize);

    if (!rect_vert || !rect_frag || !colored_quad_vert || !rrect_vert || !rrect_frag ||
        !line_vert || !line_frag || !quad_vert || !quad_frag ||
        !shader_mask_vert || !shader_mask_frag) {
        std::fprintf(stderr, "[VulkanDrawBackend] shader load failed: rect_v=%d rect_f=%d cq_v=%d rr_v=%d rr_f=%d line_v=%d line_f=%d q_v=%d q_f=%d sm_v=%d sm_f=%d\n",
            !!rect_vert, !!rect_frag, !!colored_quad_vert, !!rrect_vert, !!rrect_frag,
            !!line_vert, !!line_frag, !!quad_vert, !!quad_frag,
            !!shader_mask_vert, !!shader_mask_frag);
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

    // Bind group layout for ShaderMask: child texture @ 1, gradient LUT @ 2,
    // sampler @ 3 (fragment). Reuses the same push-constant uniform path as
    // clip_shape for ShaderMaskUniforms.
    {
        GPU::BindGroupLayoutDescriptor desc{};

        GPU::EntryObject child_tex{};
        child_tex.binding    = 1;
        child_tex.visibility = GPU::ShaderStage::fragment;
        child_tex.type       = GPU::EntryObjectType::texture;
        child_tex.data.texture.multisampled  = false;
        child_tex.data.texture.sampleType    = GPU::EntryObjectTextureType::ttFloat;
        child_tex.data.texture.viewDimension = GPU::TextureType::tt2d;

        GPU::EntryObject lut_tex = child_tex;
        lut_tex.binding = 2;

        GPU::EntryObject smp_entry{};
        smp_entry.binding    = 3;
        smp_entry.visibility = GPU::ShaderStage::fragment;
        smp_entry.type       = GPU::EntryObjectType::sampler;
        smp_entry.data.sampler.type = GPU::EntryObjectSamplerType::filtering;

        desc.entries = { child_tex, lut_tex, smp_entry };
        shader_mask_bgl_ = device_->createBindGroupLayout(desc);
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

    // Nearest-neighbor sampler — used only by drawImage()/drawTintedImage()
    // when FilterQuality::none is requested; linear_sampler_ stays the
    // shared default for every other texture-sampling draw.
    {
        GPU::SamplerDescriptor desc{};
        desc.magFilter = GPU::FilterMode::fmNearest;
        desc.minFilter = GPU::FilterMode::fmNearest;
        desc.addressModeU = GPU::WrapMode::clampToEdge;
        desc.addressModeV = GPU::WrapMode::clampToEdge;
        desc.addressModeW = GPU::WrapMode::clampToEdge;
        desc.lodMinClamp = 0.0;
        desc.lodMaxClamp = 1000.0;
        desc.maxAnisotropy = 1.0;
        nearest_sampler_ = device_->createSampler(desc);
    }

    // Text rasterizer (platform-specific: JNI on Android, FreeType+HarfBuzz on Linux)
    text_rasterizer_ = createPlatformTextRasterizer();

    // Shared blend state
    auto blend = premultipliedAlphaBlend();

    // Helper that builds blend-mode variants of a solid-color pipeline.
    // The base pipeline is registered under BlendMode::srcOver; additional
    // pipelines are created for modes that have a fixed-function equivalent.
    auto buildBlendVariants = [&](GPU::RenderPipelineDescriptor& desc,
                                  std::map<BlendMode, std::shared_ptr<GPU::RenderPipeline>>& cache,
                                  const std::shared_ptr<GPU::RenderPipeline>& base) {
        cache[BlendMode::srcOver] = base;
        for (BlendMode mode : {
                 BlendMode::modulate,   BlendMode::plus,
                 BlendMode::clear,      BlendMode::src,       BlendMode::dst,
                 BlendMode::dstOver,    BlendMode::srcIn,     BlendMode::dstIn,
                 BlendMode::srcOut,     BlendMode::dstOut,    BlendMode::srcATop,
                 BlendMode::dstATop,    BlendMode::xorMode,
             }) {
            auto bs = blendStateForBlendMode(mode);
            if (!bs) continue;
            desc.fragment->targets[0].blend = *bs;
            auto pl = device_->createRenderPipeline(desc);
            if (pl) cache[mode] = std::move(pl);
        }
        desc.fragment->targets[0].blend = blend;
    };

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

    // Pipeline layout: no bind groups — LineUniforms (80 bytes) rides a
    // push constant instead (line pipeline).
    {
        GPU::PipelineLayoutDescriptor desc{};
        desc.pushConstantRanges = { {
            static_cast<GPU::ShaderStage>(
                static_cast<int>(GPU::ShaderStage::vertex) |
                static_cast<int>(GPU::ShaderStage::fragment)),
            0, sizeof(LineUniforms) } };
        line_layout_ = device_->createPipelineLayout(desc);
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

    // Pipeline layout for the icon pipeline: Set 0 = quad_bgl_ (same
    // texture+sampler shape), push constants are IconUniforms, visible to
    // *both* stages — unlike quad_layout_, icon.frag itself reads `tint`.
    {
        GPU::PipelineLayoutDescriptor desc{};
        desc.bindGroupLayouts   = { quad_bgl_ };
        desc.pushConstantRanges = { {
            static_cast<GPU::ShaderStage>(
                static_cast<int>(GPU::ShaderStage::vertex) |
                static_cast<int>(GPU::ShaderStage::fragment)),
            0, sizeof(IconUniforms) } };
        icon_layout_ = device_->createPipelineLayout(desc);
    }

    // Pipeline layout for ShaderMask: Set 0 = shader_mask_bgl_, push
    // constants are vertex-stage-only ShaderMaskUniforms.
    {
        GPU::PipelineLayoutDescriptor desc{};
        desc.bindGroupLayouts   = { shader_mask_bgl_ };
        desc.pushConstantRanges = { { GPU::ShaderStage::vertex, 0, sizeof(ShaderMaskUniforms) } };
        shader_mask_layout_ = device_->createPipelineLayout(desc);
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
        buildBlendVariants(desc, rect_blend_pipelines_, rect_pipeline_);
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
        buildBlendVariants(desc, colored_quad_blend_pipelines_, colored_quad_pipeline_);
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
        buildBlendVariants(desc, rrect_blend_pipelines_, rrect_pipeline_);
    }

    // Line pipeline — arbitrary-angle line segment rendered as a rotated quad.
    {
        GPU::RenderPipelineDescriptor desc{};
        desc.layout      = line_layout_;
        desc.topology    = GPU::PrimitiveTopology::triangleList;
        desc.cullMode    = GPU::CullMode::none;
        desc.frontFace   = GPU::FrontFace::ccw;

        desc.vertex.module     = line_vert;
        desc.vertex.entryPoint = "main";

        GPU::FragmentDescriptor frag{};
        frag.module     = line_frag;
        frag.entryPoint = "main";
        frag.targets    = { GPU::ColorState{ pixel_format_, GPU::ColorWrite::all, blend } };
        desc.fragment   = frag;

        line_pipeline_ = device_->createRenderPipeline(desc);
        if (!line_pipeline_) std::fprintf(stderr, "[VulkanDrawBackend] line_pipeline_ creation FAILED\n");
        buildBlendVariants(desc, line_blend_pipelines_, line_pipeline_);
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

    // Icon pipeline — tinted template images (see icon.vert/icon.frag).
    // Same vertex layout as the quad pipeline (QuadVertex: x,y,w,u,v), its
    // own pipeline layout (icon_layout_ — IconUniforms need fragment-stage
    // visibility for `tint`, unlike quad_layout_).
    {
        auto icon_vert = loadSpv(device_, shaders::kicon_vert_spv, shaders::kicon_vert_spvSize);
        auto icon_frag = loadSpv(device_, shaders::kicon_frag_spv, shaders::kicon_frag_spvSize);
        if (!icon_vert || !icon_frag) {
            std::fprintf(stderr, "[VulkanDrawBackend] icon shader module load FAILED\n");
        } else {
            GPU::RenderPipelineDescriptor desc{};
            desc.layout      = icon_layout_;
            desc.topology    = GPU::PrimitiveTopology::triangleList;
            desc.cullMode    = GPU::CullMode::none;
            desc.frontFace   = GPU::FrontFace::ccw;

            desc.vertex.module     = icon_vert;
            desc.vertex.entryPoint = "main";
            desc.vertex.buffers    = { quad_vtx_layout };

            GPU::FragmentDescriptor frag{};
            frag.module     = icon_frag;
            frag.entryPoint = "main";
            frag.targets    = { GPU::ColorState{ pixel_format_, GPU::ColorWrite::all, blend } };
            desc.fragment   = frag;

            icon_pipeline_ = device_->createRenderPipeline(desc);
            if (!icon_pipeline_) std::fprintf(stderr, "[VulkanDrawBackend] icon_pipeline_ creation FAILED\n");
        }
    }

    // Axis-aligned quad pipeline — same fragment shader as quad, but vertex
    // shader derives positions and UVs from push constants (no vertex buffer).
    // Used for all normal UI image/text draws; falls back to quad_pipeline_
    // for perspective-rotated quads where w≠1 or edges aren't grid-aligned.
    {
        auto aa_vert = loadSpv(device_, shaders::kquad_aa_vert_spv, shaders::kquad_aa_vert_spvSize);
        auto aa_frag = loadSpv(device_, shaders::kquad_frag_spv,    shaders::kquad_frag_spvSize);
        if (aa_vert && aa_frag)
        {
            GPU::RenderPipelineDescriptor desc{};
            desc.layout      = quad_layout_;
            desc.topology    = GPU::PrimitiveTopology::triangleList;
            desc.cullMode    = GPU::CullMode::none;
            desc.frontFace   = GPU::FrontFace::ccw;

            desc.vertex.module     = aa_vert;
            desc.vertex.entryPoint = "main";
            // No vertex.buffers — vertices come entirely from push constants.

            GPU::FragmentDescriptor frag{};
            frag.module     = aa_frag;
            frag.entryPoint = "main";
            frag.targets    = { GPU::ColorState{ pixel_format_, GPU::ColorWrite::all, blend } };
            desc.fragment   = frag;

            quad_aa_pipeline_ = device_->createRenderPipeline(desc);
            if (!quad_aa_pipeline_) std::fprintf(stderr, "[VulkanDrawBackend] quad_aa_pipeline_ creation FAILED\n");
        }
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

    // ShaderMask pipeline — composites a child texture with a gradient mask.
    {
        GPU::RenderPipelineDescriptor desc{};
        desc.layout      = shader_mask_layout_;
        desc.topology    = GPU::PrimitiveTopology::triangleList;
        desc.cullMode    = GPU::CullMode::none;
        desc.frontFace   = GPU::FrontFace::ccw;

        desc.vertex.module     = shader_mask_vert;
        desc.vertex.entryPoint = "main";
        desc.vertex.buffers    = { quad_vtx_layout };

        GPU::FragmentDescriptor frag{};
        frag.module     = shader_mask_frag;
        frag.entryPoint = "main";
        frag.targets    = { GPU::ColorState{ pixel_format_, GPU::ColorWrite::all, blend } };
        desc.fragment   = frag;

        shader_mask_pipeline_ = device_->createRenderPipeline(desc);
        if (!shader_mask_pipeline_) std::fprintf(stderr, "[VulkanDrawBackend] shader_mask_pipeline_ creation FAILED\n");
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

    std::fprintf(stderr, "[VulkanDrawBackend] pipelines: rect=%d cq=%d rrect=%d line=%d quad=%d quad_aa=%d clip=%d shader_mask=%d blur=%d\n",
        !!rect_pipeline_, !!colored_quad_pipeline_, !!rrect_pipeline_, !!line_pipeline_,
        !!quad_pipeline_, !!quad_aa_pipeline_, !!clip_shape_pipeline_, !!shader_mask_pipeline_, !!blur_pipeline_);
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
// UniformBufferPool — see its doc comment in vulkan_draw_backend.hpp
// ---------------------------------------------------------------------------

std::shared_ptr<GPU::Buffer> VulkanDrawBackend::UniformBufferPool::acquire(
    GPU::Device& device, uint64_t size, const void* data)
{
    auto&  buffers = generations_[current_generation_];
    size_t idx     = next_index_[current_generation_]++;

    if (idx >= buffers.size())
        // mapWrite forces Device::createBuffer() down its host-visible-only
        // allocation path (see its needsHostVisible branch) instead of
        // best-effort probing for a combined DEVICE_LOCAL|HOST_VISIBLE type
        // and silently falling back to pure DEVICE_LOCAL when the GPU
        // doesn't expose one for this buffer's memoryTypeBits. That
        // fallback makes every Buffer::upload() take its device-local path
        // — a synchronous staging-buffer copy plus vkWaitForFences(...,
        // UINT64_MAX) — which is fine for a rarely-updated buffer but,
        // called every single animation frame (this pool backs rotated/
        // perspective-quad vertex data, re-uploaded whenever the transform
        // changes), starves the render thread badly enough on at least one
        // Mali/Vulkan device (Chromecast with Google TV) to look like a
        // total freeze — confirmed absent on Adreno/UMA hardware, which
        // already had a combined memory type and so never hit this path.
        buffers.push_back(device.createBuffer(size,
            static_cast<GPU::BufferUsage>(
                static_cast<int>(GPU::BufferUsage::vertex) |
                static_cast<int>(GPU::BufferUsage::copyDst) |
                static_cast<int>(GPU::BufferUsage::mapWrite)),
            const_cast<void*>(data)));
    else
        buffers[idx]->upload(0, size, const_cast<void*>(data));

    return buffers[idx];
}

void VulkanDrawBackend::UniformBufferPool::beginFrame() noexcept
{
    current_generation_ = (current_generation_ + 1) % kGenerations;
    next_index_[current_generation_] = 0;
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

    // Stroke: the 4 corners as a closed polyline through strokePolyline() —
    // correctly handles rotation (unlike the old always-axis-aligned
    // 4-separate-rects approach) and gets real caps/joins for free. Local
    // (pre-transform) corners: strokePolyline() applies `transform` itself.
    if (cmd.paint.style == PaintStyle::stroke) {
        const std::vector<Offset> corners{
            {cmd.rect.left(),  cmd.rect.top()},
            {cmd.rect.right(), cmd.rect.top()},
            {cmd.rect.right(), cmd.rect.bottom()},
            {cmd.rect.left(),  cmd.rect.bottom()},
        };
        strokePolyline(corners, /*closed=*/true, cmd.paint, clip, transform, encoder);
        return;
    }

    // Fill: detect whether the transform is axis-aligned (translation + uniform
    // scale only — all corners have w≈1 and the edges are horizontal/vertical).
    // This is always true for normal widget-tree transforms. In that case reuse
    // rect_pipeline_ with push constants — no vertex buffer needed, matching
    // the speed of drawRRect. Only fall back to colored_quad_pipeline_ for
    // genuinely rotated or perspective-projected rects.
    const bool is_axis_aligned =
        std::abs(c00.y() - c10.y()) < 0.5f &&
        std::abs(c01.y() - c11.y()) < 0.5f &&
        std::abs(c00.x() - c01.x()) < 0.5f &&
        std::abs(c10.x() - c11.x()) < 0.5f;

    if (is_axis_aligned) {
        RectUniforms u{};
        u.rect[0] = min_x; u.rect[1] = min_y; u.rect[2] = w; u.rect[3] = h;
        u.color[0] = cmd.paint.color.r;
        u.color[1] = cmd.paint.color.g;
        u.color[2] = cmd.paint.color.b;
        u.color[3] = cmd.paint.color.a;
        u.viewport[0] = vp_w_;
        u.viewport[1] = vp_h_;

        applyScissor(clip, encoder);
        encoder.setPipeline(pipelineForBlendMode(
            cmd.paint.blend_mode, rect_pipeline_, rect_blend_pipelines_));
        encoder.setPushConstants(
            static_cast<GPU::ShaderStage>(
                static_cast<int>(GPU::ShaderStage::vertex) |
                static_cast<int>(GPU::ShaderStage::fragment)),
            0, sizeof(RectUniforms), &u);
        encoder.draw(6);
        return;
    }

    // Rotated / perspective quad — upload per-vertex projected positions.
    if (!colored_quad_pipeline_) return;

    ColoredQuadVertex verts[6] = {
        { c00.x(), c00.y(), c00.w() },
        { c10.x(), c10.y(), c10.w() },
        { c01.x(), c01.y(), c01.w() },
        { c01.x(), c01.y(), c01.w() },
        { c10.x(), c10.y(), c10.w() },
        { c11.x(), c11.y(), c11.w() },
    };
    auto vbuf = colored_quad_vertex_pool_.acquire(*device_, sizeof(verts), verts);
    if (!vbuf) return;

    RectUniforms u{};
    u.color[0] = cmd.paint.color.r;
    u.color[1] = cmd.paint.color.g;
    u.color[2] = cmd.paint.color.b;
    u.color[3] = cmd.paint.color.a;
    u.viewport[0] = vp_w_;
    u.viewport[1] = vp_h_;

    applyScissor(clip, encoder);
    encoder.setPipeline(pipelineForBlendMode(
        cmd.paint.blend_mode, colored_quad_pipeline_, colored_quad_blend_pipelines_));
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
    encoder.setPipeline(pipelineForBlendMode(
        cmd.paint.blend_mode, rrect_pipeline_, rrect_blend_pipelines_));
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
    encoder.setPipeline(pipelineForBlendMode(
        cmd.paint.blend_mode, rrect_pipeline_, rrect_blend_pipelines_));
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
    encoder.setPipeline(pipelineForBlendMode(
        cmd.paint.blend_mode, rrect_pipeline_, rrect_blend_pipelines_));
    encoder.setPushConstants(
        static_cast<GPU::ShaderStage>(
            static_cast<int>(GPU::ShaderStage::vertex) |
            static_cast<int>(GPU::ShaderStage::fragment)),
        0, sizeof(RRectUniforms), &u);
    encoder.draw(6);
}

// ---------------------------------------------------------------------------
// Stroke primitives — see this backend's header for the overall design.
// ---------------------------------------------------------------------------

void VulkanDrawBackend::drawStrokeSegmentBody(
    const Offset& p0, const Offset& p1, float half_width,
    const Paint& paint, const Matrix4& transform,
    GPU::RenderPassEncoder& encoder)
{
    if (!line_pipeline_) return;

    namespace vm = systems::leal::vector_math;

    auto tp0 = transform * vm::Vector4<float>(p0.x, p0.y, 0.0f, 1.0f);
    auto tp1 = transform * vm::Vector4<float>(p1.x, p1.y, 0.0f, 1.0f);
    auto tv  = transform * vm::Vector4<float>(1.0f, 0.0f, 0.0f, 0.0f);
    float scale = std::sqrt(tv.x() * tv.x() + tv.y() * tv.y());

    LineUniforms u{};
    u.p1[0]       = tp0.x();
    u.p1[1]       = tp0.y();
    u.p2[0]       = tp1.x();
    u.p2[1]       = tp1.y();
    u.color[0]    = paint.color.r;
    u.color[1]    = paint.color.g;
    u.color[2]    = paint.color.b;
    u.color[3]    = paint.color.a;
    u.viewport[0] = vp_w_;
    u.viewport[1] = vp_h_;
    u.stroke_w    = std::max(1.0f, half_width * 2.0f * scale);

    encoder.setPipeline(pipelineForBlendMode(
        paint.blend_mode, line_pipeline_, line_blend_pipelines_));
    encoder.setPushConstants(
        static_cast<GPU::ShaderStage>(
            static_cast<int>(GPU::ShaderStage::vertex) |
            static_cast<int>(GPU::ShaderStage::fragment)),
        0, sizeof(LineUniforms), &u);
    encoder.draw(6);
}

void VulkanDrawBackend::drawStrokeRoundPrimitive(
    const Offset& center, float half_width,
    const Paint& paint, const Matrix4& transform,
    GPU::RenderPassEncoder& encoder)
{
    if (!rrect_pipeline_) return;

    namespace vm = systems::leal::vector_math;

    auto tc = transform * vm::Vector4<float>(center.x, center.y, 0.0f, 1.0f);
    auto tv = transform * vm::Vector4<float>(1.0f, 0.0f, 0.0f, 0.0f);
    float scale = std::sqrt(tv.x() * tv.x() + tv.y() * tv.y());
    float r = half_width * scale;

    RRectUniforms u{};
    u.rect[0]     = tc.x() - r;
    u.rect[1]     = tc.y() - r;
    u.rect[2]     = r * 2.0f;
    u.rect[3]     = r * 2.0f;
    u.color[0]    = paint.color.r;
    u.color[1]    = paint.color.g;
    u.color[2]    = paint.color.b;
    u.color[3]    = paint.color.a;
    u.viewport[0] = vp_w_;
    u.viewport[1] = vp_h_;
    u.radius      = r; // corner radius = r -> perfect circle via roundedBox SDF
    u.stroke_w    = 0.0f; // fill

    encoder.setPipeline(pipelineForBlendMode(
        paint.blend_mode, rrect_pipeline_, rrect_blend_pipelines_));
    encoder.setPushConstants(
        static_cast<GPU::ShaderStage>(
            static_cast<int>(GPU::ShaderStage::vertex) |
            static_cast<int>(GPU::ShaderStage::fragment)),
        0, sizeof(RRectUniforms), &u);
    encoder.draw(6);
}

void VulkanDrawBackend::strokePolyline(
    const std::vector<Offset>& points, bool closed,
    const Paint& paint, const Rect& clip, const Matrix4& transform,
    GPU::RenderPassEncoder& encoder)
{
    applyScissor(clip, encoder);

    const float half_width = std::max(0.5f, paint.stroke_width * 0.5f);
    auto geo = buildStrokeGeometry(points, closed, half_width,
                                    paint.stroke_cap, paint.stroke_join, paint.stroke_miter_limit);

    for (const auto& seg : geo.segments)
        drawStrokeSegmentBody(seg.p0, seg.p1, half_width, paint, transform, encoder);

    for (const auto& c : geo.circles)
        drawStrokeRoundPrimitive(c.center, half_width, paint, transform, encoder);

    if (!geo.wedges.empty() && colored_quad_pipeline_)
    {
        namespace vm = systems::leal::vector_math;
        std::vector<ColoredQuadVertex> verts;
        verts.reserve(geo.wedges.size() * 6);
        auto pushPt = [&](const Offset& o) {
            auto p = transform * vm::Vector4<float>(o.x, o.y, 0.0f, 1.0f);
            verts.push_back({p.x(), p.y(), p.w()});
        };
        for (const auto& w : geo.wedges)
        {
            if (w.has_miter_point)
            {
                pushPt(w.hub); pushPt(w.outer_a); pushPt(w.miter_point);
                pushPt(w.hub); pushPt(w.miter_point); pushPt(w.outer_b);
            }
            else
            {
                pushPt(w.hub); pushPt(w.outer_a); pushPt(w.outer_b);
            }
        }

        auto vbuf = colored_quad_vertex_pool_.acquire(*device_, verts.size() * sizeof(ColoredQuadVertex), verts.data());
        if (vbuf)
        {
            RectUniforms u{};
            u.color[0]    = paint.color.r;
            u.color[1]    = paint.color.g;
            u.color[2]    = paint.color.b;
            u.color[3]    = paint.color.a;
            u.viewport[0] = vp_w_;
            u.viewport[1] = vp_h_;

            encoder.setPipeline(pipelineForBlendMode(
                paint.blend_mode, colored_quad_pipeline_, colored_quad_blend_pipelines_));
            encoder.setPushConstants(
                static_cast<GPU::ShaderStage>(
                    static_cast<int>(GPU::ShaderStage::vertex) |
                    static_cast<int>(GPU::ShaderStage::fragment)),
                0, sizeof(RectUniforms), &u);
            encoder.setVertexBuffer(0, vbuf);
            encoder.draw(static_cast<uint32_t>(verts.size()));
        }
    }
}

void VulkanDrawBackend::appendStrokePolylineBatched(
    const std::vector<Offset>& points, bool closed,
    const Paint& paint, const Matrix4& transform,
    std::vector<ColoredQuadVertex>& verts)
{
    namespace vm = systems::leal::vector_math;

    const float half_width = std::max(0.5f, paint.stroke_width * 0.5f);
    auto geo = buildStrokeGeometry(points, closed, half_width,
                                    paint.stroke_cap, paint.stroke_join, paint.stroke_miter_limit);

    auto pushPt = [&](const Offset& o) {
        auto p = transform * vm::Vector4<float>(o.x, o.y, 0.0f, 1.0f);
        verts.push_back({p.x(), p.y(), p.w()});
    };

    // Segment bodies: 2 triangles per segment (same shape the pre-existing
    // per-segment loop built, just now sourced from buildStrokeGeometry()).
    for (const auto& seg : geo.segments)
    {
        const float dx  = seg.p1.x - seg.p0.x;
        const float dy  = seg.p1.y - seg.p0.y;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-4f) continue;
        const float nx = -dy / len * half_width;
        const float ny =  dx / len * half_width;
        const Offset a{seg.p0.x + nx, seg.p0.y + ny};
        const Offset b{seg.p0.x - nx, seg.p0.y - ny};
        const Offset c{seg.p1.x + nx, seg.p1.y + ny};
        const Offset d{seg.p1.x - nx, seg.p1.y - ny};
        pushPt(b); pushPt(a); pushPt(d);
        pushPt(d); pushPt(a); pushPt(c);
    }

    // Round caps/joins: approximated as a small triangle fan -- no SDF
    // available in this flat/batched context (see this method's header doc).
    constexpr int kFanSegments = 12;
    for (const auto& circ : geo.circles)
    {
        for (int i = 0; i < kFanSegments; ++i)
        {
            const float a0 = (2.0f * static_cast<float>(M_PI) * i) / kFanSegments;
            const float a1 = (2.0f * static_cast<float>(M_PI) * (i + 1)) / kFanSegments;
            pushPt(circ.center);
            pushPt({circ.center.x + half_width * std::cos(a0), circ.center.y + half_width * std::sin(a0)});
            pushPt({circ.center.x + half_width * std::cos(a1), circ.center.y + half_width * std::sin(a1)});
        }
    }

    // Bevel/miter join wedges.
    for (const auto& w : geo.wedges)
    {
        if (w.has_miter_point)
        {
            pushPt(w.hub); pushPt(w.outer_a); pushPt(w.miter_point);
            pushPt(w.hub); pushPt(w.miter_point); pushPt(w.outer_b);
        }
        else
        {
            pushPt(w.hub); pushPt(w.outer_a); pushPt(w.outer_b);
        }
    }
}

// ---------------------------------------------------------------------------
// drawLine
// ---------------------------------------------------------------------------

void VulkanDrawBackend::drawLine(
    const DrawLineCmd&               cmd,
    const Matrix4&                   transform,
    const Rect&                      clip,
    GPU::RenderPassEncoder&          encoder)
{
    // A line is just a 2-point open polyline -- strokePolyline() gives it
    // real caps (butt/round/square) for free; a plain 2-point stroke has no
    // interior vertex, so stroke_join never applies here.
    strokePolyline({cmd.p1, cmd.p2}, /*closed=*/false, cmd.paint, clip, transform, encoder);
}

// ---------------------------------------------------------------------------
// drawPoints — decompose to circles/lines using existing pipelines
// ---------------------------------------------------------------------------

void VulkanDrawBackend::drawPoints(
    const DrawPointsCmd&             cmd,
    const Matrix4&                   transform,
    const Rect&                      clip,
    GPU::RenderPassEncoder&          encoder)
{
    if (cmd.points.empty()) return;

    switch (cmd.mode)
    {
        case PointMode::points:
        {
            // Use paint.stroke_width as the point diameter.
            const float radius = std::max(1.0f, cmd.paint.stroke_width * 0.5f);
            Paint p = cmd.paint;
            p.style = PaintStyle::fill;
            for (const auto& pt : cmd.points)
            {
                DrawCircleCmd circle{pt, radius, p};
                drawCircle(circle, transform, clip, encoder);
            }
            break;
        }
        case PointMode::lines:
        {
            Paint p = cmd.paint;
            p.style = PaintStyle::stroke;
            for (size_t i = 0; i + 1 < cmd.points.size(); i += 2)
            {
                DrawLineCmd line{cmd.points[i], cmd.points[i + 1], p};
                drawLine(line, transform, clip, encoder);
            }
            break;
        }
        case PointMode::polygon:
        {
            if (cmd.points.size() < 2) break;
            Paint p = cmd.paint;
            p.style = PaintStyle::stroke;
            // Real joins at every vertex, including the closing corner, via
            // strokePolyline() -- see PointMode::polygon's doc comment
            // ("Lines connecting all points in a loop") for why this is
            // closed, unlike PointMode::lines' independent segment pairs
            // below (which stay per-pair drawLine() calls -- disjoint
            // segments have no shared vertex to join).
            strokePolyline(cmd.points, /*closed=*/true, p, clip, transform, encoder);
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// drawArc — tessellate to triangles and draw via colored_quad_pipeline_
// ---------------------------------------------------------------------------

void VulkanDrawBackend::drawArc(
    const DrawArcCmd&                cmd,
    const Matrix4&                   transform,
    const Rect&                      clip,
    GPU::RenderPassEncoder&          encoder)
{
    if (!colored_quad_pipeline_) return;

    namespace vm = systems::leal::vector_math;

    const float cx = cmd.rect.x + cmd.rect.width * 0.5f;
    const float cy = cmd.rect.y + cmd.rect.height * 0.5f;
    const float rx = cmd.rect.width * 0.5f;
    const float ry = cmd.rect.height * 0.5f;
    if (rx <= 0.0f || ry <= 0.0f) return;

    // Choose segment count based on the larger radius and sweep magnitude.
    const float abs_sweep = std::abs(cmd.sweep_angle);
    const int   segments  = std::max(3, static_cast<int>(abs_sweep * 20.0f));

    std::vector<ColoredQuadVertex> verts;
    verts.reserve(static_cast<size_t>(segments) * 6);

    const bool is_stroke = (cmd.paint.style == PaintStyle::stroke);
    const float stroke_w = std::max(1.0f, cmd.paint.stroke_width);

    // Scale factor for stroke width (magnitude of x-axis under transform).
    auto tv = transform * vm::Vector4<float>(1.0f, 0.0f, 0.0f, 0.0f);
    const float scale = std::sqrt(tv.x() * tv.x() + tv.y() * tv.y());
    const float pixel_stroke = stroke_w * scale;

    for (int i = 0; i < segments; ++i)
    {
        const float t0 = cmd.start_angle + cmd.sweep_angle * (float(i) / float(segments));
        const float t1 = cmd.start_angle + cmd.sweep_angle * (float(i + 1) / float(segments));

        const float c0 = std::cos(t0), s0 = std::sin(t0);
        const float c1 = std::cos(t1), s1 = std::sin(t1);

        if (cmd.use_center)
        {
            // Pie wedge: triangles fan from center.
            const vm::Vector4<float> p_center = transform * vm::Vector4<float>(cx, cy, 0.0f, 1.0f);
            const vm::Vector4<float> p0 = transform * vm::Vector4<float>(cx + rx * c0, cy + ry * s0, 0.0f, 1.0f);
            const vm::Vector4<float> p1 = transform * vm::Vector4<float>(cx + rx * c1, cy + ry * s1, 0.0f, 1.0f);

            verts.push_back({p_center.x(), p_center.y(), p_center.w()});
            verts.push_back({p0.x(), p0.y(), p0.w()});
            verts.push_back({p1.x(), p1.y(), p1.w()});
        }
        else
        {
            // Open arc segment: quad strip with inner/outer radius.
            const auto eval = [&](float t, float r) -> vm::Vector4<float> {
                // Point on the outer oval at angle t.
                const float ox = cx + rx * std::cos(t);
                const float oy = cy + ry * std::sin(t);
                // Normalized outward normal for an oval at angle t.
                float nx = std::cos(t) / rx;
                float ny = std::sin(t) / ry;
                float nlen = std::sqrt(nx * nx + ny * ny);
                if (nlen > 0.0001f) { nx /= nlen; ny /= nlen; }
                // r is the inward offset (0 = outer edge, stroke_w = inner edge).
                return transform * vm::Vector4<float>(ox - r * nx, oy - r * ny, 0.0f, 1.0f);
            };

            // For fill, draw the full sector (inner radius = 0).
            // For stroke, draw a band inside the outer edge.
            const float inner_r = is_stroke ? pixel_stroke : 0.0f;

            const auto p00 = eval(t0, 0.0f);
            const auto p01 = eval(t0, inner_r);
            const auto p10 = eval(t1, 0.0f);
            const auto p11 = eval(t1, inner_r);

            verts.push_back({p01.x(), p01.y(), p01.w()});
            verts.push_back({p00.x(), p00.y(), p00.w()});
            verts.push_back({p11.x(), p11.y(), p11.w()});
            verts.push_back({p11.x(), p11.y(), p11.w()});
            verts.push_back({p00.x(), p00.y(), p00.w()});
            verts.push_back({p10.x(), p10.y(), p10.w()});
        }
    }

    if (verts.empty()) return;
    applyScissor(clip, encoder);

    auto vbuf = colored_quad_vertex_pool_.acquire(*device_, verts.size() * sizeof(ColoredQuadVertex), verts.data());
    if (!vbuf) return;

    // ColoredQuadUniforms rect field is unused, but the layout requires it.
    RectUniforms u{};
    u.color[0]    = cmd.paint.color.r;
    u.color[1]    = cmd.paint.color.g;
    u.color[2]    = cmd.paint.color.b;
    u.color[3]    = cmd.paint.color.a;
    u.viewport[0] = vp_w_;
    u.viewport[1] = vp_h_;

    encoder.setPipeline(pipelineForBlendMode(
        cmd.paint.blend_mode, colored_quad_pipeline_, colored_quad_blend_pipelines_));
    encoder.setPushConstants(
        static_cast<GPU::ShaderStage>(
            static_cast<int>(GPU::ShaderStage::vertex) |
            static_cast<int>(GPU::ShaderStage::fragment)),
        0, sizeof(RectUniforms), &u);
    encoder.setVertexBuffer(0, vbuf);
    encoder.draw(static_cast<uint32_t>(verts.size()));
}

// ---------------------------------------------------------------------------
// drawPath — CPU tessellation to triangles, drawn via colored_quad_pipeline_
// ---------------------------------------------------------------------------

void VulkanDrawBackend::drawPath(
    const DrawPathCmd&               cmd,
    const Matrix4&                   transform,
    const Rect&                      clip,
    GPU::RenderPassEncoder&          encoder)
{
    if (!colored_quad_pipeline_) return;

    namespace vm = systems::leal::vector_math;

    auto contours = buildPathContours(cmd.path);
    if (contours.empty()) return;

    std::vector<ColoredQuadVertex> verts;

    if (cmd.paint.style == PaintStyle::stroke)
    {
        for (const auto& contour : contours)
        {
            std::vector<Offset> pts;
            pts.reserve(contour.size());
            for (const auto& v : contour) pts.push_back({v.x, v.y});

            // buildPathContours() appends an explicit closing duplicate
            // point when the source Path has a close() command --
            // buildStrokeGeometry() expects "closed=true, N distinct
            // points" (see its doc comment), so detect and let it strip
            // the duplicate itself; just tell it whether one is present.
            bool closed = false;
            if (pts.size() > 2)
            {
                const float dx = pts.front().x - pts.back().x;
                const float dy = pts.front().y - pts.back().y;
                closed = (dx * dx + dy * dy) < 1e-6f;
            }

            appendStrokePolylineBatched(pts, closed, cmd.paint, transform, verts);
        }
    }
    else
    {
        std::vector<PathTessVertex> triangles;
        for (const auto& contour : contours)
            triangulateContour(contour, triangles);

        verts.reserve(triangles.size());
        for (const auto& v : triangles)
        {
            auto p = transform * vm::Vector4<float>(v.x, v.y, 0.0f, 1.0f);
            verts.push_back({p.x(), p.y(), p.w()});
        }
    }

    if (verts.empty()) return;

    applyScissor(clip, encoder);

    auto vbuf = colored_quad_vertex_pool_.acquire(*device_, verts.size() * sizeof(ColoredQuadVertex), verts.data());
    if (!vbuf) return;

    RectUniforms u{};
    u.color[0]    = cmd.paint.color.r;
    u.color[1]    = cmd.paint.color.g;
    u.color[2]    = cmd.paint.color.b;
    u.color[3]    = cmd.paint.color.a;
    u.viewport[0] = vp_w_;
    u.viewport[1] = vp_h_;

    encoder.setPipeline(pipelineForBlendMode(
        cmd.paint.blend_mode, colored_quad_pipeline_, colored_quad_blend_pipelines_));
    encoder.setPushConstants(
        static_cast<GPU::ShaderStage>(
            static_cast<int>(GPU::ShaderStage::vertex) |
            static_cast<int>(GPU::ShaderStage::fragment)),
        0, sizeof(RectUniforms), &u);
    encoder.setVertexBuffer(0, vbuf);
    encoder.draw(static_cast<uint32_t>(verts.size()));
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
    std::shared_ptr<GPU::BindGroup> cached_bind_group,
    bool                            persistent,
    std::shared_ptr<GPU::Sampler>   sampler)
{
    if (!quad_pipeline_ || !quad_bgl_ || !linear_sampler_) return nullptr;
    if (!texture) return nullptr;
    if (!sampler) sampler = linear_sampler_;

    std::shared_ptr<GPU::BindGroup> bind_group;
    if (cached_bind_group) {
        bind_group = cached_bind_group;
    } else {
        GPU::BindGroupDescriptor bg_desc{};
        bg_desc.layout  = quad_bgl_;
        bg_desc.entries = {
            { 1, texture },
            { 2, sampler }
        };
        bind_group = device_->createBindGroup(bg_desc, persistent);
        if (!bind_group) return nullptr;
    }

    applyScissor(clip, encoder);

    // Axis-aligned fast path: edges are horizontal/vertical, so the quad is
    // fully described by two rects (position + UV). Skip the vertex pool
    // acquire and vertex buffer bind — push constants only.
    const bool is_axis_aligned = quad_aa_pipeline_ &&
        std::abs(c00.y - c10.y) < 0.5f &&
        std::abs(c01.y - c11.y) < 0.5f &&
        std::abs(c00.x - c01.x) < 0.5f &&
        std::abs(c10.x - c11.x) < 0.5f &&
        std::abs(c00.u - c01.u) < 1e-4f &&
        std::abs(c10.u - c11.u) < 1e-4f &&
        std::abs(c00.v - c10.v) < 1e-4f &&
        std::abs(c01.v - c11.v) < 1e-4f;

    // setPipeline() must precede setBindGroup(): the bind group's layout is
    // validated against whatever pipeline layout is bound at the time of the
    // bind call, not at draw time. Binding it before selecting between
    // quad_aa_pipeline_/quad_pipeline_ validated it against whatever
    // unrelated pipeline the previous draw call in this pass had left bound
    // (e.g. a text glyph draw) — usually incompatible, which left the actual
    // draw with no valid descriptor set 0 and made it sample whatever
    // texture happened to still be resident in that binding slot.
    if (is_axis_aligned) {
        QuadAAUniforms u{};
        u.viewport[0] = vp_w_;
        u.viewport[1] = vp_h_;
        u.opacity     = opacity;
        u.pos[0] = std::min(c00.x, c01.x);
        u.pos[1] = std::min(c00.y, c10.y);
        u.pos[2] = std::max(c10.x, c11.x) - u.pos[0];
        u.pos[3] = std::max(c01.y, c11.y) - u.pos[1];
        u.uv[0]  = c00.u; u.uv[1] = c00.v;
        u.uv[2]  = c11.u; u.uv[3] = c11.v;
        encoder.setPipeline(quad_aa_pipeline_);
        encoder.setBindGroup(0, bind_group);
        encoder.setPushConstants(GPU::ShaderStage::vertex, 0, sizeof(QuadAAUniforms), &u);
        encoder.draw(6);
        return bind_group;
    }

    // Perspective / rotated quad — upload per-vertex projected positions.
    QuadVertex verts[6] = {
        {c00.x, c00.y, c00.w, c00.u, c00.v},
        {c10.x, c10.y, c10.w, c10.u, c10.v},
        {c01.x, c01.y, c01.w, c01.u, c01.v},
        {c01.x, c01.y, c01.w, c01.u, c01.v},
        {c10.x, c10.y, c10.w, c10.u, c10.v},
        {c11.x, c11.y, c11.w, c11.u, c11.v},
    };
    auto vbuf = quad_vertex_pool_.acquire(*device_, sizeof(verts), verts);
    if (!vbuf) return cached_bind_group;

    QuadUniforms u{};
    u.viewport[0] = vp_w_;
    u.viewport[1] = vp_h_;
    u.opacity     = opacity;
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
        encoder,
        /*cached_bind_group=*/nullptr,
        /*persistent=*/false,
        cmd.filter_quality == FilterQuality::none ? nearest_sampler_ : linear_sampler_);
}

// ---------------------------------------------------------------------------
// drawTintedImage — mirrors drawImage() exactly, see DrawTintedImageCmd
// ---------------------------------------------------------------------------

void VulkanDrawBackend::drawTintedImage(
    const DrawTintedImageCmd&        cmd,
    const Matrix4&                   transform,
    const Rect&                      clip,
    GPU::RenderPassEncoder&          encoder)
{
    namespace vm = systems::leal::vector_math;

    auto c00 = transform * vm::Vector4<float>(cmd.dst_rect.left(),  cmd.dst_rect.top(),    0.0f, 1.0f);
    auto c10 = transform * vm::Vector4<float>(cmd.dst_rect.right(), cmd.dst_rect.top(),    0.0f, 1.0f);
    auto c01 = transform * vm::Vector4<float>(cmd.dst_rect.left(),  cmd.dst_rect.bottom(), 0.0f, 1.0f);
    auto c11 = transform * vm::Vector4<float>(cmd.dst_rect.right(), cmd.dst_rect.bottom(), 0.0f, 1.0f);

    const float su0 = cmd.src_rect.left(),  sv0 = cmd.src_rect.top();
    const float su1 = cmd.src_rect.right(), sv1 = cmd.src_rect.bottom();

    drawTintedTexturedQuad(
        cmd.texture,
        {c00.x(), c00.y(), c00.w(), su0, sv0},
        {c10.x(), c10.y(), c10.w(), su1, sv0},
        {c01.x(), c01.y(), c01.w(), su0, sv1},
        {c11.x(), c11.y(), c11.w(), su1, sv1},
        cmd.tint,
        cmd.opacity,
        clip,
        encoder,
        cmd.filter_quality == FilterQuality::none ? nearest_sampler_ : linear_sampler_);
}

// ---------------------------------------------------------------------------
// drawTintedTexturedQuad — mirrors drawTexturedQuad()'s vertex-buffer path,
// binds icon_pipeline_/icon_layout_ instead of quad_pipeline_/quad_layout_
// ---------------------------------------------------------------------------

void VulkanDrawBackend::drawTintedTexturedQuad(
    std::shared_ptr<GPU::Texture>   texture,
    const QuadCorner&               c00,
    const QuadCorner&               c10,
    const QuadCorner&               c01,
    const QuadCorner&               c11,
    const Color&                    tint,
    float                           opacity,
    const Rect&                     clip,
    GPU::RenderPassEncoder&         encoder,
    std::shared_ptr<GPU::Sampler>   sampler)
{
    if (!icon_pipeline_ || !quad_bgl_ || !linear_sampler_) return;
    if (!texture) return;
    if (!sampler) sampler = linear_sampler_;

    GPU::BindGroupDescriptor bg_desc{};
    bg_desc.layout  = quad_bgl_;
    bg_desc.entries = {
        { 1, texture },
        { 2, sampler }
    };
    auto bind_group = device_->createBindGroup(bg_desc);
    if (!bind_group) return;

    applyScissor(clip, encoder);

    // Same corner/winding layout as drawTexturedQuad() — geometry shape is
    // identical between the quad and icon pipelines, only the pipeline/
    // push constants bound below differ.
    QuadVertex verts[6] = {
        {c00.x, c00.y, c00.w, c00.u, c00.v},
        {c10.x, c10.y, c10.w, c10.u, c10.v},
        {c01.x, c01.y, c01.w, c01.u, c01.v},
        {c01.x, c01.y, c01.w, c01.u, c01.v},
        {c10.x, c10.y, c10.w, c10.u, c10.v},
        {c11.x, c11.y, c11.w, c11.u, c11.v},
    };
    auto vbuf = quad_vertex_pool_.acquire(*device_, sizeof(verts), verts);
    if (!vbuf) return;

    IconUniforms u{};
    u.viewport[0] = vp_w_;
    u.viewport[1] = vp_h_;
    u.opacity     = opacity;
    u.tint[0]     = tint.r;
    u.tint[1]     = tint.g;
    u.tint[2]     = tint.b;
    u.tint[3]     = tint.a;

    encoder.setPipeline(icon_pipeline_);
    encoder.setBindGroup(0, bind_group);
    encoder.setPushConstants(
        static_cast<GPU::ShaderStage>(
            static_cast<int>(GPU::ShaderStage::vertex) |
            static_cast<int>(GPU::ShaderStage::fragment)),
        0, sizeof(IconUniforms), &u);
    encoder.setVertexBuffer(0, vbuf);
    encoder.draw(6);
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
        cached_bind_group,
        /*persistent=*/true);
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
    // copySrc/copyDst included so a RenderDrawSurface-style caller (which
    // relies on this method for its dedicated texture too — Vulkan never
    // pools, see createDedicatedOffscreenTexture()'s doc comment) can blit
    // a previous texture's content into this one on resize (see
    // Renderer::applyDrawSurfaceUpdate()'s blit_source path).
    auto tex = device_->createTexture(
        GPU::TextureType::tt2d,
        pixel_format_,
        width, height, 1, 1, 1,
        static_cast<GPU::TextureUsage>(
            static_cast<int>(GPU::TextureUsage::renderTarget) |
            static_cast<int>(GPU::TextureUsage::textureBinding) |
            static_cast<int>(GPU::TextureUsage::copySrc) |
            static_cast<int>(GPU::TextureUsage::copyDst)));
    if (tex)
        frame_textures_.push_back(tex);
    return tex;
}

std::shared_ptr<GPU::RenderPassEncoder> VulkanDrawBackend::beginOffscreenPass(
    std::shared_ptr<GPU::Texture> tex,
    GPU::CommandEncoder&          encoder,
    bool                          preserve_content)
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
    ca.loadOp        = preserve_content ? GPU::LoadOp::load : GPU::LoadOp::clear;
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
    auto vbuf = quad_vertex_pool_.acquire(*device_, sizeof(verts), verts);
    if (!vbuf) return;

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

// ---------------------------------------------------------------------------
// ShaderMask compositing
// ---------------------------------------------------------------------------

std::shared_ptr<GPU::Texture> VulkanDrawBackend::buildGradientLUT(
    const std::vector<Color>& colors,
    const std::vector<float>& stops)
{
    if (colors.empty()) return nullptr;

    // `stops` must be the same size as `colors` to index safely below; an
    // omitted or mismatched-size list falls back to evenly-spaced stops
    // (matches Flutter's Gradient.stops semantics) rather than indexing OOB.
    std::vector<float> even_stops;
    if (colors.size() > 1 && stops.size() != colors.size())
    {
        even_stops.resize(colors.size());
        for (size_t i = 0; i < colors.size(); ++i)
            even_stops[i] = static_cast<float>(i) / static_cast<float>(colors.size() - 1);
    }
    const std::vector<float>& use_stops = even_stops.empty() ? stops : even_stops;

    constexpr int kLutSize = 256;
    std::vector<uint8_t> data(kLutSize * 4);

    for (int i = 0; i < kLutSize; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(kLutSize - 1);

        Color c;
        if (colors.size() == 1)
        {
            c = colors[0];
        }
        else
        {
            int lo = 0;
            int hi = static_cast<int>(colors.size()) - 1;
            for (int s = 0; s < static_cast<int>(use_stops.size()) - 1; ++s)
            {
                if (t >= use_stops[s] && t <= use_stops[s + 1])
                {
                    lo = s;
                    hi = s + 1;
                    break;
                }
            }
            const float range = use_stops[hi] - use_stops[lo];
            const float f     = (range > 0.0001f) ? (t - use_stops[lo]) / range : 0.0f;
            const Color& ca   = colors[lo];
            const Color& cb   = colors[hi];
            c = Color::fromRGBA(
                ca.r + f * (cb.r - ca.r),
                ca.g + f * (cb.g - ca.g),
                ca.b + f * (cb.b - ca.b),
                ca.a + f * (cb.a - ca.a));
        }

        // BGRA layout (matching bgra8unorm).
        data[i * 4 + 0] = static_cast<uint8_t>(c.b * 255.0f);
        data[i * 4 + 1] = static_cast<uint8_t>(c.g * 255.0f);
        data[i * 4 + 2] = static_cast<uint8_t>(c.r * 255.0f);
        data[i * 4 + 3] = static_cast<uint8_t>(c.a * 255.0f);
    }

    auto lut = device_->createTexture(
        GPU::TextureType::tt2d, pixel_format_,
        kLutSize, 1, 1, 1, 1,
        static_cast<GPU::TextureUsage>(
            static_cast<int>(GPU::TextureUsage::textureBinding) |
            static_cast<int>(GPU::TextureUsage::copyDst)));
    if (!lut) return nullptr;

    lut->upload(0, static_cast<uint64_t>(kLutSize * 4), data.data());
    return lut;
}

void VulkanDrawBackend::drawShaderMaskComposite(
    std::shared_ptr<GPU::Texture>   child_tex,
    const DrawShaderMaskBeginCmd&   cmd,
    const Matrix4&                  transform,
    const Rect&                     clip,
    GPU::RenderPassEncoder&         encoder)
{
    if (!shader_mask_pipeline_ || !shader_mask_bgl_ || !linear_sampler_ || !child_tex)
        return;
    applyScissor(clip, encoder);

    namespace vm = systems::leal::vector_math;

    // Transform all four corners independently — real per-vertex quad.
    auto c00 = transform * vm::Vector4<float>(cmd.bounds.left(),  cmd.bounds.top(),    0.0f, 1.0f);
    auto c10 = transform * vm::Vector4<float>(cmd.bounds.right(), cmd.bounds.top(),    0.0f, 1.0f);
    auto c01 = transform * vm::Vector4<float>(cmd.bounds.left(),  cmd.bounds.bottom(), 0.0f, 1.0f);
    auto c11 = transform * vm::Vector4<float>(cmd.bounds.right(), cmd.bounds.bottom(), 0.0f, 1.0f);

    QuadVertex verts[6] = {
        {c00.x(), c00.y(), c00.w(), 0.0f, 0.0f},
        {c10.x(), c10.y(), c10.w(), 1.0f, 0.0f},
        {c01.x(), c01.y(), c01.w(), 0.0f, 1.0f},
        {c01.x(), c01.y(), c01.w(), 0.0f, 1.0f},
        {c10.x(), c10.y(), c10.w(), 1.0f, 0.0f},
        {c11.x(), c11.y(), c11.w(), 1.0f, 1.0f},
    };
    auto vbuf = quad_vertex_pool_.acquire(*device_, sizeof(verts), verts);
    if (!vbuf) return;

    // Build gradient LUT and parameters from shader variant.
    std::shared_ptr<GPU::Texture> lut_tex;
    float gradient_type = 0.0f;
    float tile_mode      = 0.0f;
    float p1[2] = {0.0f, 0.0f};
    float p2[2] = {0.0f, 0.0f};

    std::visit([&](auto&& s) {
        using S = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<S, LinearGradient>) {
            gradient_type = 0.0f;
            auto tp1 = transform * vm::Vector4<float>(cmd.bounds.x + s.begin.x,
                                                       cmd.bounds.y + s.begin.y, 0.0f, 1.0f);
            auto tp2 = transform * vm::Vector4<float>(cmd.bounds.x + s.end.x,
                                                       cmd.bounds.y + s.end.y,   0.0f, 1.0f);
            p1[0] = tp1.x();
            p1[1] = tp1.y();
            p2[0] = tp2.x();
            p2[1] = tp2.y();
            tile_mode = static_cast<float>(s.tile_mode);
            lut_tex = buildGradientLUT(s.colors, s.stops);
        } else if constexpr (std::is_same_v<S, RadialGradient>) {
            gradient_type = 1.0f;
            auto tc = transform * vm::Vector4<float>(cmd.bounds.x + s.center.x,
                                                      cmd.bounds.y + s.center.y, 0.0f, 1.0f);
            auto tv = transform * vm::Vector4<float>(1.0f, 0.0f, 0.0f, 0.0f);
            float sc = std::sqrt(tv.x() * tv.x() + tv.y() * tv.y());
            p1[0] = tc.x();
            p1[1] = tc.y();
            p2[0] = s.radius * sc;
            p2[1] = 0.0f;
            tile_mode = static_cast<float>(s.tile_mode);
            lut_tex = buildGradientLUT(s.colors, s.stops);
        } else if constexpr (std::is_same_v<S, SweepGradient>) {
            gradient_type = 2.0f;
            auto tc = transform * vm::Vector4<float>(cmd.bounds.x + s.center.x,
                                                      cmd.bounds.y + s.center.y, 0.0f, 1.0f);
            p1[0] = tc.x();
            p1[1] = tc.y();
            p2[0] = s.start_angle;
            p2[1] = s.end_angle;
            tile_mode = static_cast<float>(s.tile_mode);
            lut_tex = buildGradientLUT(s.colors, s.stops);
        }
    }, cmd.shader);

    if (!lut_tex) return;

    ShaderMaskUniforms u{};
    u.viewport[0]    = vp_w_;
    u.viewport[1]    = vp_h_;
    u.gradient_type  = gradient_type;
    u.tile_mode      = tile_mode;
    u.gradient_p1[0] = p1[0];
    u.gradient_p1[1] = p1[1];
    u.gradient_p1[2] = 0.0f;
    u.gradient_p1[3] = 0.0f;
    u.gradient_p2[0] = p2[0];
    u.gradient_p2[1] = p2[1];
    u.gradient_p2[2] = 0.0f;
    u.gradient_p2[3] = 0.0f;
    u.blend_mode     = (cmd.blend_mode == BlendMode::modulate) ? 1.0f : 0.0f;

    GPU::BindGroupDescriptor bg_desc{};
    bg_desc.layout = shader_mask_bgl_;
    bg_desc.entries = {
        { 1, child_tex },
        { 2, lut_tex },
        { 3, linear_sampler_ }
    };
    auto bind_group = device_->createBindGroup(bg_desc);
    if (!bind_group) return;

    encoder.setPipeline(shader_mask_pipeline_);
    encoder.setBindGroup(0, bind_group);
    encoder.setPushConstants(GPU::ShaderStage::vertex, 0, sizeof(ShaderMaskUniforms), &u);
    encoder.setVertexBuffer(0, vbuf);
    encoder.draw(6);
}

// ---------------------------------------------------------------------------
// SaveLayer compositing
// ---------------------------------------------------------------------------

void VulkanDrawBackend::saveLayerComposite(
    std::shared_ptr<GPU::Texture>   child_tex,
    const SaveLayerCmd&             cmd,
    const Matrix4&                  transform,
    const Rect&                     clip,
    GPU::RenderPassEncoder&         encoder)
{
    if (!child_tex) return;

    namespace vm = systems::leal::vector_math;

    auto c00 = transform * vm::Vector4<float>(cmd.bounds.left(),  cmd.bounds.top(),    0.0f, 1.0f);
    auto c10 = transform * vm::Vector4<float>(cmd.bounds.right(), cmd.bounds.top(),    0.0f, 1.0f);
    auto c01 = transform * vm::Vector4<float>(cmd.bounds.left(),  cmd.bounds.bottom(), 0.0f, 1.0f);
    auto c11 = transform * vm::Vector4<float>(cmd.bounds.right(), cmd.bounds.bottom(), 0.0f, 1.0f);

    drawTexturedQuad(
        child_tex,
        {c00.x(), c00.y(), c00.w(), 0.0f, 0.0f},
        {c10.x(), c10.y(), c10.w(), 1.0f, 0.0f},
        {c01.x(), c01.y(), c01.w(), 0.0f, 1.0f},
        {c11.x(), c11.y(), c11.w(), 1.0f, 1.0f},
        cmd.paint.color.a,
        clip,
        encoder);
}

} // namespace systems::leal::campello_widgets
