#include "vulkan_draw_backend.hpp"
#include "android_text_rasterizer.hpp"
#include <campello_widgets/ui/draw_command.hpp>
#include <campello_widgets/ui/text_span.hpp>
#include <campello_widgets/ui/text_style.hpp>
#include <campello_widgets/ui/rect.hpp>

#include <campello_gpu/device.hpp>
#include <campello_gpu/render_pipeline.hpp>
#include <campello_gpu/bind_group_layout.hpp>
#include <campello_gpu/pipeline_layout.hpp>
#include <campello_gpu/sampler.hpp>
#include <campello_gpu/texture.hpp>
#include <campello_gpu/render_pass_encoder.hpp>
#include <campello_gpu/buffer.hpp>
#include <campello_gpu/constants/buffer_usage.hpp>
#include <campello_gpu/constants/texture_usage.hpp>
#include <campello_gpu/constants/shader_stage.hpp>
#include <campello_gpu/constants/primitive_topology.hpp>

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

struct alignas(16) QuadUniforms
{
    float dstRect[4];   // x, y, w, h
    float srcRect[4];   // u0, v0, u1, v1
    float viewport[2];  // w, h
    float opacity;
    float _pad;
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
    auto rect_vert = loadSpv(device_, shaders::krect_vert_spv, shaders::krect_vert_spvSize);
    auto rect_frag = loadSpv(device_, shaders::krect_frag_spv, shaders::krect_frag_spvSize);
    auto quad_vert = loadSpv(device_, shaders::kquad_vert_spv,  shaders::kquad_vert_spvSize);
    auto quad_frag = loadSpv(device_, shaders::kquad_frag_spv,  shaders::kquad_frag_spvSize);

    if (!rect_vert || !rect_frag || !quad_vert || !quad_frag) {
        // Shader loading failed — pipelines remain null, backend is effectively no-op
        return;
    }

    // Bind group layout: uniform buffer @ binding 0 (vertex + fragment)
    {
        GPU::EntryObject entry{};
        entry.binding    = 0;
        entry.visibility = static_cast<GPU::ShaderStage>(
            static_cast<int>(GPU::ShaderStage::vertex) |
            static_cast<int>(GPU::ShaderStage::fragment));
        entry.type       = GPU::EntryObjectType::buffer;
        entry.data.buffer.type             = GPU::EntryObjectBufferType::uniform;
        entry.data.buffer.hasDinamicOffaset = false;
        entry.data.buffer.minBindingSize   = sizeof(RectUniforms);

        GPU::BindGroupLayoutDescriptor desc{};
        desc.entries = { entry };
        uniforms_bgl_ = device_->createBindGroupLayout(desc);
    }

    // Bind group layout: texture @ 1, sampler @ 2 (fragment only)
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

    // Text rasterizer
    text_rasterizer_ = std::make_unique<AndroidTextRasterizer>();

    // Shared blend state
    auto blend = premultipliedAlphaBlend();

    // Rect pipeline
    {
        GPU::RenderPipelineDescriptor desc{};
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
    }

    // Quad pipeline
    {
        GPU::RenderPipelineDescriptor desc{};
        desc.topology    = GPU::PrimitiveTopology::triangleList;
        desc.cullMode    = GPU::CullMode::none;
        desc.frontFace   = GPU::FrontFace::ccw;

        desc.vertex.module     = quad_vert;
        desc.vertex.entryPoint = "main";

        GPU::FragmentDescriptor frag{};
        frag.module     = quad_frag;
        frag.entryPoint = "main";
        frag.targets    = { GPU::ColorState{ pixel_format_, GPU::ColorWrite::all, blend } };
        desc.fragment   = frag;

        quad_pipeline_ = device_->createRenderPipeline(desc);
    }
}

// ---------------------------------------------------------------------------
// Scissor
// ---------------------------------------------------------------------------

void VulkanDrawBackend::applyScissor(
    const Rect& clip,
    GPU::RenderPassEncoder& encoder)
{
    float x = std::max(0.0f, clip.left());
    float y = std::max(0.0f, clip.top());
    float w = std::max(0.0f, clip.width);
    float h = std::max(0.0f, clip.height);

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
    if (!rect_pipeline_ || !uniforms_bgl_) return;

    namespace vm = systems::leal::vector_math;

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

    RectUniforms u{};
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

    auto ubuf = device_->createBuffer(sizeof(RectUniforms),
                                       static_cast<GPU::BufferUsage>(
                                           static_cast<int>(GPU::BufferUsage::uniform) |
                                           static_cast<int>(GPU::BufferUsage::copyDst)),
                                       &u);
    if (!ubuf) return;

    GPU::BindGroupDescriptor bg_desc{};
    bg_desc.layout = uniforms_bgl_;
    bg_desc.entries = {
        { 0, GPU::BufferBinding{ ubuf, 0, sizeof(RectUniforms) } }
    };
    auto bind_group = device_->createBindGroup(bg_desc);
    if (!bind_group) return;

    applyScissor(clip, encoder);
    encoder.setPipeline(rect_pipeline_);
    encoder.setBindGroup(0, bind_group);
    encoder.draw(6);
}

// ---------------------------------------------------------------------------
// Internal textured-quad helper
// ---------------------------------------------------------------------------

void VulkanDrawBackend::drawTexturedQuad(
    std::shared_ptr<GPU::Texture>    texture,
    const Rect&                      dst_rect,
    const Rect&                      src_rect,
    float                            opacity,
    const Rect&                      clip,
    GPU::RenderPassEncoder&          encoder)
{
    if (!quad_pipeline_ || !uniforms_bgl_ || !quad_bgl_ || !linear_sampler_) return;
    if (!texture) return;

    QuadUniforms u{};
    u.dstRect[0] = dst_rect.left();
    u.dstRect[1] = dst_rect.top();
    u.dstRect[2] = dst_rect.width;
    u.dstRect[3] = dst_rect.height;
    u.srcRect[0] = src_rect.left();
    u.srcRect[1] = src_rect.top();
    u.srcRect[2] = src_rect.right();
    u.srcRect[3] = src_rect.bottom();
    u.viewport[0] = vp_w_;
    u.viewport[1] = vp_h_;
    u.opacity = opacity;

    auto ubuf = device_->createBuffer(sizeof(QuadUniforms),
                                       static_cast<GPU::BufferUsage>(
                                           static_cast<int>(GPU::BufferUsage::uniform) |
                                           static_cast<int>(GPU::BufferUsage::copyDst)),
                                       &u);
    if (!ubuf) return;

    GPU::BindGroupDescriptor ubg_desc{};
    ubg_desc.layout = uniforms_bgl_;
    ubg_desc.entries = {
        { 0, GPU::BufferBinding{ ubuf, 0, sizeof(QuadUniforms) } }
    };
    auto u_bind_group = device_->createBindGroup(ubg_desc);
    if (!u_bind_group) return;

    GPU::BindGroupDescriptor tbg_desc{};
    tbg_desc.layout = quad_bgl_;
    tbg_desc.entries = {
        { 1, texture },
        { 2, linear_sampler_ }
    };
    auto t_bind_group = device_->createBindGroup(tbg_desc);
    if (!t_bind_group) return;

    applyScissor(clip, encoder);
    encoder.setPipeline(quad_pipeline_);
    encoder.setBindGroup(0, u_bind_group);
    encoder.setBindGroup(1, t_bind_group);
    encoder.draw(6);
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

    auto tl = transform * vm::Vector4<float>(cmd.dst_rect.left(),  cmd.dst_rect.top(),    0.0f, 1.0f);
    auto tr = transform * vm::Vector4<float>(cmd.dst_rect.right(), cmd.dst_rect.top(),    0.0f, 1.0f);
    auto bl = transform * vm::Vector4<float>(cmd.dst_rect.left(),  cmd.dst_rect.bottom(), 0.0f, 1.0f);
    auto br = transform * vm::Vector4<float>(cmd.dst_rect.right(), cmd.dst_rect.bottom(), 0.0f, 1.0f);

    const float min_x = std::min({tl.x(), tr.x(), bl.x(), br.x()});
    const float min_y = std::min({tl.y(), tr.y(), bl.y(), br.y()});
    const float max_x = std::max({tl.x(), tr.x(), bl.x(), br.x()});
    const float max_y = std::max({tl.y(), tr.y(), bl.y(), br.y()});

    const float w = max_x - min_x;
    const float h = max_y - min_y;
    if (w <= 0.0f || h <= 0.0f) return;

    drawTexturedQuad(
        cmd.texture,
        Rect::fromLTWH(min_x, min_y, w, h),
        cmd.src_rect,
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
// drawText
// ---------------------------------------------------------------------------

void VulkanDrawBackend::drawText(
    const DrawTextCmd&               cmd,
    const Matrix4&                   transform,
    const Rect&                      clip,
    GPU::RenderPassEncoder&          encoder)
{
    if (!text_rasterizer_ || !text_rasterizer_->isAvailable()) return;
    if (cmd.span.text.empty()) return;

    auto bitmap = text_rasterizer_->rasterize(cmd.span);
    if (bitmap.width <= 0 || bitmap.height <= 0) return;

    auto texture = device_->createTexture(
        GPU::TextureType::tt2d,
        GPU::PixelFormat::bgra8unorm,
        static_cast<uint32_t>(bitmap.width),
        static_cast<uint32_t>(bitmap.height),
        1, 1, 1,
        GPU::TextureUsage::textureBinding);
    if (!texture) return;

    texture->upload(0, bitmap.pixels.size(), bitmap.pixels.data());

    namespace vm = systems::leal::vector_math;
    auto t_origin = transform * vm::Vector4<float>(cmd.origin.x, cmd.origin.y, 0.0f, 1.0f);

    drawTexturedQuad(
        texture,
        Rect::fromLTWH(t_origin.x(), t_origin.y(),
                       static_cast<float>(bitmap.width),
                       static_cast<float>(bitmap.height)),
        Rect::fromLTWH(0.0f, 0.0f, 1.0f, 1.0f),
        1.0f,
        clip,
        encoder);
}

} // namespace systems::leal::campello_widgets
