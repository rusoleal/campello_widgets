#include "d3d_draw_backend.hpp"
#include <campello_widgets/ui/draw_command.hpp>
#include <campello_widgets/ui/text_span.hpp>
#include <campello_widgets/ui/text_style.hpp>

#include <campello_gpu/device.hpp>
#include <campello_gpu/render_pipeline.hpp>
#include <campello_gpu/bind_group_layout.hpp>
#include <campello_gpu/bind_group.hpp>
#include <campello_gpu/pipeline_layout.hpp>
#include <campello_gpu/sampler.hpp>
#include <campello_gpu/texture.hpp>
#include <campello_gpu/buffer.hpp>
#include <campello_gpu/render_pass_encoder.hpp>
#include <campello_gpu/command_encoder.hpp>
#include <campello_gpu/texture_view.hpp>
#include <campello_gpu/descriptors/begin_render_pass_descriptor.hpp>
#include <campello_gpu/constants/buffer_usage.hpp>
#include <campello_gpu/constants/texture_usage.hpp>
#include <campello_gpu/constants/texture_type.hpp>
#include <campello_gpu/constants/filter_mode.hpp>
#include <campello_gpu/constants/wrap_mode.hpp>
#include <campello_gpu/constants/shader_stage.hpp>
#include <campello_gpu/constants/primitive_topology.hpp>
#include <campello_gpu/constants/cull_mode.hpp>
#include <campello_gpu/constants/front_face.hpp>
#include <campello_gpu/descriptors/render_pipeline_descriptor.hpp>
#include <campello_gpu/descriptors/bind_group_layout_descriptor.hpp>
#include <campello_gpu/descriptors/bind_group_descriptor.hpp>
#include <campello_gpu/descriptors/pipeline_layout_descriptor.hpp>
#include <campello_gpu/descriptors/sampler_descriptor.hpp>
#include <campello_gpu/descriptors/vertex_descriptor.hpp>

#include <vector_math/vector4.hpp>

#include "shaders/dx12_widgets.h"

#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace GPU = systems::leal::campello_gpu;
namespace vm  = systems::leal::vector_math;

namespace systems::leal::campello_widgets
{

// ---------------------------------------------------------------------------
// Uniform buffer layouts — must match the cbuffer structs in shaders/dx12/*.hlsl
// (which in turn mirror the Metal structs in shaders/metal/widgets.metal).
// ---------------------------------------------------------------------------

struct alignas(16) RectUniforms
{
    float color[4];     // r, g, b, a
    float viewport[2];  // width, height
    float _pad[2];
};

// Real per-vertex data for the rect pipeline (see drawFilledQuad()).
struct RectVertex { float x, y, w; };

struct alignas(16) QuadUniforms
{
    float viewport[2];
    float opacity;
    float _pad;
};

// Real per-vertex data for the quad pipeline (see drawTexturedQuad()).
struct QuadVertex { float x, y, w, u, v; };

struct alignas(16) ShapeUniforms
{
    float rect[4];
    float color[4];
    float viewport[2];
    float corner_r;
    float stroke_w;
    float kind;
    float _pad[3];
};

struct alignas(16) LineUniforms
{
    float p1[4];
    float p2[4];
    float color[4];
    float viewport[2];
    float stroke_w;
    float _pad;
};

struct alignas(16) ClipShapeUniforms
{
    float rect_size[2];  // logical w, h — used for SDF evaluation
    float viewport[2];   // physical w, h
    float corner_r;      // logical corner radius (ignored when kind == 1)
    float kind;          // 0 = rounded rect, 1 = ellipse/oval
    float _pad[2];
};

struct alignas(16) BlurUniforms
{
    float dstRect[4];   // x, y, w, h (pixels, destination quad)
    float srcRect[4];   // u0, v0, u1, v1 (normalised UV of source region)
    float viewport[2];  // framebuffer width, height
    float sigma;        // Gaussian sigma (pixels)
    float horizontal;   // 1.0 = H pass, 0.0 = V pass
    float tex_size[2];  // source texture width, height
    float _pad[2];
};

// ---------------------------------------------------------------------------
// Pools
// ---------------------------------------------------------------------------

std::shared_ptr<GPU::Buffer> D3DDrawBackend::VertexBufferPool::acquire(
    GPU::Device& device, uint64_t size, const void* data)
{
    auto&  buffers = generations_[current_generation_];
    size_t idx     = next_index_[current_generation_]++;

    if (idx >= buffers.size())
        buffers.push_back(device.createBuffer(size, GPU::BufferUsage::vertex, const_cast<void*>(data)));
    else
        buffers[idx]->upload(0, size, const_cast<void*>(data));

    return buffers[idx];
}

void D3DDrawBackend::VertexBufferPool::beginFrame() noexcept
{
    current_generation_ = (current_generation_ + 1) % kGenerations;
    next_index_[current_generation_] = 0;
}

D3DDrawBackend::UniformBindGroupPool::Slot D3DDrawBackend::UniformBindGroupPool::acquire(
    GPU::Device& device,
    const std::shared_ptr<GPU::BindGroupLayout>& layout,
    uint64_t size, const void* data)
{
    auto&  slots = generations_[current_generation_];
    size_t idx   = next_index_[current_generation_]++;

    if (idx >= slots.size())
    {
        Slot slot;
        slot.buffer = device.createBuffer(size, GPU::BufferUsage::uniform, const_cast<void*>(data));
        if (slot.buffer)
        {
            GPU::BindGroupDescriptor bgDesc{};
            bgDesc.layout  = layout;
            bgDesc.entries = { GPU::BindGroupEntryDescriptor{ 0, GPU::BufferBinding{ slot.buffer, 0, size } } };
            slot.bind_group = device.createBindGroup(bgDesc);
        }
        slots.push_back(slot);
        return slot;
    }

    slots[idx].buffer->upload(0, size, const_cast<void*>(data));
    return slots[idx];
}

void D3DDrawBackend::UniformBindGroupPool::beginFrame() noexcept
{
    current_generation_ = (current_generation_ + 1) % kGenerations;
    next_index_[current_generation_] = 0;
}

std::shared_ptr<GPU::Texture> D3DDrawBackend::OffscreenTexturePool::acquire(
    GPU::Device& device, uint32_t width, uint32_t height,
    GPU::PixelFormat format, GPU::TextureUsage usage, uint64_t current_frame)
{
    const SizeKey key{width, height};
    const bool isNewBucket = !last_used_frame_.count(key);
    last_used_frame_[key] = current_frame;

    if (isNewBucket && last_used_frame_.size() > kMaxSizeBuckets)
        evictLeastRecentlyUsed(key);

    auto&   textures = generations_[current_generation_][key];
    size_t& idx       = next_index_[current_generation_][key];

    if (idx >= textures.size())
        textures.push_back(device.createTexture(
            GPU::TextureType::tt2d, format, width, height, 1, 1, 1, usage));

    return textures[idx++];
}

void D3DDrawBackend::OffscreenTexturePool::evictLeastRecentlyUsed(const SizeKey& keep)
{
    auto oldest = last_used_frame_.end();
    for (auto it = last_used_frame_.begin(); it != last_used_frame_.end(); ++it)
    {
        if (it->first == keep) continue;
        if (oldest == last_used_frame_.end() || it->second < oldest->second)
            oldest = it;
    }
    if (oldest == last_used_frame_.end()) return;

    const SizeKey victim = oldest->first;
    for (auto& gen : generations_) gen.erase(victim);
    for (auto& idx : next_index_)  idx.erase(victim);
    last_used_frame_.erase(oldest);
}

void D3DDrawBackend::OffscreenTexturePool::beginFrame() noexcept
{
    current_generation_ = (current_generation_ + 1) % kGenerations;
    next_index_[current_generation_].clear();
}

void D3DDrawBackend::OffscreenTexturePool::evictStale(uint64_t current_frame)
{
    for (auto it = last_used_frame_.begin(); it != last_used_frame_.end(); )
    {
        if (current_frame - it->second > kMaxAgeFrames)
        {
            const SizeKey key = it->first;
            for (auto& gen : generations_) gen.erase(key);
            for (auto& idx : next_index_)  idx.erase(key);
            it = last_used_frame_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

// ---------------------------------------------------------------------------
// Construction — compile pipelines
// ---------------------------------------------------------------------------

namespace
{
    std::shared_ptr<GPU::ShaderModule> loadShader(
        const std::shared_ptr<GPU::Device>& device, const unsigned char* data, unsigned int size)
    {
        return device->createShaderModule(data, size);
    }

    GPU::BlendState premultipliedAlphaBlend()
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

    // ---------------------------------------------------------------------
    // Text rasterization helpers (GDI) — analogous to MetalDrawBackend's
    // CoreText path, but simpler: no DirectWrite/Direct2D COM setup needed.
    // ---------------------------------------------------------------------

    std::wstring utf8ToUtf16(const std::string& s)
    {
        if (s.empty()) return {};
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        if (len <= 0) return {};
        std::wstring w(static_cast<size_t>(len - 1), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), len);
        return w;
    }

    HFONT createFontForStyle(const TextStyle& style)
    {
        LOGFONTW lf{};
        const float size = style.font_size > 0.0f ? style.font_size : 14.0f;
        lf.lfHeight         = -static_cast<int>(size + 0.5f);
        lf.lfWeight         = (style.font_weight == FontWeight::bold) ? FW_BOLD : FW_NORMAL;
        lf.lfItalic         = style.italic ? TRUE : FALSE;
        lf.lfCharSet        = DEFAULT_CHARSET;
        lf.lfOutPrecision   = OUT_DEFAULT_PRECIS;
        lf.lfClipPrecision  = CLIP_DEFAULT_PRECIS;
        // Antialiased (grayscale), not ClearType — ClearType's per-channel
        // color fringing would corrupt the white-on-black luminance mask
        // used to derive per-pixel alpha in rasterizeText().
        lf.lfQuality        = ANTIALIASED_QUALITY;
        lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;

        std::wstring family = style.font_family.empty()
            ? std::wstring(L"Segoe UI") : utf8ToUtf16(style.font_family);
        wcsncpy_s(lf.lfFaceName, LF_FACESIZE, family.c_str(), _TRUNCATE);

        return CreateFontIndirectW(&lf);
    }
}

D3DDrawBackend::D3DDrawBackend(
    std::shared_ptr<GPU::Device> device,
    Color                        bg_color,
    GPU::PixelFormat             pixel_format)
    : device_(std::move(device))
    , bg_color_(bg_color)
    , pixel_format_(pixel_format)
{
    using namespace systems::leal::campello_widgets::shaders;

    auto rect_vs  = loadShader(device_, krect_vs_cso,  krect_vs_csoSize);
    auto rect_ps  = loadShader(device_, krect_ps_cso,  krect_ps_csoSize);
    auto quad_vs  = loadShader(device_, kquad_vs_cso,  kquad_vs_csoSize);
    auto quad_ps  = loadShader(device_, kquad_ps_cso,  kquad_ps_csoSize);
    auto shape_vs = loadShader(device_, kshape_vs_cso, kshape_vs_csoSize);
    auto shape_ps = loadShader(device_, kshape_ps_cso, kshape_ps_csoSize);
    auto line_vs  = loadShader(device_, kline_vs_cso,  kline_vs_csoSize);
    auto line_ps  = loadShader(device_, kline_ps_cso,  kline_ps_csoSize);
    auto blur_vs  = loadShader(device_, kblur_vs_cso,  kblur_vs_csoSize);
    auto blur_ps  = loadShader(device_, kblur_ps_cso,  kblur_ps_csoSize);
    auto clip_shape_vs = loadShader(device_, kclip_shape_vs_cso, kclip_shape_vs_csoSize);
    auto clip_shape_ps = loadShader(device_, kclip_shape_ps_cso, kclip_shape_ps_csoSize);

    if (!rect_vs || !rect_ps || !quad_vs || !quad_ps ||
        !shape_vs || !shape_ps || !line_vs || !line_ps ||
        !blur_vs || !blur_ps || !clip_shape_vs || !clip_shape_ps)
        return;

    auto cs = [&](GPU::ShaderStage stage) {
        GPU::EntryObject e{};
        e.binding    = 0;
        e.visibility = stage;
        e.type       = GPU::EntryObjectType::buffer;
        e.data.buffer.type             = GPU::EntryObjectBufferType::uniform;
        e.data.buffer.hasDinamicOffaset = false;
        e.data.buffer.minBindingSize    = 0;
        return e;
    };

    // --- Bind group layouts: one CBV (binding 0, vertex stage) each ---
    {
        GPU::BindGroupLayoutDescriptor desc{};
        desc.entries = { cs(GPU::ShaderStage::vertex) };
        rect_bgl_  = device_->createBindGroupLayout(desc);
        shape_bgl_ = device_->createBindGroupLayout(desc);
        line_bgl_  = device_->createBindGroupLayout(desc);
        blur_uniform_bgl_ = device_->createBindGroupLayout(desc);
    }

    // --- Bind group layout 0 for quad: uniform@0 (vertex) — pooled, reused
    //     across draws (see UniformBindGroupPool's doc comment).
    {
        GPU::BindGroupLayoutDescriptor desc{};
        desc.entries = { cs(GPU::ShaderStage::vertex) };
        quad_uniform_bgl_ = device_->createBindGroupLayout(desc);
    }

    // --- Bind group layout 1 for quad: texture@0, sampler@1 (pixel) — a
    //     SEPARATE layout/root parameter from the uniform above, because the
    //     texture varies per draw (arbitrary images/glyphs) and can't share a
    //     pooled ring-buffer BindGroup the way small numeric uniforms can.
    //     Matches MetalDrawBackend's quad_bgl_ (texture@0/sampler@1 only,
    //     with QuadUniforms delivered through a completely separate
    //     mechanism there too).
    {
        GPU::EntryObject tex{};
        tex.binding    = 0;
        tex.visibility = GPU::ShaderStage::fragment;
        tex.type       = GPU::EntryObjectType::texture;
        tex.data.texture.multisampled  = false;
        tex.data.texture.sampleType    = GPU::EntryObjectTextureType::ttFloat;
        tex.data.texture.viewDimension = GPU::TextureType::tt2d;

        GPU::EntryObject smp{};
        smp.binding    = 1;
        smp.visibility = GPU::ShaderStage::fragment;
        smp.type       = GPU::EntryObjectType::sampler;
        smp.data.sampler.type = GPU::EntryObjectSamplerType::filtering;

        GPU::BindGroupLayoutDescriptor desc{};
        desc.entries = { tex, smp };
        quad_tex_bgl_ = device_->createBindGroupLayout(desc);
    }

    // --- Bind group layout 0 for clip-shape composite: uniform@0 (vertex) —
    //     texture+sampler (bind group 1) reuses quad_tex_bgl_ below.
    {
        GPU::BindGroupLayoutDescriptor desc{};
        desc.entries = { cs(GPU::ShaderStage::vertex) };
        clip_shape_uniform_bgl_ = device_->createBindGroupLayout(desc);
    }

    if (!rect_bgl_ || !shape_bgl_ || !line_bgl_ || !quad_uniform_bgl_ ||
        !quad_tex_bgl_ || !blur_uniform_bgl_ || !clip_shape_uniform_bgl_)
        return;

    auto makeLayout = [&](std::vector<std::shared_ptr<GPU::BindGroupLayout>> bgls) {
        GPU::PipelineLayoutDescriptor desc{};
        desc.bindGroupLayouts = std::move(bgls);
        return device_->createPipelineLayout(desc);
    };
    auto rect_layout  = makeLayout({ rect_bgl_ });
    auto shape_layout = makeLayout({ shape_bgl_ });
    auto line_layout  = makeLayout({ line_bgl_ });
    // Order matters: index 0 = quad_uniform_bgl_ (setBindGroup(0, ...)),
    // index 1 = quad_tex_bgl_ (setBindGroup(1, ...)) — see drawTexturedQuad().
    auto quad_layout  = makeLayout({ quad_uniform_bgl_, quad_tex_bgl_ });
    // Same split, reusing quad_tex_bgl_ for the source texture+sampler —
    // see runBlurPass().
    auto blur_layout  = makeLayout({ blur_uniform_bgl_, quad_tex_bgl_ });
    // Same split again, reusing quad_tex_bgl_ for the child texture+sampler —
    // see drawClipShapeComposite().
    auto clip_shape_layout = makeLayout({ clip_shape_uniform_bgl_, quad_tex_bgl_ });
    if (!rect_layout || !shape_layout || !line_layout || !quad_layout || !blur_layout ||
        !clip_shape_layout)
        return;

    // --- Rect pipeline (premultiplied-alpha blend) ---
    {
        GPU::ColorState colorState{};
        colorState.format    = pixel_format_;
        colorState.writeMask = GPU::ColorWrite::all;
        colorState.blend     = premultipliedAlphaBlend();

        GPU::RenderPipelineDescriptor desc{};
        desc.vertex.module     = rect_vs;
        desc.vertex.entryPoint = "RectVS";

        GPU::VertexAttribute posAttr{};
        posAttr.componentType  = GPU::ComponentType::ctFloat;
        posAttr.accessorType   = GPU::AccessorType::acVec3;
        posAttr.offset         = 0;
        posAttr.shaderLocation = 0;

        GPU::VertexLayout layout{};
        layout.arrayStride = sizeof(RectVertex);
        layout.attributes  = { posAttr };
        layout.stepMode    = GPU::StepMode::vertex;
        desc.vertex.buffers = { layout };

        GPU::FragmentDescriptor frag{};
        frag.module     = rect_ps;
        frag.entryPoint = "RectPS";
        frag.targets.push_back(colorState);
        desc.fragment = frag;

        desc.topology  = GPU::PrimitiveTopology::triangleList;
        desc.cullMode  = GPU::CullMode::none;
        desc.frontFace = GPU::FrontFace::ccw;
        desc.layout    = rect_layout;

        rect_pipeline_ = device_->createRenderPipeline(desc);
    }

    // --- Quad (textured) pipeline ---
    {
        GPU::ColorState colorState{};
        colorState.format    = pixel_format_;
        colorState.writeMask = GPU::ColorWrite::all;
        colorState.blend     = premultipliedAlphaBlend();

        GPU::RenderPipelineDescriptor desc{};
        desc.vertex.module     = quad_vs;
        desc.vertex.entryPoint = "QuadVS";

        GPU::VertexAttribute posAttr{};
        posAttr.componentType  = GPU::ComponentType::ctFloat;
        posAttr.accessorType   = GPU::AccessorType::acVec3;
        posAttr.offset         = offsetof(QuadVertex, x);
        posAttr.shaderLocation = 0;

        GPU::VertexAttribute uvAttr{};
        uvAttr.componentType  = GPU::ComponentType::ctFloat;
        uvAttr.accessorType   = GPU::AccessorType::acVec2;
        uvAttr.offset         = offsetof(QuadVertex, u);
        uvAttr.shaderLocation = 1;

        GPU::VertexLayout layout{};
        layout.arrayStride = sizeof(QuadVertex);
        layout.attributes  = { posAttr, uvAttr };
        layout.stepMode    = GPU::StepMode::vertex;
        desc.vertex.buffers = { layout };

        GPU::FragmentDescriptor frag{};
        frag.module     = quad_ps;
        frag.entryPoint = "QuadPS";
        frag.targets.push_back(colorState);
        desc.fragment = frag;

        desc.topology  = GPU::PrimitiveTopology::triangleList;
        desc.cullMode  = GPU::CullMode::none;
        desc.frontFace = GPU::FrontFace::ccw;
        desc.layout    = quad_layout;

        quad_pipeline_ = device_->createRenderPipeline(desc);
    }

    // --- Shape pipeline (SDF circle/oval/rrect) — no vertex buffers ---
    {
        GPU::ColorState colorState{};
        colorState.format    = pixel_format_;
        colorState.writeMask = GPU::ColorWrite::all;
        colorState.blend     = premultipliedAlphaBlend();

        GPU::RenderPipelineDescriptor desc{};
        desc.vertex.module     = shape_vs;
        desc.vertex.entryPoint = "ShapeVS";

        GPU::FragmentDescriptor frag{};
        frag.module     = shape_ps;
        frag.entryPoint = "ShapePS";
        frag.targets.push_back(colorState);
        desc.fragment = frag;

        desc.topology  = GPU::PrimitiveTopology::triangleList;
        desc.cullMode  = GPU::CullMode::none;
        desc.frontFace = GPU::FrontFace::ccw;
        desc.layout    = shape_layout;

        shape_pipeline_ = device_->createRenderPipeline(desc);
    }

    // --- Line pipeline — no vertex buffers ---
    {
        GPU::ColorState colorState{};
        colorState.format    = pixel_format_;
        colorState.writeMask = GPU::ColorWrite::all;
        colorState.blend     = premultipliedAlphaBlend();

        GPU::RenderPipelineDescriptor desc{};
        desc.vertex.module     = line_vs;
        desc.vertex.entryPoint = "LineVS";

        GPU::FragmentDescriptor frag{};
        frag.module     = line_ps;
        frag.entryPoint = "LinePS";
        frag.targets.push_back(colorState);
        desc.fragment = frag;

        desc.topology  = GPU::PrimitiveTopology::triangleList;
        desc.cullMode  = GPU::CullMode::none;
        desc.frontFace = GPU::FrontFace::ccw;
        desc.layout    = line_layout;

        line_pipeline_ = device_->createRenderPipeline(desc);
    }

    // --- Blur pipeline (reuses quad_tex_bgl_ for the source texture+sampler
    //     via blur_layout_) — no vertex buffers ---
    {
        GPU::ColorState colorState{};
        colorState.format    = pixel_format_;
        colorState.writeMask = GPU::ColorWrite::all;
        colorState.blend     = premultipliedAlphaBlend();

        GPU::RenderPipelineDescriptor desc{};
        desc.vertex.module     = blur_vs;
        desc.vertex.entryPoint = "BlurVS";

        GPU::FragmentDescriptor frag{};
        frag.module     = blur_ps;
        frag.entryPoint = "BlurPS";
        frag.targets.push_back(colorState);
        desc.fragment = frag;

        desc.topology  = GPU::PrimitiveTopology::triangleList;
        desc.cullMode  = GPU::CullMode::none;
        desc.frontFace = GPU::FrontFace::ccw;
        desc.layout    = blur_layout;

        blur_pipeline_ = device_->createRenderPipeline(desc);
    }

    // --- Clip-shape composite pipeline (reuses quad_tex_bgl_ for the child
    //     texture+sampler via clip_shape_layout) — same QuadVertex layout as
    //     the quad pipeline ---
    {
        GPU::ColorState colorState{};
        colorState.format    = pixel_format_;
        colorState.writeMask = GPU::ColorWrite::all;
        colorState.blend     = premultipliedAlphaBlend();

        GPU::RenderPipelineDescriptor desc{};
        desc.vertex.module     = clip_shape_vs;
        desc.vertex.entryPoint = "ClipShapeVS";

        GPU::VertexAttribute posAttr{};
        posAttr.componentType  = GPU::ComponentType::ctFloat;
        posAttr.accessorType   = GPU::AccessorType::acVec3;
        posAttr.offset         = offsetof(QuadVertex, x);
        posAttr.shaderLocation = 0;

        GPU::VertexAttribute uvAttr{};
        uvAttr.componentType  = GPU::ComponentType::ctFloat;
        uvAttr.accessorType   = GPU::AccessorType::acVec2;
        uvAttr.offset         = offsetof(QuadVertex, u);
        uvAttr.shaderLocation = 1;

        GPU::VertexLayout layout{};
        layout.arrayStride = sizeof(QuadVertex);
        layout.attributes  = { posAttr, uvAttr };
        layout.stepMode    = GPU::StepMode::vertex;
        desc.vertex.buffers = { layout };

        GPU::FragmentDescriptor frag{};
        frag.module     = clip_shape_ps;
        frag.entryPoint = "ClipShapePS";
        frag.targets.push_back(colorState);
        desc.fragment = frag;

        desc.topology  = GPU::PrimitiveTopology::triangleList;
        desc.cullMode  = GPU::CullMode::none;
        desc.frontFace = GPU::FrontFace::ccw;
        desc.layout    = clip_shape_layout;

        clip_shape_pipeline_ = device_->createRenderPipeline(desc);
    }

    // --- Default sampler (linear, clamp-to-edge) ---
    {
        GPU::SamplerDescriptor sd{};
        sd.addressModeU  = GPU::WrapMode::clampToEdge;
        sd.addressModeV  = GPU::WrapMode::clampToEdge;
        sd.addressModeW  = GPU::WrapMode::clampToEdge;
        sd.magFilter     = GPU::FilterMode::fmLinear;
        sd.minFilter     = GPU::FilterMode::fmLinear;
        sd.lodMinClamp   = 0.0;
        sd.lodMaxClamp   = 1000.0;
        sd.maxAnisotropy = 1.0;
        quad_sampler_ = device_->createSampler(sd);
    }
}

D3DDrawBackend::~D3DDrawBackend()
{
    for (auto& [key, font] : font_cache_)
        DeleteObject(font);
}

// ---------------------------------------------------------------------------
// applyScissor
// ---------------------------------------------------------------------------

bool D3DDrawBackend::applyScissor(const Rect& clip, GPU::RenderPassEncoder& encoder)
{
    // See onBeginFlush()'s doc comment in the header: DirectX 12's
    // beginRenderPass() never sets a viewport itself, unlike Metal/Vulkan.
    if (viewport_dirty_)
    {
        encoder.setViewport(0.0f, 0.0f, vp_w_, vp_h_, 0.0f, 1.0f);
        viewport_dirty_ = false;
    }

    float x = clip.x      * dpr_;
    float y = clip.y      * dpr_;
    float w = clip.width  * dpr_;
    float h = clip.height * dpr_;

    const float rx = std::min(x + w, vp_w_);
    const float by = std::min(y + h, vp_h_);
    x = std::max(0.0f, x);
    y = std::max(0.0f, y);
    w = std::max(0.0f, rx - x);
    h = std::max(0.0f, by - y);

    if (w < 1.0f || h < 1.0f)
        return false;

    if (x == last_scissor_x_ && y == last_scissor_y_ &&
        w == last_scissor_w_ && h == last_scissor_h_)
        return true;

    encoder.setScissorRect(x, y, w, h);
    last_scissor_x_ = x;
    last_scissor_y_ = y;
    last_scissor_w_ = w;
    last_scissor_h_ = h;
    return true;
}

// ---------------------------------------------------------------------------
// drawFilledQuad / drawFilledRect / drawRect
// ---------------------------------------------------------------------------

void D3DDrawBackend::drawFilledQuad(
    const ProjectedCorner& c00, const ProjectedCorner& c10,
    const ProjectedCorner& c01, const ProjectedCorner& c11,
    const Color& color,
    GPU::RenderPassEncoder& encoder)
{
    if (!rect_pipeline_ || !rect_bgl_) return;

    const RectVertex verts[6] = {
        {c00.x, c00.y, c00.w}, {c10.x, c10.y, c10.w}, {c01.x, c01.y, c01.w},
        {c01.x, c01.y, c01.w}, {c10.x, c10.y, c10.w}, {c11.x, c11.y, c11.w},
    };
    auto vbuf = rect_vertex_pool_.acquire(*device_, sizeof(verts), verts);
    if (!vbuf) return;

    RectUniforms u{};
    u.color[0]    = color.r;
    u.color[1]    = color.g;
    u.color[2]    = color.b;
    u.color[3]    = color.a;
    u.viewport[0] = vp_w_;
    u.viewport[1] = vp_h_;

    auto slot = rect_uniform_pool_.acquire(*device_, rect_bgl_, sizeof(RectUniforms), &u);
    if (!slot.bind_group) return;

    encoder.setPipeline(rect_pipeline_);
    encoder.setBindGroup(0, slot.bind_group);
    encoder.setVertexBuffer(0, vbuf);
    encoder.draw(6);
}

void D3DDrawBackend::drawFilledRect(
    float x, float y, float w, float h,
    const Color& color,
    GPU::RenderPassEncoder& encoder)
{
    drawFilledQuad(
        ProjectedCorner{x,     y,     1.0f, 0.0f, 0.0f}, ProjectedCorner{x + w, y,     1.0f, 0.0f, 0.0f},
        ProjectedCorner{x,     y + h, 1.0f, 0.0f, 0.0f}, ProjectedCorner{x + w, y + h, 1.0f, 0.0f, 0.0f},
        color, encoder);
}

void D3DDrawBackend::drawRect(
    const DrawRectCmd&      cmd,
    const Matrix4&          transform,
    const Rect&             clip,
    GPU::RenderPassEncoder& encoder)
{
    if (!rect_pipeline_) return;
    if (!applyScissor(clip, encoder)) return;

    auto c00 = transform * vm::Vector4<float>(cmd.rect.left(),  cmd.rect.top(),    0.0f, 1.0f);
    auto c10 = transform * vm::Vector4<float>(cmd.rect.right(), cmd.rect.top(),    0.0f, 1.0f);
    auto c01 = transform * vm::Vector4<float>(cmd.rect.left(),  cmd.rect.bottom(), 0.0f, 1.0f);
    auto c11 = transform * vm::Vector4<float>(cmd.rect.right(), cmd.rect.bottom(), 0.0f, 1.0f);

    if (cmd.paint.style == PaintStyle::fill)
    {
        drawFilledQuad(
            ProjectedCorner{c00.x(), c00.y(), c00.w(), 0.0f, 0.0f},
            ProjectedCorner{c10.x(), c10.y(), c10.w(), 0.0f, 0.0f},
            ProjectedCorner{c01.x(), c01.y(), c01.w(), 0.0f, 0.0f},
            ProjectedCorner{c11.x(), c11.y(), c11.w(), 0.0f, 0.0f},
            cmd.paint.color, encoder);
    }
    else
    {
        const float min_x = std::min({c00.x(), c10.x(), c01.x(), c11.x()});
        const float min_y = std::min({c00.y(), c10.y(), c01.y(), c11.y()});
        const float max_x = std::max({c00.x(), c10.x(), c01.x(), c11.x()});
        const float max_y = std::max({c00.y(), c10.y(), c01.y(), c11.y()});

        const float sw = cmd.paint.stroke_width;
        const float w  = max_x - min_x;
        const float h  = max_y - min_y;

        drawFilledRect(min_x,      min_y,      w,  sw, cmd.paint.color, encoder);
        drawFilledRect(min_x,      max_y - sw, w,  sw, cmd.paint.color, encoder);
        drawFilledRect(min_x,      min_y + sw, sw, h - 2.0f * sw, cmd.paint.color, encoder);
        drawFilledRect(max_x - sw, min_y + sw, sw, h - 2.0f * sw, cmd.paint.color, encoder);
    }
}

// ---------------------------------------------------------------------------
// drawShape / drawCircle / drawOval / drawRRect
// ---------------------------------------------------------------------------

void D3DDrawBackend::drawShape(
    float x, float y, float w, float h,
    float corner_r, float stroke_w, float kind,
    const Color& color,
    GPU::RenderPassEncoder& encoder)
{
    if (!shape_pipeline_ || !shape_bgl_) return;

    ShapeUniforms u{};
    u.rect[0]     = x;
    u.rect[1]     = y;
    u.rect[2]     = w;
    u.rect[3]     = h;
    u.color[0]    = color.r;
    u.color[1]    = color.g;
    u.color[2]    = color.b;
    u.color[3]    = color.a;
    u.viewport[0] = vp_w_;
    u.viewport[1] = vp_h_;
    u.corner_r    = corner_r;
    u.stroke_w    = stroke_w;
    u.kind        = kind;

    auto slot = shape_uniform_pool_.acquire(*device_, shape_bgl_, sizeof(ShapeUniforms), &u);
    if (!slot.bind_group) return;

    encoder.setPipeline(shape_pipeline_);
    encoder.setBindGroup(0, slot.bind_group);
    encoder.draw(6);
}

void D3DDrawBackend::drawCircle(
    const DrawCircleCmd&    cmd,
    const Matrix4&          transform,
    const Rect&             clip,
    GPU::RenderPassEncoder& encoder)
{
    if (!shape_pipeline_) return;
    if (!applyScissor(clip, encoder)) return;

    auto tc = transform * vm::Vector4<float>(cmd.center.x, cmd.center.y, 0.0f, 1.0f);
    auto tv = transform * vm::Vector4<float>(1.0f, 0.0f, 0.0f, 0.0f);
    float scale = std::sqrt(tv.x() * tv.x() + tv.y() * tv.y());
    float r = cmd.radius * scale;

    float sw = (cmd.paint.style == PaintStyle::stroke) ? cmd.paint.stroke_width * scale : 0.0f;
    drawShape(tc.x() - r, tc.y() - r, r * 2.0f, r * 2.0f,
              0.0f, sw, 1.0f, cmd.paint.color, encoder);
}

void D3DDrawBackend::drawOval(
    const DrawOvalCmd&      cmd,
    const Matrix4&          transform,
    const Rect&             clip,
    GPU::RenderPassEncoder& encoder)
{
    if (!shape_pipeline_) return;
    if (!applyScissor(clip, encoder)) return;

    auto tl = transform * vm::Vector4<float>(cmd.rect.left(), cmd.rect.top(), 0.0f, 1.0f);
    auto br = transform * vm::Vector4<float>(cmd.rect.right(), cmd.rect.bottom(), 0.0f, 1.0f);
    float sw = (cmd.paint.style == PaintStyle::stroke) ? cmd.paint.stroke_width : 0.0f;
    drawShape(tl.x(), tl.y(), br.x() - tl.x(), br.y() - tl.y(),
              0.0f, sw, 1.0f, cmd.paint.color, encoder);
}

void D3DDrawBackend::drawRRect(
    const DrawRRectCmd&     cmd,
    const Matrix4&          transform,
    const Rect&             clip,
    GPU::RenderPassEncoder& encoder)
{
    if (!shape_pipeline_) return;
    if (!applyScissor(clip, encoder)) return;

    auto tl = transform * vm::Vector4<float>(cmd.rrect.rect.left(), cmd.rrect.rect.top(), 0.0f, 1.0f);
    auto br = transform * vm::Vector4<float>(cmd.rrect.rect.right(), cmd.rrect.rect.bottom(), 0.0f, 1.0f);
    auto tv = transform * vm::Vector4<float>(1.0f, 0.0f, 0.0f, 0.0f);
    float scale = std::sqrt(tv.x() * tv.x() + tv.y() * tv.y());
    float r  = (cmd.rrect.radius_x + cmd.rrect.radius_y) * 0.5f * scale;
    float sw = (cmd.paint.style == PaintStyle::stroke) ? cmd.paint.stroke_width * scale : 0.0f;
    drawShape(tl.x(), tl.y(), br.x() - tl.x(), br.y() - tl.y(),
              r, sw, 0.0f, cmd.paint.color, encoder);
}

// ---------------------------------------------------------------------------
// drawLine
// ---------------------------------------------------------------------------

void D3DDrawBackend::drawLine(
    const DrawLineCmd&      cmd,
    const Matrix4&          transform,
    const Rect&             clip,
    GPU::RenderPassEncoder& encoder)
{
    if (!line_pipeline_ || !line_bgl_) return;
    if (!applyScissor(clip, encoder)) return;

    auto tp1 = transform * vm::Vector4<float>(cmd.p1.x, cmd.p1.y, 0.0f, 1.0f);
    auto tp2 = transform * vm::Vector4<float>(cmd.p2.x, cmd.p2.y, 0.0f, 1.0f);
    auto tv  = transform * vm::Vector4<float>(1.0f, 0.0f, 0.0f, 0.0f);
    float scale = std::sqrt(tv.x() * tv.x() + tv.y() * tv.y());
    float sw = std::max(1.0f, cmd.paint.stroke_width * scale);

    LineUniforms u{};
    u.p1[0]       = tp1.x();
    u.p1[1]       = tp1.y();
    u.p2[0]       = tp2.x();
    u.p2[1]       = tp2.y();
    u.color[0]    = cmd.paint.color.r;
    u.color[1]    = cmd.paint.color.g;
    u.color[2]    = cmd.paint.color.b;
    u.color[3]    = cmd.paint.color.a;
    u.viewport[0] = vp_w_;
    u.viewport[1] = vp_h_;
    u.stroke_w    = sw;

    auto slot = line_uniform_pool_.acquire(*device_, line_bgl_, sizeof(LineUniforms), &u);
    if (!slot.bind_group) return;

    encoder.setPipeline(line_pipeline_);
    encoder.setBindGroup(0, slot.bind_group);
    encoder.draw(6);
}

// ---------------------------------------------------------------------------
// drawTexturedQuad / drawImage
// ---------------------------------------------------------------------------

std::shared_ptr<GPU::BindGroup> D3DDrawBackend::drawTexturedQuad(
    std::shared_ptr<GPU::Texture>  texture,
    const ProjectedCorner& c00, const ProjectedCorner& c10,
    const ProjectedCorner& c01, const ProjectedCorner& c11,
    float opacity,
    GPU::RenderPassEncoder&        encoder,
    std::shared_ptr<GPU::BindGroup> cached_bind_group)
{
    if (!quad_pipeline_ || !quad_uniform_bgl_ || !quad_tex_bgl_ || !quad_sampler_) return nullptr;

    // Bind group 1: texture@0/sampler@1 — reuse the caller-supplied cached
    // bind group if there is one (e.g. Renderer's text_texture_cache_),
    // otherwise build one. This is intentionally SEPARATE from the uniform
    // bind group below (see quad_tex_bgl_'s doc comment in the constructor
    // for why).
    auto texBindGroup = cached_bind_group;
    if (!texBindGroup)
    {
        GPU::BindGroupDescriptor bgDesc{};
        bgDesc.layout  = quad_tex_bgl_;
        bgDesc.entries = {
            GPU::BindGroupEntryDescriptor{ 0, texture },
            GPU::BindGroupEntryDescriptor{ 1, quad_sampler_ },
        };
        texBindGroup = device_->createBindGroup(bgDesc);
        if (!texBindGroup) return nullptr;
    }

    const QuadVertex verts[6] = {
        {c00.x, c00.y, c00.w, c00.u, c00.v},
        {c10.x, c10.y, c10.w, c10.u, c10.v},
        {c01.x, c01.y, c01.w, c01.u, c01.v},
        {c01.x, c01.y, c01.w, c01.u, c01.v},
        {c10.x, c10.y, c10.w, c10.u, c10.v},
        {c11.x, c11.y, c11.w, c11.u, c11.v},
    };
    auto vbuf = quad_vertex_pool_.acquire(*device_, sizeof(verts), verts);
    if (!vbuf) return texBindGroup;

    QuadUniforms u{};
    u.viewport[0] = vp_w_;
    u.viewport[1] = vp_h_;
    u.opacity     = opacity;

    // Bind group 0: QuadUniforms CBV — pooled/reused ring buffer, distinct
    // per draw's contents but not a fresh heap allocation every time.
    auto uniformSlot = quad_uniform_pool_.acquire(*device_, quad_uniform_bgl_, sizeof(QuadUniforms), &u);
    if (!uniformSlot.bind_group) return texBindGroup;

    encoder.setPipeline(quad_pipeline_);
    encoder.setBindGroup(0, uniformSlot.bind_group);
    encoder.setBindGroup(1, texBindGroup);
    encoder.setVertexBuffer(0, vbuf);
    encoder.draw(6);
    return texBindGroup;
}

void D3DDrawBackend::drawImage(
    const DrawImageCmd&     cmd,
    const Matrix4&          transform,
    const Rect&             clip,
    GPU::RenderPassEncoder& encoder)
{
    if (!quad_pipeline_ || !quad_tex_bgl_ || !quad_sampler_) return;
    if (!cmd.texture) return;
    if (!applyScissor(clip, encoder)) return;

    auto c00 = transform * vm::Vector4<float>(cmd.dst_rect.left(),  cmd.dst_rect.top(),    0.0f, 1.0f);
    auto c10 = transform * vm::Vector4<float>(cmd.dst_rect.right(), cmd.dst_rect.top(),    0.0f, 1.0f);
    auto c01 = transform * vm::Vector4<float>(cmd.dst_rect.left(),  cmd.dst_rect.bottom(), 0.0f, 1.0f);
    auto c11 = transform * vm::Vector4<float>(cmd.dst_rect.right(), cmd.dst_rect.bottom(), 0.0f, 1.0f);

    const float su0 = cmd.src_rect.x,      sv0 = cmd.src_rect.y;
    const float su1 = cmd.src_rect.right(), sv1 = cmd.src_rect.bottom();

    drawTexturedQuad(
        cmd.texture,
        ProjectedCorner{c00.x(), c00.y(), c00.w(), su0, sv0},
        ProjectedCorner{c10.x(), c10.y(), c10.w(), su1, sv0},
        ProjectedCorner{c01.x(), c01.y(), c01.w(), su0, sv1},
        ProjectedCorner{c11.x(), c11.y(), c11.w(), su1, sv1},
        cmd.opacity,
        encoder);
}

// ---------------------------------------------------------------------------
// Offscreen / compositing support (BackdropFilter)
// ---------------------------------------------------------------------------

std::shared_ptr<GPU::Texture> D3DDrawBackend::createOffscreenTexture(
    uint32_t width, uint32_t height)
{
    return offscreen_texture_pool_.acquire(*device_, width, height, pixel_format_,
        static_cast<GPU::TextureUsage>(
            static_cast<int>(GPU::TextureUsage::renderTarget) |
            static_cast<int>(GPU::TextureUsage::textureBinding) |
            static_cast<int>(GPU::TextureUsage::copySrc)),
        frame_counter_);
}

std::shared_ptr<GPU::RenderPassEncoder> D3DDrawBackend::beginOffscreenPass(
    std::shared_ptr<GPU::Texture> tex,
    GPU::CommandEncoder&          encoder)
{
    // New encoder == fresh scissor/viewport state.
    last_scissor_x_ = last_scissor_y_ = last_scissor_w_ = last_scissor_h_ = -1.0f;
    viewport_dirty_ = true;
    if (!tex) return nullptr;

    auto view = tex->createView(pixel_format_, 1);
    if (!view) return nullptr;

    GPU::ColorAttachment ca{};
    ca.view          = view;
    ca.loadOp        = GPU::LoadOp::clear;
    ca.storeOp       = GPU::StoreOp::store;
    ca.clearValue[0] = 0.0f;
    ca.clearValue[1] = 0.0f;
    ca.clearValue[2] = 0.0f;
    ca.clearValue[3] = 0.0f;

    GPU::BeginRenderPassDescriptor desc{};
    desc.colorAttachments = { ca };

    return encoder.beginRenderPass(desc);
}

void D3DDrawBackend::runBlurPass(
    std::shared_ptr<GPU::Texture> src,
    std::shared_ptr<GPU::Texture> dst,
    float sigma,
    bool  horizontal,
    GPU::CommandEncoder& encoder,
    std::shared_ptr<GPU::BindGroup> src_bind_group)
{
    if (!blur_pipeline_ || !blur_uniform_bgl_ || !quad_tex_bgl_ || !quad_sampler_ || !src || !dst || !src_bind_group) return;

    const uint32_t tw = static_cast<uint32_t>(dst->getWidth());
    const uint32_t th = static_cast<uint32_t>(dst->getHeight());

    auto dst_view = dst->createView(pixel_format_, 1);
    if (!dst_view) return;

    GPU::ColorAttachment ca{};
    ca.view    = dst_view;
    ca.loadOp  = GPU::LoadOp::clear;
    ca.storeOp = GPU::StoreOp::store;

    GPU::BeginRenderPassDescriptor desc{};
    desc.colorAttachments = { ca };

    auto rpe = encoder.beginRenderPass(desc);
    if (!rpe) return;

    BlurUniforms u{};
    u.dstRect[0]  = 0.0f;
    u.dstRect[1]  = 0.0f;
    u.dstRect[2]  = static_cast<float>(tw);
    u.dstRect[3]  = static_cast<float>(th);
    u.srcRect[0]  = 0.0f;
    u.srcRect[1]  = 0.0f;
    u.srcRect[2]  = 1.0f;
    u.srcRect[3]  = 1.0f;
    u.viewport[0] = static_cast<float>(tw);
    u.viewport[1] = static_cast<float>(th);
    u.sigma       = sigma;
    u.horizontal  = horizontal ? 1.0f : 0.0f;
    u.tex_size[0] = static_cast<float>(src->getWidth());
    u.tex_size[1] = static_cast<float>(src->getHeight());

    auto uniformSlot = blur_uniform_pool_.acquire(*device_, blur_uniform_bgl_, sizeof(BlurUniforms), &u);
    if (!uniformSlot.bind_group) { rpe->end(); return; }

    // Fresh command list segment (new render pass) — this pass's viewport
    // must be set explicitly too, matching applyScissor()'s doc comment.
    rpe->setViewport(0.0f, 0.0f, static_cast<float>(tw), static_cast<float>(th), 0.0f, 1.0f);
    rpe->setPipeline(blur_pipeline_);
    rpe->setBindGroup(0, uniformSlot.bind_group);
    rpe->setBindGroup(1, src_bind_group);
    rpe->draw(6);
    rpe->end();
}

std::shared_ptr<GPU::BindGroup> D3DDrawBackend::lookupOrCreateSourceBindGroup(
    const std::shared_ptr<GPU::Texture>& tex)
{
    if (!tex || !quad_tex_bgl_ || !quad_sampler_) return nullptr;

    auto it = blur_source_bind_group_cache_.find(tex.get());
    if (it != blur_source_bind_group_cache_.end())
        return it->second.bind_group;

    if (blur_source_bind_group_cache_.size() >= kMaxSourceBindGroupCacheSize)
        blur_source_bind_group_cache_.clear();

    GPU::BindGroupDescriptor bgDesc{};
    bgDesc.layout  = quad_tex_bgl_;
    bgDesc.entries = {
        GPU::BindGroupEntryDescriptor{ 0, tex },
        GPU::BindGroupEntryDescriptor{ 1, quad_sampler_ },
    };
    auto bindGroup = device_->createBindGroup(bgDesc);
    if (!bindGroup) return nullptr;

    blur_source_bind_group_cache_.emplace(tex.get(), SourceBindGroupCacheEntry{ tex, bindGroup });
    return bindGroup;
}

std::shared_ptr<GPU::Texture> D3DDrawBackend::blurTexture(
    std::shared_ptr<GPU::Texture> source,
    float sigma_x, float sigma_y,
    GPU::CommandEncoder& encoder)
{
    if (!source || !blur_pipeline_) return nullptr;

    const uint32_t tw = static_cast<uint32_t>(source->getWidth());
    const uint32_t th = static_cast<uint32_t>(source->getHeight());

    // See kBlurGenerations' doc comment: this frame's blur writes must land
    // in a slot the GPU is confirmed done reading from — the same
    // generation-rotation scheme the other pools use, keyed off
    // frame_counter_ (incremented once per setViewport() call, i.e. once
    // per raster frame) rather than a pool-local counter, since blur
    // textures aren't acquired from a pool.
    const size_t gen = frame_counter_ % kBlurGenerations;

    if (!blur_h_tex_[gen] || blur_tex_w_ != tw || blur_tex_h_ != th)
    {
        // A resize invalidates every generation, not just the current one —
        // recreate all slots together so a stale differently-sized texture
        // never surfaces when rotation reaches an untouched slot later.
        for (size_t i = 0; i < kBlurGenerations; ++i)
        {
            // About to become new GPU objects — drop their old cache
            // entries (the cache's own strong reference would otherwise
            // keep the old textures alive uselessly until the cache
            // happens to be cleared).
            if (blur_h_tex_[i]) blur_source_bind_group_cache_.erase(blur_h_tex_[i].get());
            if (blur_v_tex_[i]) blur_source_bind_group_cache_.erase(blur_v_tex_[i].get());

            const auto usage = static_cast<GPU::TextureUsage>(
                static_cast<int>(GPU::TextureUsage::renderTarget) |
                static_cast<int>(GPU::TextureUsage::textureBinding));
            blur_h_tex_[i] = device_->createTexture(GPU::TextureType::tt2d, pixel_format_, tw, th, 1, 1, 1, usage);
            blur_v_tex_[i] = device_->createTexture(GPU::TextureType::tt2d, pixel_format_, tw, th, 1, 1, 1, usage);
        }
        blur_tex_w_ = tw;
        blur_tex_h_ = th;
    }

    // Horizontal blur: source -> blur_h_tex_[gen]. `source` rotates through
    // OffscreenTexturePool, but lookupOrCreateSourceBindGroup() caches per
    // texture object, so this stays cheap once the pool warms up.
    runBlurPass(source, blur_h_tex_[gen], sigma_x, /*horizontal=*/true, encoder,
                lookupOrCreateSourceBindGroup(source));
    // Vertical blur: blur_h_tex_[gen] -> blur_v_tex_[gen]. Each slot is a
    // stable, persistent object across the frames it's reused for, so this
    // is a cache hit every time after the first frame that lands on it.
    runBlurPass(blur_h_tex_[gen], blur_v_tex_[gen], sigma_y, /*horizontal=*/false, encoder,
                lookupOrCreateSourceBindGroup(blur_h_tex_[gen]));

    return blur_v_tex_[gen];
}

void D3DDrawBackend::drawBackdropFilter(
    const DrawBackdropFilterBeginCmd& cmd,
    std::shared_ptr<GPU::Texture>     blurred_source,
    const Matrix4&                    transform,
    const Rect&                       clip,
    GPU::RenderPassEncoder&           encoder)
{
    if (!blurred_source) return;
    if (!applyScissor(clip, encoder)) return;

    // Transform all four corners independently (see ProjectedCorner's doc
    // comment) so a rotated/perspective-projected BackdropFilter renders as
    // a genuinely tilted shape, sampling the right region of the full-
    // viewport screen-space capture at each corner.
    auto c00 = transform * vm::Vector4<float>(cmd.bounds.left(),  cmd.bounds.top(),    0.0f, 1.0f);
    auto c10 = transform * vm::Vector4<float>(cmd.bounds.right(), cmd.bounds.top(),    0.0f, 1.0f);
    auto c01 = transform * vm::Vector4<float>(cmd.bounds.left(),  cmd.bounds.bottom(), 0.0f, 1.0f);
    auto c11 = transform * vm::Vector4<float>(cmd.bounds.right(), cmd.bounds.bottom(), 0.0f, 1.0f);

    const float src_w = static_cast<float>(blurred_source->getWidth());
    const float src_h = static_cast<float>(blurred_source->getHeight());

    // Divide by each corner's own w before normalizing by texture size — see
    // MetalDrawBackend::drawBackdropFilter()'s identical derivation.
    auto uv = [&](const vm::Vector4<float>& c) -> vm::Vector4<float> {
        return vm::Vector4<float>(c.x() / c.w() / src_w, c.y() / c.w() / src_h, 0.0f, 1.0f);
    };
    const auto uv00 = uv(c00);
    const auto uv10 = uv(c10);
    const auto uv01 = uv(c01);
    const auto uv11 = uv(c11);

    drawTexturedQuad(
        blurred_source,
        ProjectedCorner{c00.x(), c00.y(), c00.w(), uv00.x(), uv00.y()},
        ProjectedCorner{c10.x(), c10.y(), c10.w(), uv10.x(), uv10.y()},
        ProjectedCorner{c01.x(), c01.y(), c01.w(), uv01.x(), uv01.y()},
        ProjectedCorner{c11.x(), c11.y(), c11.w(), uv11.x(), uv11.y()},
        1.0f,
        encoder,
        lookupOrCreateSourceBindGroup(blurred_source));
}

void D3DDrawBackend::drawClipShapeComposite(
    std::shared_ptr<GPU::Texture> child_tex,
    const Rect&                   bounds,
    float                         corner_radius,
    bool                          is_oval,
    const Matrix4&                transform,
    const Rect&                   clip,
    GPU::RenderPassEncoder&       encoder)
{
    if (!clip_shape_pipeline_ || !clip_shape_uniform_bgl_ || !quad_tex_bgl_ || !quad_sampler_) return;
    if (!child_tex) return;
    if (!applyScissor(clip, encoder)) return;

    // Transform all four corners independently — real per-vertex quad, not
    // an axis-aligned bounding box — so a rotated/perspective/mirrored
    // ClipRRect/ClipOval renders as a genuinely tilted shape instead of a
    // resized box. See MetalDrawBackend::drawClipShapeComposite()'s doc
    // comment for the full rationale.
    auto c00 = transform * vm::Vector4<float>(bounds.left(),  bounds.top(),    0.0f, 1.0f);
    auto c10 = transform * vm::Vector4<float>(bounds.right(), bounds.top(),    0.0f, 1.0f);
    auto c01 = transform * vm::Vector4<float>(bounds.left(),  bounds.bottom(), 0.0f, 1.0f);
    auto c11 = transform * vm::Vector4<float>(bounds.right(), bounds.bottom(), 0.0f, 1.0f);

    const QuadVertex verts[6] = {
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

    auto uniformSlot = clip_shape_uniform_pool_.acquire(*device_, clip_shape_uniform_bgl_, sizeof(ClipShapeUniforms), &u);
    if (!uniformSlot.bind_group) return;

    auto texBindGroup = lookupOrCreateSourceBindGroup(child_tex);
    if (!texBindGroup) return;

    encoder.setPipeline(clip_shape_pipeline_);
    encoder.setBindGroup(0, uniformSlot.bind_group);
    encoder.setBindGroup(1, texBindGroup);
    encoder.setVertexBuffer(0, vbuf);
    encoder.draw(6);
}

// ---------------------------------------------------------------------------
// Font cache
// ---------------------------------------------------------------------------

size_t D3DDrawBackend::FontCacheKeyHash::operator()(const FontCacheKey& k) const noexcept
{
    size_t h = std::hash<std::wstring>{}(k.family);
    auto mix = [&h](size_t v) { h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2); };
    mix(std::hash<int>{}(k.height));
    mix(std::hash<bool>{}(k.bold));
    mix(std::hash<bool>{}(k.italic));
    return h;
}

// Both measureText() (layout) and rasterizeText() (paint) need
// an HFONT for the same TextStyle; caching by the exact fields
// createFontForStyle() derives from it (not the raw TextStyle) means two
// styles that resolve to the same LOGFONT — e.g. font_size 14.0f vs
// 14.2f, which both round to the same integer lfHeight — correctly share
// one cached font instead of minting a near-duplicate.
HFONT D3DDrawBackend::getOrCreateFont(const TextStyle& style) const
{
    const float size = style.font_size > 0.0f ? style.font_size : 14.0f;
    FontCacheKey key;
    key.family = style.font_family.empty()
        ? std::wstring(L"Segoe UI") : utf8ToUtf16(style.font_family);
    key.height = -static_cast<int>(size + 0.5f);
    key.bold   = style.font_weight == FontWeight::bold;
    key.italic = style.italic;

    if (auto it = font_cache_.find(key); it != font_cache_.end())
        return it->second;

    HFONT font = createFontForStyle(style);
    if (font)
        font_cache_.emplace(std::move(key), font);
    return font;
}

// ---------------------------------------------------------------------------
// measureText
// ---------------------------------------------------------------------------

Size D3DDrawBackend::measureText(const TextSpan& span) const
{
    if (span.text.empty()) return Size::zero();

    const float fontSize = span.style.font_size > 0.0f ? span.style.font_size : 14.0f;
    const Size fallback{ fontSize * 0.6f * static_cast<float>(span.text.size()), fontSize * 1.2f };

    std::wstring wtext = utf8ToUtf16(span.text);
    if (wtext.empty()) return fallback;

    HDC hdc = CreateCompatibleDC(nullptr);
    if (!hdc) return fallback;

    HFONT font = getOrCreateFont(span.style);
    if (!font) { DeleteDC(hdc); return fallback; }

    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
    SIZE sz{};
    BOOL ok = GetTextExtentPoint32W(hdc, wtext.c_str(), static_cast<int>(wtext.size()), &sz);
    SelectObject(hdc, oldFont);
    DeleteDC(hdc);

    if (!ok || sz.cx <= 0 || sz.cy <= 0) return fallback;
    return Size{ static_cast<float>(sz.cx), static_cast<float>(sz.cy) };
}

// ---------------------------------------------------------------------------
// rasterizeText — GDI glyph rasterization only, no caching (Renderer's
// text_texture_cache_ owns that — see its doc comment).
// ---------------------------------------------------------------------------

std::shared_ptr<GPU::Texture> D3DDrawBackend::rasterizeText(
    const TextSpan& span, float /*dpr*/,
    uint32_t& out_width, uint32_t& out_height)
{
    if (span.text.empty()) return nullptr;
    std::wstring wtext = utf8ToUtf16(span.text);
    if (wtext.empty()) return nullptr;

    HDC hdc = CreateCompatibleDC(nullptr);
    if (!hdc) return nullptr;

    HFONT font = getOrCreateFont(span.style);
    if (!font) { DeleteDC(hdc); return nullptr; }
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));

    SIZE sz{};
    GetTextExtentPoint32W(hdc, wtext.c_str(), static_cast<int>(wtext.size()), &sz);
    if (sz.cx <= 0 || sz.cy <= 0)
    {
        SelectObject(hdc, oldFont);
        DeleteDC(hdc);
        return nullptr;
    }

    // Small padding for anti-aliasing, matching MetalDrawBackend's approach.
    const uint32_t texW = static_cast<uint32_t>(sz.cx) + 2;
    const uint32_t texH = static_cast<uint32_t>(sz.cy) + 2;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = static_cast<LONG>(texW);
    bmi.bmiHeader.biHeight      = -static_cast<LONG>(texH); // negative = top-down DIB
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dib || !bits)
    {
        SelectObject(hdc, oldFont);
        DeleteDC(hdc);
        if (dib) DeleteObject(dib);
        return nullptr;
    }
    std::memset(bits, 0, static_cast<size_t>(texW) * texH * 4);

    // Draw white text on the zeroed (black) DIB — its grayscale luminance
    // becomes this glyph's alpha mask below, since GDI's ExtTextOut/DrawText
    // don't write a usable alpha channel directly.
    HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(hdc, dib));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    RECT rect{ 1, 1, static_cast<LONG>(texW) - 1, static_cast<LONG>(texH) - 1 };
    DrawTextW(hdc, wtext.c_str(), static_cast<int>(wtext.size()), &rect,
              DT_LEFT | DT_TOP | DT_NOCLIP | DT_SINGLELINE);
    GdiFlush();

    // Convert the luminance mask into a premultiplied-alpha texture tinted
    // with the paint color. CreateDIBSection's 32bpp BI_RGB layout stores
    // bytes as B,G,R,X per pixel; since the text was drawn pure white,
    // R == G == B, so any one channel works as the luminance/alpha source.
    // pixel_format_ is always PixelFormat::rgba8unorm on this backend (the
    // DirectX 12 swapchain format — see the constructor), which stores
    // bytes as R,G,B,A in increasing memory order.
    const Color& tc = span.style.color;
    std::vector<uint8_t> pixels(static_cast<size_t>(texW) * texH * 4, 0);
    const uint8_t* src = static_cast<const uint8_t*>(bits);
    for (uint32_t i = 0; i < texW * texH; ++i)
    {
        const float alpha = src[i * 4 + 0] / 255.0f;
        pixels[i * 4 + 0] = static_cast<uint8_t>(tc.r * alpha * 255.0f + 0.5f);
        pixels[i * 4 + 1] = static_cast<uint8_t>(tc.g * alpha * 255.0f + 0.5f);
        pixels[i * 4 + 2] = static_cast<uint8_t>(tc.b * alpha * 255.0f + 0.5f);
        pixels[i * 4 + 3] = static_cast<uint8_t>(alpha * 255.0f + 0.5f);
    }

    SelectObject(hdc, oldBmp);
    SelectObject(hdc, oldFont);
    DeleteObject(dib);
    DeleteDC(hdc);

    auto texture = device_->createTexture(
        GPU::TextureType::tt2d, pixel_format_, texW, texH, 1, 1, 1,
        GPU::TextureUsage::textureBinding);
    if (!texture) return nullptr;
    texture->upload(0, static_cast<uint64_t>(pixels.size()), pixels.data());

    out_width  = texW;
    out_height = texH;
    return texture;
}

// ---------------------------------------------------------------------------
// drawTextTexture — draws an already-rasterized text texture (from
// rasterizeText(), cached or fresh) as a quad.
// ---------------------------------------------------------------------------

std::shared_ptr<GPU::BindGroup> D3DDrawBackend::drawTextTexture(
    std::shared_ptr<GPU::Texture>   texture,
    std::shared_ptr<GPU::BindGroup> cached_bind_group,
    uint32_t width, uint32_t height,
    const Offset&            origin,
    const Matrix4&           transform,
    const Rect&              clip,
    GPU::RenderPassEncoder&  encoder)
{
    if (!quad_pipeline_ || !quad_tex_bgl_ || !quad_sampler_) return nullptr;
    if (!texture) return nullptr;
    if (!applyScissor(clip, encoder)) return nullptr;

    auto t_origin = transform * vm::Vector4<float>(origin.x, origin.y, 0.0f, 1.0f);

    // Subtract the 1-physical-pixel padding baked into the rasterized
    // texture. Width/height are added post-transform in physical pixels —
    // text rotation/perspective is out of scope for this pass, matching
    // Metal.
    const float x0 = t_origin.x() - 1.0f;
    const float y0 = t_origin.y() - 1.0f;
    const float x1 = x0 + static_cast<float>(width);
    const float y1 = y0 + static_cast<float>(height);

    return drawTexturedQuad(
        texture,
        ProjectedCorner{x0, y0, 1.0f, 0.0f, 0.0f}, ProjectedCorner{x1, y0, 1.0f, 1.0f, 0.0f},
        ProjectedCorner{x0, y1, 1.0f, 0.0f, 1.0f}, ProjectedCorner{x1, y1, 1.0f, 1.0f, 1.0f},
        1.0f,  // text colour alpha is already baked into the glyph texture
        encoder,
        cached_bind_group);
}

} // namespace systems::leal::campello_widgets
