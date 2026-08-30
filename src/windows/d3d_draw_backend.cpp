#include "d3d_draw_backend.hpp"
#include "gpu/path_tessellation.hpp"
#include "gpu/stroke_geometry.hpp"
#include "gpu/path_fill_aa.hpp"
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

struct alignas(16) QuadUniforms
{
    float viewport[2];
    float opacity;
    float _pad;
};

// Real per-vertex data for the quad pipeline (see drawTexturedQuad()).
struct QuadVertex { float x, y, w, u, v; };

// Mirrors icon.hlsl's IconUniforms field-for-field.
struct alignas(16) IconUniforms
{
    float viewport[2];
    float opacity;
    float _pad;
    float tint[4];
};

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

std::shared_ptr<GPU::Buffer> D3DDrawBackend::IndexBufferPool::acquire(
    GPU::Device& device, uint64_t size, const void* data)
{
    auto&  buffers = generations_[current_generation_];
    size_t idx     = next_index_[current_generation_]++;

    if (idx >= buffers.size())
        buffers.push_back(device.createBuffer(size, GPU::BufferUsage::index, const_cast<void*>(data)));
    else
        buffers[idx]->upload(0, size, const_cast<void*>(data));

    return buffers[idx];
}

void D3DDrawBackend::IndexBufferPool::beginFrame() noexcept
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
    // Not part of the fatal shader-load gate below: if this shader isn't
    // present (dx12_widgets.h regeneration on Windows is required after
    // adding shaders/dx12/rect_aa.hlsl -- no local compiler exists on this
    // development machine), drawPath() just skips the fill-AA skirt
    // (checked via rect_aa_pipeline_ == nullptr) rather than failing the
    // whole backend. See src/gpu/path_fill_aa.hpp.
    auto rect_aa_vs = loadShader(device_, krect_aa_vs_cso, krect_aa_vs_csoSize);
    auto rect_aa_ps = loadShader(device_, krect_aa_ps_cso, krect_aa_ps_csoSize);
    auto vertices_vs = loadShader(device_, kvertices_vs_cso, kvertices_vs_csoSize);
    auto vertices_ps = loadShader(device_, kvertices_ps_cso, kvertices_ps_csoSize);
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
    auto shader_mask_vs = loadShader(device_, kshader_mask_vs_cso, kshader_mask_vs_csoSize);
    auto shader_mask_ps = loadShader(device_, kshader_mask_ps_cso, kshader_mask_ps_csoSize);

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

    // --- Bind group layout 0 for icon: uniform@0, visible to *both*
    //     vertex and pixel stages — IconPS itself reads `tint`, unlike
    //     QuadPS (QuadUniforms is vertex-only). Bind group 1 (texture+
    //     sampler) reuses quad_tex_bgl_ below — same shape.
    {
        GPU::BindGroupLayoutDescriptor desc{};
        desc.entries = { cs(GPU::ShaderStage::vertex | GPU::ShaderStage::fragment) };
        icon_uniform_bgl_ = device_->createBindGroupLayout(desc);
    }

    // --- Bind group layout 0 for clip-shape composite: uniform@0 (vertex) —
    //     texture+sampler (bind group 1) reuses quad_tex_bgl_ below.
    {
        GPU::BindGroupLayoutDescriptor desc{};
        desc.entries = { cs(GPU::ShaderStage::vertex) };
        clip_shape_uniform_bgl_ = device_->createBindGroupLayout(desc);
    }

    // --- Bind group layout 0 for shader-mask composite: uniform@0 (vertex).
    {
        GPU::BindGroupLayoutDescriptor desc{};
        desc.entries = { cs(GPU::ShaderStage::vertex) };
        shader_mask_uniform_bgl_ = device_->createBindGroupLayout(desc);
    }

    // --- Bind group layout 1 for shader-mask composite: child texture@0,
    //     gradient LUT@1, sampler@2 (pixel) — mirrors Metal's bindings.
    {
        GPU::EntryObject childTex{};
        childTex.binding    = 0;
        childTex.visibility = GPU::ShaderStage::fragment;
        childTex.type       = GPU::EntryObjectType::texture;
        childTex.data.texture.multisampled  = false;
        childTex.data.texture.sampleType    = GPU::EntryObjectTextureType::ttFloat;
        childTex.data.texture.viewDimension = GPU::TextureType::tt2d;

        GPU::EntryObject lutTex = childTex;
        lutTex.binding = 1;

        GPU::EntryObject smp{};
        smp.binding    = 2;
        smp.visibility = GPU::ShaderStage::fragment;
        smp.type       = GPU::EntryObjectType::sampler;
        smp.data.sampler.type = GPU::EntryObjectSamplerType::filtering;

        GPU::BindGroupLayoutDescriptor desc{};
        desc.entries = { childTex, lutTex, smp };
        shader_mask_bgl_ = device_->createBindGroupLayout(desc);
    }

    if (!rect_bgl_ || !shape_bgl_ || !line_bgl_ || !quad_uniform_bgl_ ||
        !quad_tex_bgl_ || !blur_uniform_bgl_ || !clip_shape_uniform_bgl_ ||
        !shader_mask_uniform_bgl_ || !shader_mask_bgl_)
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
    // Same split, reusing quad_tex_bgl_ — see drawTintedTexturedQuad().
    auto icon_layout  = makeLayout({ icon_uniform_bgl_, quad_tex_bgl_ });
    // Same split, reusing quad_tex_bgl_ for the source texture+sampler —
    // see runBlurPass().
    auto blur_layout  = makeLayout({ blur_uniform_bgl_, quad_tex_bgl_ });
    // Same split again, reusing quad_tex_bgl_ for the child texture+sampler —
    // see drawClipShapeComposite().
    auto clip_shape_layout = makeLayout({ clip_shape_uniform_bgl_, quad_tex_bgl_ });
    // ShaderMask uses its own texture+sampler bind group with two textures.
    auto shader_mask_layout = makeLayout({ shader_mask_uniform_bgl_, shader_mask_bgl_ });
    if (!rect_layout || !shape_layout || !line_layout || !quad_layout || !blur_layout ||
        !clip_shape_layout || !shader_mask_layout)
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

    // --- Rect-AA pipeline (premultiplied-alpha blend) — same shape as the
    //     rect pipeline above, plus a per-vertex alpha; see rect_aa.hlsl's
    //     doc comment and drawFillAA() below. A separate pipeline (not an
    //     addition to rect_pipeline_ itself) so every existing
    //     rect_pipeline_ call site stays untouched. Reuses rect_layout/
    //     rect_bgl_ (identical RectUniforms layout). ---
    if (rect_aa_vs && rect_aa_ps)
    {
        GPU::ColorState colorState{};
        colorState.format    = pixel_format_;
        colorState.writeMask = GPU::ColorWrite::all;
        colorState.blend     = premultipliedAlphaBlend();

        GPU::RenderPipelineDescriptor desc{};
        desc.vertex.module     = rect_aa_vs;
        desc.vertex.entryPoint = "RectAAVS";

        GPU::VertexAttribute posAlphaAttr{};
        posAlphaAttr.componentType  = GPU::ComponentType::ctFloat;
        posAlphaAttr.accessorType   = GPU::AccessorType::acVec4;
        posAlphaAttr.offset         = 0;
        posAlphaAttr.shaderLocation = 0;

        GPU::VertexLayout layout{};
        layout.arrayStride = sizeof(RectAAVertex);
        layout.attributes  = { posAlphaAttr };
        layout.stepMode    = GPU::StepMode::vertex;
        desc.vertex.buffers = { layout };

        GPU::FragmentDescriptor frag{};
        frag.module     = rect_aa_ps;
        frag.entryPoint = "RectAAPS";
        frag.targets.push_back(colorState);
        desc.fragment = frag;

        desc.topology  = GPU::PrimitiveTopology::triangleList;
        desc.cullMode  = GPU::CullMode::none;
        desc.frontFace = GPU::FrontFace::ccw;
        desc.layout    = rect_layout;

        rect_aa_pipeline_ = device_->createRenderPipeline(desc);
    }

    // --- Vertices pipeline (drawVertices()) — premultiplied-alpha blend,
    //     per-vertex color instead of rect_aa's shared-uniform color +
    //     per-vertex alpha; see vertices.hlsl's doc comment. Reuses
    //     rect_layout/rect_bgl_ -- VerticesUniforms occupies the same
    //     16-byte viewport prefix RectUniforms does. ---
    if (vertices_vs && vertices_ps)
    {
        GPU::ColorState colorState{};
        colorState.format    = pixel_format_;
        colorState.writeMask = GPU::ColorWrite::all;
        colorState.blend     = premultipliedAlphaBlend();

        GPU::RenderPipelineDescriptor desc{};
        desc.vertex.module     = vertices_vs;
        desc.vertex.entryPoint = "VerticesVS";

        GPU::VertexAttribute posAttr{};
        posAttr.componentType  = GPU::ComponentType::ctFloat;
        posAttr.accessorType   = GPU::AccessorType::acVec3;
        posAttr.offset         = 0;
        posAttr.shaderLocation = 0;

        GPU::VertexAttribute colorAttr{};
        colorAttr.componentType  = GPU::ComponentType::ctFloat;
        colorAttr.accessorType   = GPU::AccessorType::acVec4;
        colorAttr.offset         = 3 * sizeof(float);
        colorAttr.shaderLocation = 1;

        GPU::VertexLayout layout{};
        layout.arrayStride = sizeof(VerticesVertex);
        layout.attributes  = { posAttr, colorAttr };
        layout.stepMode    = GPU::StepMode::vertex;
        desc.vertex.buffers = { layout };

        GPU::FragmentDescriptor frag{};
        frag.module     = vertices_ps;
        frag.entryPoint = "VerticesPS";
        frag.targets.push_back(colorState);
        desc.fragment = frag;

        desc.topology  = GPU::PrimitiveTopology::triangleList;
        desc.cullMode  = GPU::CullMode::none;
        desc.frontFace = GPU::FrontFace::ccw;
        desc.layout    = rect_layout;

        vertices_pipeline_ = device_->createRenderPipeline(desc);
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

    // --- Icon pipeline — tinted template images (see icon.hlsl) ---
    // Loaded locally (not part of the mandatory shader list checked at the
    // top of this constructor) so a missing/uncompiled icon shader
    // degrades gracefully — see IDrawBackend::drawTintedImage()'s default
    // no-op — rather than breaking every other pipeline's initialization.
    {
        auto icon_vs = loadShader(device_, shaders::kicon_vs_cso, shaders::kicon_vs_csoSize);
        auto icon_ps = loadShader(device_, shaders::kicon_ps_cso, shaders::kicon_ps_csoSize);
        if (!icon_vs || !icon_ps) {
            std::fprintf(stderr, "[D3DDrawBackend] icon shader load FAILED\n");
        } else {
            GPU::ColorState colorState{};
            colorState.format    = pixel_format_;
            colorState.writeMask = GPU::ColorWrite::all;
            colorState.blend     = premultipliedAlphaBlend();

            GPU::RenderPipelineDescriptor desc{};
            desc.vertex.module     = icon_vs;
            desc.vertex.entryPoint = "IconVS";

            // Same vertex layout as the quad pipeline (QuadVertex: x,y,w,u,v).
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
            frag.module     = icon_ps;
            frag.entryPoint = "IconPS";
            frag.targets.push_back(colorState);
            desc.fragment = frag;

            desc.topology  = GPU::PrimitiveTopology::triangleList;
            desc.cullMode  = GPU::CullMode::none;
            desc.frontFace = GPU::FrontFace::ccw;
            desc.layout    = icon_layout;

            icon_pipeline_ = device_->createRenderPipeline(desc);
            if (!icon_pipeline_) std::fprintf(stderr, "[D3DDrawBackend] icon_pipeline_ creation FAILED\n");
        }
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

    // --- ShaderMask composite pipeline (child tex + gradient LUT + sampler) ---
    if (shader_mask_vs && shader_mask_ps)
    {
        GPU::ColorState colorState{};
        colorState.format    = pixel_format_;
        colorState.writeMask = GPU::ColorWrite::all;
        colorState.blend     = premultipliedAlphaBlend();

        GPU::RenderPipelineDescriptor desc{};
        desc.vertex.module     = shader_mask_vs;
        desc.vertex.entryPoint = "ShaderMaskVS";

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
        frag.module     = shader_mask_ps;
        frag.entryPoint = "ShaderMaskPS";
        frag.targets.push_back(colorState);
        desc.fragment = frag;

        desc.topology  = GPU::PrimitiveTopology::triangleList;
        desc.cullMode  = GPU::CullMode::none;
        desc.frontFace = GPU::FrontFace::ccw;
        desc.layout    = shader_mask_layout;

        shader_mask_pipeline_ = device_->createRenderPipeline(desc);
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

    // --- Nearest-neighbor sampler (clamp-to-edge) --- used only by
    // drawImage()/drawTintedImage() when FilterQuality::none is requested;
    // quad_sampler_ (linear) stays the shared default for every other
    // texture-sampling draw.
    {
        GPU::SamplerDescriptor sd{};
        sd.addressModeU  = GPU::WrapMode::clampToEdge;
        sd.addressModeV  = GPU::WrapMode::clampToEdge;
        sd.addressModeW  = GPU::WrapMode::clampToEdge;
        sd.magFilter     = GPU::FilterMode::fmNearest;
        sd.minFilter     = GPU::FilterMode::fmNearest;
        sd.lodMinClamp   = 0.0;
        sd.lodMaxClamp   = 1000.0;
        sd.maxAnisotropy = 1.0;
        nearest_sampler_ = device_->createSampler(sd);
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
// drawFilledQuad / drawRect
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

void D3DDrawBackend::drawFilledVertices(
    const std::vector<RectVertex>& verts,
    const Color& color,
    GPU::RenderPassEncoder& encoder)
{
    if (!rect_pipeline_ || !rect_bgl_ || verts.empty()) return;

    auto vbuf = rect_vertex_pool_.acquire(*device_, verts.size() * sizeof(RectVertex), verts.data());
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
    encoder.draw(static_cast<uint32_t>(verts.size()));
}

void D3DDrawBackend::drawFillAA(
    const std::vector<RectAAVertex>& verts,
    const Color& color,
    GPU::RenderPassEncoder& encoder)
{
    if (!rect_aa_pipeline_ || !rect_bgl_ || verts.empty()) return;

    auto vbuf = rect_vertex_pool_.acquire(*device_, verts.size() * sizeof(RectAAVertex), verts.data());
    if (!vbuf) return;

    // RectUniforms layout (color, viewport) is shared with rect_pipeline_ --
    // see rect_aa.hlsl's doc comment.
    RectUniforms u{};
    u.color[0]    = color.r;
    u.color[1]    = color.g;
    u.color[2]    = color.b;
    u.color[3]    = color.a;
    u.viewport[0] = vp_w_;
    u.viewport[1] = vp_h_;

    auto slot = rect_uniform_pool_.acquire(*device_, rect_bgl_, sizeof(RectUniforms), &u);
    if (!slot.bind_group) return;

    encoder.setPipeline(rect_aa_pipeline_);
    encoder.setBindGroup(0, slot.bind_group);
    encoder.setVertexBuffer(0, vbuf);
    encoder.draw(static_cast<uint32_t>(verts.size()));
}

void D3DDrawBackend::drawVertices(
    const DrawVerticesCmd&  cmd,
    const Matrix4&          transform,
    const Rect&              clip,
    GPU::RenderPassEncoder& encoder)
{
    if (!vertices_pipeline_ || !rect_bgl_ || cmd.vertices.empty() || cmd.indices.empty()) return;
    if (!applyScissor(clip, encoder)) return;

    std::vector<VerticesVertex> verts;
    verts.reserve(cmd.vertices.size());
    for (const auto& v : cmd.vertices)
    {
        auto p = transform * vm::Vector4<float>(v.pos.x, v.pos.y, 0.0f, 1.0f);
        verts.push_back({p.x(), p.y(), p.w(), v.color.r, v.color.g, v.color.b, v.color.a});
    }

    auto vbuf = vertices_vertex_pool_.acquire(*device_, verts.size() * sizeof(VerticesVertex), verts.data());
    if (!vbuf) return;

    auto ibuf = vertices_index_pool_.acquire(*device_, cmd.indices.size() * sizeof(uint32_t), cmd.indices.data());
    if (!ibuf) return;

    // RectUniforms layout (color, viewport) is shared with rect_pipeline_ --
    // see vertices.hlsl's doc comment. `color` is unused by this pipeline.
    RectUniforms u{};
    u.viewport[0] = vp_w_;
    u.viewport[1] = vp_h_;

    auto slot = rect_uniform_pool_.acquire(*device_, rect_bgl_, sizeof(RectUniforms), &u);
    if (!slot.bind_group) return;

    encoder.setPipeline(vertices_pipeline_);
    encoder.setBindGroup(0, slot.bind_group);
    encoder.setVertexBuffer(0, vbuf);
    encoder.setIndexBuffer(ibuf, GPU::IndexFormat::uint32);
    encoder.drawIndexed(static_cast<uint32_t>(cmd.indices.size()));
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
        // Stroke: the 4 corners as a closed polyline through
        // strokePolyline() — correctly handles rotation (unlike the old
        // always-axis-aligned 4-separate-rects approach) and gets real
        // caps/joins for free. Local (pre-transform) corners:
        // strokePolyline() applies `transform` itself.
        const std::vector<Offset> corners{
            {cmd.rect.left(),  cmd.rect.top()},
            {cmd.rect.right(), cmd.rect.top()},
            {cmd.rect.right(), cmd.rect.bottom()},
            {cmd.rect.left(),  cmd.rect.bottom()},
        };
        strokePolyline(corners, /*closed=*/true, cmd.paint, clip, transform, encoder);
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

// ---------------------------------------------------------------------------
// Stroke primitives — see this backend's header for the overall design.
// ---------------------------------------------------------------------------

void D3DDrawBackend::drawStrokeSegmentBody(
    const Offset& p0, const Offset& p1, float half_width,
    const Color& color, const Matrix4& transform,
    GPU::RenderPassEncoder& encoder)
{
    if (!line_pipeline_ || !line_bgl_) return;

    auto tp0 = transform * vm::Vector4<float>(p0.x, p0.y, 0.0f, 1.0f);
    auto tp1 = transform * vm::Vector4<float>(p1.x, p1.y, 0.0f, 1.0f);
    auto tv  = transform * vm::Vector4<float>(1.0f, 0.0f, 0.0f, 0.0f);
    float scale = std::sqrt(tv.x() * tv.x() + tv.y() * tv.y());

    LineUniforms u{};
    u.p1[0]       = tp0.x();
    u.p1[1]       = tp0.y();
    u.p2[0]       = tp1.x();
    u.p2[1]       = tp1.y();
    u.color[0]    = color.r;
    u.color[1]    = color.g;
    u.color[2]    = color.b;
    u.color[3]    = color.a;
    u.viewport[0] = vp_w_;
    u.viewport[1] = vp_h_;
    u.stroke_w    = std::max(1.0f, half_width * 2.0f * scale);

    auto slot = line_uniform_pool_.acquire(*device_, line_bgl_, sizeof(LineUniforms), &u);
    if (!slot.bind_group) return;

    encoder.setPipeline(line_pipeline_);
    encoder.setBindGroup(0, slot.bind_group);
    encoder.draw(6);
}

void D3DDrawBackend::drawStrokeRoundPrimitive(
    const Offset& center, float half_width,
    const Color& color, const Matrix4& transform,
    GPU::RenderPassEncoder& encoder)
{
    auto tc = transform * vm::Vector4<float>(center.x, center.y, 0.0f, 1.0f);
    auto tv = transform * vm::Vector4<float>(1.0f, 0.0f, 0.0f, 0.0f);
    float scale = std::sqrt(tv.x() * tv.x() + tv.y() * tv.y());
    float r = half_width * scale;
    drawShape(tc.x() - r, tc.y() - r, r * 2.0f, r * 2.0f,
              0.0f, 0.0f, 1.0f /*kind=circle*/, color, encoder);
}

void D3DDrawBackend::strokePolyline(
    const std::vector<Offset>& points, bool closed,
    const Paint& paint, const Rect& clip, const Matrix4& transform,
    GPU::RenderPassEncoder& encoder)
{
    if (!applyScissor(clip, encoder)) return;

    const float half_width = std::max(0.5f, paint.stroke_width * 0.5f);
    auto geo = buildStrokeGeometry(points, closed, half_width,
                                    paint.stroke_cap, paint.stroke_join, paint.stroke_miter_limit);

    for (const auto& seg : geo.segments)
        drawStrokeSegmentBody(seg.p0, seg.p1, half_width, paint.color, transform, encoder);

    for (const auto& c : geo.circles)
        drawStrokeRoundPrimitive(c.center, half_width, paint.color, transform, encoder);

    if (!geo.wedges.empty())
    {
        std::vector<RectVertex> verts;
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
        drawFilledVertices(verts, paint.color, encoder);
    }
}

void D3DDrawBackend::appendStrokePolylineBatched(
    const std::vector<Offset>& points, bool closed,
    const Paint& paint, const Matrix4& transform,
    std::vector<RectVertex>& verts)
{
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
    constexpr int   kFanSegments = 12;
    constexpr float kTwoPi       = 6.28318530717958647692f; // avoids relying on M_PI, which MSVC's
                                                              // <cmath> only defines under _USE_MATH_DEFINES
    for (const auto& circ : geo.circles)
    {
        for (int i = 0; i < kFanSegments; ++i)
        {
            const float a0 = (kTwoPi * i) / kFanSegments;
            const float a1 = (kTwoPi * (i + 1)) / kFanSegments;
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
    // A line is just a 2-point open polyline -- strokePolyline() gives it
    // real caps (butt/round/square) for free; a plain 2-point stroke has no
    // interior vertex, so stroke_join never applies here.
    strokePolyline({cmd.p1, cmd.p2}, /*closed=*/false, cmd.paint, clip, transform, encoder);
}

// ---------------------------------------------------------------------------
// drawArc — tessellate to triangles and draw via rect_pipeline_
// ---------------------------------------------------------------------------

void D3DDrawBackend::drawArc(
    const DrawArcCmd&       cmd,
    const Matrix4&          transform,
    const Rect&             clip,
    GPU::RenderPassEncoder& encoder)
{
    if (!rect_pipeline_) return;
    if (!applyScissor(clip, encoder)) return;

    const float cx = cmd.rect.x + cmd.rect.width * 0.5f;
    const float cy = cmd.rect.y + cmd.rect.height * 0.5f;
    const float rx = cmd.rect.width * 0.5f;
    const float ry = cmd.rect.height * 0.5f;
    if (rx <= 0.0f || ry <= 0.0f) return;

    const float abs_sweep = std::abs(cmd.sweep_angle);
    const int   segments  = std::max(3, static_cast<int>(abs_sweep * 20.0f));

    std::vector<RectVertex> verts;
    verts.reserve(static_cast<size_t>(segments) * 6);

    const bool is_stroke = (cmd.paint.style == PaintStyle::stroke);
    const float stroke_w = std::max(1.0f, cmd.paint.stroke_width);

    auto tv = transform * vm::Vector4<float>(1.0f, 0.0f, 0.0f, 0.0f);
    const float scale = std::sqrt(tv.x() * tv.x() + tv.y() * tv.y());
    const float pixel_stroke = stroke_w * scale;

    for (int i = 0; i < segments; ++i)
    {
        const float t0 = cmd.start_angle + cmd.sweep_angle * (float(i) / float(segments));
        const float t1 = cmd.start_angle + cmd.sweep_angle * (float(i + 1) / float(segments));

        if (cmd.use_center)
        {
            const vm::Vector4<float> p_center = transform * vm::Vector4<float>(cx, cy, 0.0f, 1.0f);
            const vm::Vector4<float> p0 = transform * vm::Vector4<float>(cx + rx * std::cos(t0), cy + ry * std::sin(t0), 0.0f, 1.0f);
            const vm::Vector4<float> p1 = transform * vm::Vector4<float>(cx + rx * std::cos(t1), cy + ry * std::sin(t1), 0.0f, 1.0f);

            verts.push_back({p_center.x(), p_center.y(), p_center.w()});
            verts.push_back({p0.x(), p0.y(), p0.w()});
            verts.push_back({p1.x(), p1.y(), p1.w()});
        }
        else
        {
            const auto eval = [&](float t, float r) -> vm::Vector4<float> {
                const float ox = cx + rx * std::cos(t);
                const float oy = cy + ry * std::sin(t);
                float nx = std::cos(t) / rx;
                float ny = std::sin(t) / ry;
                float nlen = std::sqrt(nx * nx + ny * ny);
                if (nlen > 0.0001f) { nx /= nlen; ny /= nlen; }
                return transform * vm::Vector4<float>(ox - r * nx, oy - r * ny, 0.0f, 1.0f);
            };

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

    if (!verts.empty())
        drawFilledVertices(verts, cmd.paint.color, encoder);
}

// ---------------------------------------------------------------------------
// drawPath — CPU tessellation to triangles, drawn via rect_pipeline_
// ---------------------------------------------------------------------------

void D3DDrawBackend::drawPath(
    const DrawPathCmd&      cmd,
    const Matrix4&          transform,
    const Rect&             clip,
    GPU::RenderPassEncoder& encoder)
{
    if (!rect_pipeline_) return;

    auto contours = buildPathContours(cmd.path);
    if (contours.empty()) return;

    std::vector<RectVertex> verts;

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
    if (!applyScissor(clip, encoder)) return;

    drawFilledVertices(verts, cmd.paint.color, encoder);

    // Fill antialiasing "skirt" -- see src/gpu/path_fill_aa.hpp. Interior
    // fill above is untouched; this adds one further draw call (regardless
    // of contour/point count) for a thin antialiased band along each
    // contour's own boundary. Stroke fills don't need this: they're
    // already antialiased via appendStrokePolylineBatched().
    if (cmd.paint.style != PaintStyle::stroke)
    {
        auto tv = transform * vm::Vector4<float>(1.0f, 0.0f, 0.0f, 0.0f);
        const float scale = std::sqrt(tv.x() * tv.x() + tv.y() * tv.y());
        const float aa_width_local = (scale > 1e-4f) ? (1.0f / scale) : 1.0f;

        std::vector<RectAAVertex> aa_verts;
        for (const auto& contour : contours)
        {
            std::vector<Offset> pts;
            pts.reserve(contour.size());
            for (const auto& v : contour) pts.push_back({v.x, v.y});

            for (const auto& sv : buildFillAASkirt(pts, aa_width_local))
            {
                auto p = transform * vm::Vector4<float>(sv.pos.x, sv.pos.y, 0.0f, 1.0f);
                aa_verts.push_back({p.x(), p.y(), p.w(), sv.alpha});
            }
        }
        drawFillAA(aa_verts, cmd.paint.color, encoder);
    }
}

// ---------------------------------------------------------------------------
// drawPoints — decompose to circles/lines using existing pipelines
// ---------------------------------------------------------------------------

void D3DDrawBackend::drawPoints(
    const DrawPointsCmd&    cmd,
    const Matrix4&          transform,
    const Rect&             clip,
    GPU::RenderPassEncoder& encoder)
{
    if (cmd.points.empty()) return;

    switch (cmd.mode)
    {
        case PointMode::points:
        {
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
// saveLayerComposite — draw the offscreen child texture modulated by opacity
// ---------------------------------------------------------------------------

void D3DDrawBackend::saveLayerComposite(
    std::shared_ptr<GPU::Texture> child_tex,
    const SaveLayerCmd&           cmd,
    const Matrix4&                transform,
    const Rect&                   clip,
    GPU::RenderPassEncoder&       encoder)
{
    if (!child_tex) return;
    if (!applyScissor(clip, encoder)) return;

    auto c00 = transform * vm::Vector4<float>(cmd.bounds.left(),  cmd.bounds.top(),    0.0f, 1.0f);
    auto c10 = transform * vm::Vector4<float>(cmd.bounds.right(), cmd.bounds.top(),    0.0f, 1.0f);
    auto c01 = transform * vm::Vector4<float>(cmd.bounds.left(),  cmd.bounds.bottom(), 0.0f, 1.0f);
    auto c11 = transform * vm::Vector4<float>(cmd.bounds.right(), cmd.bounds.bottom(), 0.0f, 1.0f);

    drawTexturedQuad(
        child_tex,
        ProjectedCorner{c00.x(), c00.y(), c00.w(), 0.0f, 0.0f},
        ProjectedCorner{c10.x(), c10.y(), c10.w(), 1.0f, 0.0f},
        ProjectedCorner{c01.x(), c01.y(), c01.w(), 0.0f, 1.0f},
        ProjectedCorner{c11.x(), c11.y(), c11.w(), 1.0f, 1.0f},
        cmd.paint.color.a,
        encoder);
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
    std::shared_ptr<GPU::BindGroup> cached_bind_group,
    std::shared_ptr<GPU::Sampler>   sampler)
{
    if (!quad_pipeline_ || !quad_uniform_bgl_ || !quad_tex_bgl_ || !quad_sampler_) return nullptr;
    if (!sampler) sampler = quad_sampler_;

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
            GPU::BindGroupEntryDescriptor{ 1, sampler },
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
        encoder,
        /*cached_bind_group=*/nullptr,
        cmd.filter_quality == FilterQuality::none ? nearest_sampler_ : quad_sampler_);
}

// ---------------------------------------------------------------------------
// drawTintedImage — mirrors drawImage() exactly, see DrawTintedImageCmd
// ---------------------------------------------------------------------------

void D3DDrawBackend::drawTintedImage(
    const DrawTintedImageCmd& cmd,
    const Matrix4&            transform,
    const Rect&               clip,
    GPU::RenderPassEncoder&   encoder)
{
    if (!icon_pipeline_ || !icon_uniform_bgl_ || !quad_tex_bgl_ || !quad_sampler_) return;
    if (!cmd.texture) return;
    if (!applyScissor(clip, encoder)) return;

    auto c00 = transform * vm::Vector4<float>(cmd.dst_rect.left(),  cmd.dst_rect.top(),    0.0f, 1.0f);
    auto c10 = transform * vm::Vector4<float>(cmd.dst_rect.right(), cmd.dst_rect.top(),    0.0f, 1.0f);
    auto c01 = transform * vm::Vector4<float>(cmd.dst_rect.left(),  cmd.dst_rect.bottom(), 0.0f, 1.0f);
    auto c11 = transform * vm::Vector4<float>(cmd.dst_rect.right(), cmd.dst_rect.bottom(), 0.0f, 1.0f);

    const float su0 = cmd.src_rect.x,      sv0 = cmd.src_rect.y;
    const float su1 = cmd.src_rect.right(), sv1 = cmd.src_rect.bottom();

    drawTintedTexturedQuad(
        cmd.texture,
        ProjectedCorner{c00.x(), c00.y(), c00.w(), su0, sv0},
        ProjectedCorner{c10.x(), c10.y(), c10.w(), su1, sv0},
        ProjectedCorner{c01.x(), c01.y(), c01.w(), su0, sv1},
        ProjectedCorner{c11.x(), c11.y(), c11.w(), su1, sv1},
        cmd.tint,
        cmd.opacity,
        encoder,
        cmd.filter_quality == FilterQuality::none ? nearest_sampler_ : quad_sampler_);
}

// ---------------------------------------------------------------------------
// drawTintedTexturedQuad — mirrors drawTexturedQuad(), binds icon_pipeline_
// ---------------------------------------------------------------------------

void D3DDrawBackend::drawTintedTexturedQuad(
    std::shared_ptr<GPU::Texture>  texture,
    const ProjectedCorner& c00, const ProjectedCorner& c10,
    const ProjectedCorner& c01, const ProjectedCorner& c11,
    const Color& tint,
    float opacity,
    GPU::RenderPassEncoder&        encoder,
    std::shared_ptr<GPU::Sampler>  sampler)
{
    if (!icon_pipeline_ || !icon_uniform_bgl_ || !quad_tex_bgl_ || !quad_sampler_) return;
    if (!sampler) sampler = quad_sampler_;

    GPU::BindGroupDescriptor bgDesc{};
    bgDesc.layout  = quad_tex_bgl_;
    bgDesc.entries = {
        GPU::BindGroupEntryDescriptor{ 0, texture },
        GPU::BindGroupEntryDescriptor{ 1, sampler },
    };
    auto texBindGroup = device_->createBindGroup(bgDesc);
    if (!texBindGroup) return;

    const QuadVertex verts[6] = {
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

    auto uniformSlot = icon_uniform_pool_.acquire(*device_, icon_uniform_bgl_, sizeof(IconUniforms), &u);
    if (!uniformSlot.bind_group) return;

    encoder.setPipeline(icon_pipeline_);
    encoder.setBindGroup(0, uniformSlot.bind_group);
    encoder.setBindGroup(1, texBindGroup);
    encoder.setVertexBuffer(0, vbuf);
    encoder.draw(6);
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

std::shared_ptr<GPU::Texture> D3DDrawBackend::createDedicatedOffscreenTexture(
    uint32_t width, uint32_t height)
{
    // Bypasses offscreen_texture_pool_ entirely — see this method's doc
    // comment on IDrawBackend for why a pooled texture is unsafe for a
    // caller that keeps referencing it (and relying on its content) across
    // many future frames. copyDst (in addition to copySrc) so a
    // RenderDrawSurface-style caller can blit a previous dedicated
    // texture's content into this one on resize (see Renderer::
    // applyDrawSurfaceUpdate()'s blit_source path).
    return device_->createTexture(
        GPU::TextureType::tt2d, pixel_format_, width, height, 1, 1, 1,
        static_cast<GPU::TextureUsage>(
            static_cast<int>(GPU::TextureUsage::renderTarget) |
            static_cast<int>(GPU::TextureUsage::textureBinding) |
            static_cast<int>(GPU::TextureUsage::copySrc) |
            static_cast<int>(GPU::TextureUsage::copyDst)));
}

std::shared_ptr<GPU::RenderPassEncoder> D3DDrawBackend::beginOffscreenPass(
    std::shared_ptr<GPU::Texture> tex,
    GPU::CommandEncoder&          encoder,
    bool                          preserve_content)
{
    // New encoder == fresh scissor/viewport state.
    last_scissor_x_ = last_scissor_y_ = last_scissor_w_ = last_scissor_h_ = -1.0f;
    viewport_dirty_ = true;
    if (!tex) return nullptr;

    auto view = tex->createView(pixel_format_, 1);
    if (!view) return nullptr;

    GPU::ColorAttachment ca{};
    ca.view          = view;
    ca.loadOp        = preserve_content ? GPU::LoadOp::load : GPU::LoadOp::clear;
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
    {
        it->second.last_used_frame = frame_counter_;
        return it->second.bind_group;
    }

    GPU::BindGroupDescriptor bgDesc{};
    bgDesc.layout  = quad_tex_bgl_;
    bgDesc.entries = {
        GPU::BindGroupEntryDescriptor{ 0, tex },
        GPU::BindGroupEntryDescriptor{ 1, quad_sampler_ },
    };
    auto bindGroup = device_->createBindGroup(bgDesc);
    if (!bindGroup) return nullptr;

    blur_source_bind_group_cache_.emplace(tex.get(),
        SourceBindGroupCacheEntry{ tex, bindGroup, frame_counter_ });
    return bindGroup;
}

void D3DDrawBackend::evictStaleSourceBindGroups()
{
    for (auto it = blur_source_bind_group_cache_.begin(); it != blur_source_bind_group_cache_.end(); )
    {
        if (frame_counter_ - it->second.last_used_frame > kMaxSourceBindGroupCacheAgeFrames)
            it = blur_source_bind_group_cache_.erase(it);
        else
            ++it;
    }
}

std::shared_ptr<GPU::Texture> D3DDrawBackend::blurTexture(
    std::shared_ptr<GPU::Texture> source,
    float sigma_x, float sigma_y,
    GPU::CommandEncoder& encoder)
{
    if (!source || !blur_pipeline_) return nullptr;

    const uint32_t tw = static_cast<uint32_t>(source->getWidth());
    const uint32_t th = static_cast<uint32_t>(source->getHeight());

    // Acquired from blur_texture_pool_ — see its doc comment for why this
    // must be a size-keyed, generation-rotated pool (matching every other
    // offscreen consumer) rather than one fixed-size pair shared by every
    // caller: a UI can legitimately ask for more than one distinct blur
    // size across a session (or within the same frame), and a fixed pair
    // used to force-recreate every generation on any size change —
    // including a generation the GPU might still be mid-flight reading
    // from a prior frame under campello_gpu's 2-deep frame pipelining.
    const auto usage = static_cast<GPU::TextureUsage>(
        static_cast<int>(GPU::TextureUsage::renderTarget) |
        static_cast<int>(GPU::TextureUsage::textureBinding));
    auto h_tex = blur_texture_pool_.acquire(*device_, tw, th, pixel_format_, usage, frame_counter_);
    auto v_tex = blur_texture_pool_.acquire(*device_, tw, th, pixel_format_, usage, frame_counter_);

    // Horizontal blur: source -> h_tex. `source` rotates through
    // OffscreenTexturePool, but lookupOrCreateSourceBindGroup() caches per
    // texture object, so this stays cheap once the pool warms up.
    runBlurPass(source, h_tex, sigma_x, /*horizontal=*/true, encoder,
                lookupOrCreateSourceBindGroup(source));
    // Vertical blur: h_tex -> v_tex.
    runBlurPass(h_tex, v_tex, sigma_y, /*horizontal=*/false, encoder,
                lookupOrCreateSourceBindGroup(h_tex));

    return v_tex;
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
// ShaderMask compositing
// ---------------------------------------------------------------------------

std::shared_ptr<GPU::Texture> D3DDrawBackend::buildGradientLUT(
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

        data[i * 4 + 0] = static_cast<uint8_t>(c.r * 255.0f);
        data[i * 4 + 1] = static_cast<uint8_t>(c.g * 255.0f);
        data[i * 4 + 2] = static_cast<uint8_t>(c.b * 255.0f);
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

void D3DDrawBackend::drawShaderMaskComposite(
    std::shared_ptr<GPU::Texture> child_tex,
    const DrawShaderMaskBeginCmd& cmd,
    const Matrix4&                transform,
    const Rect&                   clip,
    GPU::RenderPassEncoder&       encoder)
{
    if (!shader_mask_pipeline_ || !shader_mask_uniform_bgl_ || !shader_mask_bgl_ || !quad_sampler_ || !child_tex)
        return;
    if (!applyScissor(clip, encoder)) return;

    auto c00 = transform * vm::Vector4<float>(cmd.bounds.left(),  cmd.bounds.top(),    0.0f, 1.0f);
    auto c10 = transform * vm::Vector4<float>(cmd.bounds.right(), cmd.bounds.top(),    0.0f, 1.0f);
    auto c01 = transform * vm::Vector4<float>(cmd.bounds.left(),  cmd.bounds.bottom(), 0.0f, 1.0f);
    auto c11 = transform * vm::Vector4<float>(cmd.bounds.right(), cmd.bounds.bottom(), 0.0f, 1.0f);

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

    auto uniformSlot = shader_mask_uniform_pool_.acquire(*device_, shader_mask_uniform_bgl_, sizeof(ShaderMaskUniforms), &u);
    if (!uniformSlot.bind_group) return;

    GPU::BindGroupDescriptor bgDesc{};
    bgDesc.layout  = shader_mask_bgl_;
    bgDesc.entries = {
        GPU::BindGroupEntryDescriptor{0, child_tex},
        GPU::BindGroupEntryDescriptor{1, lut_tex},
        GPU::BindGroupEntryDescriptor{2, quad_sampler_},
    };
    auto texBindGroup = device_->createBindGroup(bgDesc);
    if (!texBindGroup) return;

    encoder.setPipeline(shader_mask_pipeline_);
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
