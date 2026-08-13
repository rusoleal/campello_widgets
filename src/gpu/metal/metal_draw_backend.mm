#import "metal_draw_backend.hpp"
#include "gpu/path_tessellation.hpp"

#include <iostream>

#import <campello_gpu/device.hpp>
#import <campello_gpu/render_pass_encoder.hpp>
#import <campello_gpu/texture.hpp>
#import <campello_gpu/buffer.hpp>
#import <campello_gpu/constants/buffer_usage.hpp>
#import <campello_gpu/constants/texture_usage.hpp>
#import <campello_gpu/constants/texture_type.hpp>
#import <campello_gpu/constants/filter_mode.hpp>
#import <campello_gpu/constants/wrap_mode.hpp>
#import <campello_gpu/constants/shader_stage.hpp>
#import <campello_gpu/constants/primitive_topology.hpp>
#import <campello_gpu/constants/cull_mode.hpp>
#import <campello_gpu/constants/front_face.hpp>
#import <campello_gpu/descriptors/render_pipeline_descriptor.hpp>
#import <campello_gpu/descriptors/bind_group_layout_descriptor.hpp>
#import <campello_gpu/descriptors/bind_group_descriptor.hpp>
#import <campello_gpu/descriptors/sampler_descriptor.hpp>
#import <campello_gpu/descriptors/vertex_descriptor.hpp>
#import <campello_gpu/constants/index_format.hpp>

#import <CoreText/CoreText.h>
#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

#import "shaders/metal_widgets.h"

#include <algorithm>
#include <vector>
#include <cmath>
#include <cstring>
#include <functional>
#include <vector_math/vector4.hpp>

namespace GPU = systems::leal::campello_gpu;
namespace vm  = systems::leal::vector_math;

using namespace systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// Font resolution
// ---------------------------------------------------------------------------

namespace
{
    // CTFontCreateWithName always returns the family's regular face — bold
    // and italic must be requested explicitly via symbolic traits, or every
    // TextStyle renders identically regardless of font_weight/italic.
    CTFontRef CreateStyledCTFont(NSString* family, CGFloat size, FontWeight weight, bool italic)
    {
        CTFontRef base = CTFontCreateWithName((__bridge CFStringRef)family, size, nullptr);
        if (!base) return nullptr;

        CTFontSymbolicTraits traits = 0;
        if (weight == FontWeight::bold) traits |= kCTFontBoldTrait;
        if (italic)                     traits |= kCTFontItalicTrait;

        if (traits == 0)
            return base;

        CTFontRef styled = CTFontCreateCopyWithSymbolicTraits(base, size, nullptr, traits, traits);
        if (styled)
        {
            CFRelease(base);
            return styled;
        }
        // Family has no matching bold/italic face — fall back to the
        // regular font rather than returning null.
        return base;
    }
}

// ---------------------------------------------------------------------------
// Uniform buffer layouts (must match the Metal structs in widgets.metal)
// ---------------------------------------------------------------------------

struct alignas(16) RectUniforms {
    float color[4];     // r, g, b, a
    float viewport[2];  // width, height
    float _pad[2];
};

struct alignas(16) QuadUniforms {
    float viewport[2];  // width, height (pixels)
    float opacity;      // [0, 1] — scales all pixel channels
    float _pad;
};

// Real per-vertex data for the quad pipeline — see QuadVertexIn's doc
// comment in widgets.metal. Not `alignas(16)`: this is a tightly-packed
// vertex-attribute element (arrayStride must match sizeof(QuadVertex)
// exactly), not a uniform buffer.
struct QuadVertex {
    float x, y, w;   // ambient-transform-projected pixel position + w
    float u, v;       // texture UV for this corner
};

struct alignas(16) ShapeUniforms {
    float rect[4];      // x, y, w, h  (bounding box, pixels)
    float color[4];     // r, g, b, a
    float viewport[2];  // w, h
    float corner_r;     // corner radius (rrect); 0 for circle/oval
    float stroke_w;     // 0 = fill, >0 = stroke width
    float kind;         // 0 = rrect,  1 = circle/oval
    float _pad[3];
};

struct alignas(16) LineUniforms {
    float p1[4];        // xy: start (pixels), zw: unused
    float p2[4];        // xy: end   (pixels), zw: unused
    float color[4];
    float viewport[2];
    float stroke_w;
    float _pad;
};

// ---------------------------------------------------------------------------
// Construction — compile pipelines
// ---------------------------------------------------------------------------

MetalDrawBackend::MetalDrawBackend(
    std::shared_ptr<GPU::Device> device,
    Color                        bg_color,
    GPU::PixelFormat             pixel_format)
    : device_(std::move(device))
    , bg_color_(bg_color)
    , pixel_format_(pixel_format)
{
    using namespace systems::leal::campello_widgets::shaders;

    auto shader = device_->createShaderModule(
        kWidgetsMetalShader,
        static_cast<uint64_t>(kWidgetsMetalShaderSize));
    if (!shader) return;

    // --- Rect pipeline (premultiplied-alpha blend) ---
    {
        GPU::ColorState rectCs{};
        rectCs.format    = pixel_format;
        rectCs.writeMask = GPU::ColorWrite::all;
        rectCs.blend = GPU::BlendState{
            .color = { GPU::BlendFactor::one, GPU::BlendFactor::oneMinusSrcAlpha, GPU::BlendOperation::add },
            .alpha = { GPU::BlendFactor::one, GPU::BlendFactor::oneMinusSrcAlpha, GPU::BlendOperation::add },
        };

        GPU::RenderPipelineDescriptor desc{};
        desc.vertex.module     = shader;
        desc.vertex.entryPoint = "rectVertex";

        // Real per-vertex position(+w) data — see RectVertex's doc comment
        // in metal_draw_backend.hpp and RectVertexIn's in widgets.metal. Bound to slot 0 via
        // setVertexBuffer(0, ...); RectUniforms (color/viewport) moves to
        // slot 1 (see drawFilledQuad()).
        GPU::VertexAttribute rectPosAttr{};
        rectPosAttr.componentType  = GPU::ComponentType::ctFloat;
        rectPosAttr.accessorType   = GPU::AccessorType::acVec3;
        rectPosAttr.offset         = 0;
        rectPosAttr.shaderLocation = 0;

        GPU::VertexLayout rectLayout{};
        rectLayout.arrayStride = sizeof(RectVertex);
        rectLayout.attributes  = {rectPosAttr};
        rectLayout.stepMode    = GPU::StepMode::vertex;

        desc.vertex.buffers = {rectLayout};

        GPU::FragmentDescriptor frag{};
        frag.module     = shader;
        frag.entryPoint = "rectFragment";
        frag.targets.push_back(rectCs);
        desc.fragment = frag;

        desc.topology  = GPU::PrimitiveTopology::triangleList;
        desc.cullMode  = GPU::CullMode::none;
        desc.frontFace = GPU::FrontFace::ccw;

        rect_pipeline_ = device_->createRenderPipeline(desc);
    }

    // --- Quad (textured) pipeline — premultiplied-alpha blend ---
    {
        GPU::RenderPipelineDescriptor desc{};
        desc.vertex.module     = shader;
        desc.vertex.entryPoint = "quadVertex";

        // Real per-vertex position(+w)/uv data — see QuadVertex's doc
        // comment above and QuadVertexIn's in widgets.metal. Bound to
        // slot 0 via setVertexBuffer(0, ...); the pipeline's own uniforms
        // (viewport/opacity) move to slot 1 accordingly (see
        // drawTexturedQuad()).
        GPU::VertexAttribute posAttr{};
        posAttr.componentType = GPU::ComponentType::ctFloat;
        posAttr.accessorType  = GPU::AccessorType::acVec3;
        posAttr.offset        = offsetof(QuadVertex, x);
        posAttr.shaderLocation = 0;

        GPU::VertexAttribute uvAttr{};
        uvAttr.componentType = GPU::ComponentType::ctFloat;
        uvAttr.accessorType  = GPU::AccessorType::acVec2;
        uvAttr.offset         = offsetof(QuadVertex, u);
        uvAttr.shaderLocation = 1;

        GPU::VertexLayout quadLayout{};
        quadLayout.arrayStride = sizeof(QuadVertex);
        quadLayout.attributes  = {posAttr, uvAttr};
        quadLayout.stepMode    = GPU::StepMode::vertex;

        desc.vertex.buffers = {quadLayout};

        GPU::ColorState quadCs{};
        quadCs.format    = pixel_format;
        quadCs.writeMask = GPU::ColorWrite::all;
        quadCs.blend = GPU::BlendState{
            .color = { GPU::BlendFactor::one, GPU::BlendFactor::oneMinusSrcAlpha, GPU::BlendOperation::add },
            .alpha = { GPU::BlendFactor::one, GPU::BlendFactor::oneMinusSrcAlpha, GPU::BlendOperation::add },
        };

        GPU::FragmentDescriptor frag{};
        frag.module     = shader;
        frag.entryPoint = "quadFragment";
        frag.targets.push_back(quadCs);
        desc.fragment = frag;

        desc.topology  = GPU::PrimitiveTopology::triangleList;
        desc.cullMode  = GPU::CullMode::none;
        desc.frontFace = GPU::FrontFace::ccw;

        quad_pipeline_ = device_->createRenderPipeline(desc);
    }

    // --- Shape pipeline (SDF circle/oval/rrect) — premultiplied-alpha blend ---
    {
        GPU::ColorState cs{};
        cs.format    = pixel_format;
        cs.writeMask = GPU::ColorWrite::all;
        cs.blend = GPU::BlendState{
            .color = { GPU::BlendFactor::one, GPU::BlendFactor::oneMinusSrcAlpha, GPU::BlendOperation::add },
            .alpha = { GPU::BlendFactor::one, GPU::BlendFactor::oneMinusSrcAlpha, GPU::BlendOperation::add },
        };

        GPU::RenderPipelineDescriptor desc{};
        desc.vertex.module     = shader;
        desc.vertex.entryPoint = "shapeVertex";

        GPU::FragmentDescriptor frag{};
        frag.module     = shader;
        frag.entryPoint = "shapeFragment";
        frag.targets.push_back(cs);
        desc.fragment = frag;

        desc.topology  = GPU::PrimitiveTopology::triangleList;
        desc.cullMode  = GPU::CullMode::none;
        desc.frontFace = GPU::FrontFace::ccw;

        shape_pipeline_ = device_->createRenderPipeline(desc);
    }

    // --- Line pipeline — reuses rectFragment, custom lineVertex ---
    {
        GPU::ColorState cs{};
        cs.format    = pixel_format;
        cs.writeMask = GPU::ColorWrite::all;
        cs.blend = GPU::BlendState{
            .color = { GPU::BlendFactor::one, GPU::BlendFactor::oneMinusSrcAlpha, GPU::BlendOperation::add },
            .alpha = { GPU::BlendFactor::one, GPU::BlendFactor::oneMinusSrcAlpha, GPU::BlendOperation::add },
        };

        GPU::RenderPipelineDescriptor desc{};
        desc.vertex.module     = shader;
        desc.vertex.entryPoint = "lineVertex";

        GPU::FragmentDescriptor frag{};
        frag.module     = shader;
        frag.entryPoint = "rectFragment";
        frag.targets.push_back(cs);
        desc.fragment = frag;

        desc.topology  = GPU::PrimitiveTopology::triangleList;
        desc.cullMode  = GPU::CullMode::none;
        desc.frontFace = GPU::FrontFace::ccw;

        line_pipeline_ = device_->createRenderPipeline(desc);
    }

    // --- Bind group layout for textured quad (texture@0, sampler@1) ---
    {
        GPU::BindGroupLayoutDescriptor bglDesc{};

        GPU::EntryObject texEntry{};
        texEntry.binding    = 0;
        texEntry.visibility = GPU::ShaderStage::fragment;
        texEntry.type       = GPU::EntryObjectType::texture;
        texEntry.data.texture.multisampled = false;
        texEntry.data.texture.sampleType   = GPU::EntryObjectTextureType::ttFloat;
        texEntry.data.texture.viewDimension = GPU::TextureType::tt2d;
        bglDesc.entries.push_back(texEntry);

        GPU::EntryObject sampEntry{};
        sampEntry.binding    = 1;
        sampEntry.visibility = GPU::ShaderStage::fragment;
        sampEntry.type       = GPU::EntryObjectType::sampler;
        sampEntry.data.sampler.type = GPU::EntryObjectSamplerType::filtering;
        bglDesc.entries.push_back(sampEntry);

        quad_bgl_ = device_->createBindGroupLayout(bglDesc);
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

    // --- Blur pipeline (reuses quad_bgl_: texture@0, sampler@1) ---
    {
        GPU::ColorState cs{};
        cs.format    = pixel_format;
        cs.writeMask = GPU::ColorWrite::all;
        cs.blend = GPU::BlendState{
            .color = { GPU::BlendFactor::one, GPU::BlendFactor::oneMinusSrcAlpha, GPU::BlendOperation::add },
            .alpha = { GPU::BlendFactor::one, GPU::BlendFactor::oneMinusSrcAlpha, GPU::BlendOperation::add },
        };

        GPU::RenderPipelineDescriptor desc{};
        desc.vertex.module     = shader;
        desc.vertex.entryPoint = "blurVertex";

        GPU::FragmentDescriptor frag{};
        frag.module     = shader;
        frag.entryPoint = "blurFragment";
        frag.targets.push_back(cs);
        desc.fragment = frag;

        desc.topology  = GPU::PrimitiveTopology::triangleList;
        desc.cullMode  = GPU::CullMode::none;
        desc.frontFace = GPU::FrontFace::ccw;

        blur_pipeline_ = device_->createRenderPipeline(desc);
    }

    // --- ShaderMask pipeline (child_tex@0, lut_tex@1, sampler@2) ---
    {
        // Bind group layout: 2 textures + 1 sampler.
        GPU::BindGroupLayoutDescriptor bglDesc{};

        GPU::EntryObject childTex{};
        childTex.binding    = 0;
        childTex.visibility = GPU::ShaderStage::fragment;
        childTex.type       = GPU::EntryObjectType::texture;
        childTex.data.texture.multisampled  = false;
        childTex.data.texture.sampleType    = GPU::EntryObjectTextureType::ttFloat;
        childTex.data.texture.viewDimension = GPU::TextureType::tt2d;
        bglDesc.entries.push_back(childTex);

        GPU::EntryObject lutTex = childTex;
        lutTex.binding = 1;
        bglDesc.entries.push_back(lutTex);

        GPU::EntryObject sampEntry{};
        sampEntry.binding    = 2;
        sampEntry.visibility = GPU::ShaderStage::fragment;
        sampEntry.type       = GPU::EntryObjectType::sampler;
        sampEntry.data.sampler.type = GPU::EntryObjectSamplerType::filtering;
        bglDesc.entries.push_back(sampEntry);

        shader_mask_bgl_ = device_->createBindGroupLayout(bglDesc);

        GPU::ColorState cs{};
        cs.format    = pixel_format;
        cs.writeMask = GPU::ColorWrite::all;
        cs.blend = GPU::BlendState{
            .color = { GPU::BlendFactor::one, GPU::BlendFactor::oneMinusSrcAlpha, GPU::BlendOperation::add },
            .alpha = { GPU::BlendFactor::one, GPU::BlendFactor::oneMinusSrcAlpha, GPU::BlendOperation::add },
        };

        GPU::RenderPipelineDescriptor desc{};
        desc.vertex.module     = shader;
        desc.vertex.entryPoint = "shaderMaskVertex";

        // Same QuadVertex layout as the quad/clip-shape pipelines — see
        // drawTexturedQuad()/drawClipShapeComposite(). Bound to slot 0;
        // ShaderMaskUniforms moves to slot 1.
        GPU::VertexAttribute maskPosAttr{};
        maskPosAttr.componentType  = GPU::ComponentType::ctFloat;
        maskPosAttr.accessorType   = GPU::AccessorType::acVec3;
        maskPosAttr.offset         = offsetof(QuadVertex, x);
        maskPosAttr.shaderLocation = 0;

        GPU::VertexAttribute maskUvAttr{};
        maskUvAttr.componentType  = GPU::ComponentType::ctFloat;
        maskUvAttr.accessorType   = GPU::AccessorType::acVec2;
        maskUvAttr.offset         = offsetof(QuadVertex, u);
        maskUvAttr.shaderLocation = 1;

        GPU::VertexLayout maskLayout{};
        maskLayout.arrayStride = sizeof(QuadVertex);
        maskLayout.attributes  = {maskPosAttr, maskUvAttr};
        maskLayout.stepMode    = GPU::StepMode::vertex;

        desc.vertex.buffers = {maskLayout};

        GPU::FragmentDescriptor frag{};
        frag.module     = shader;
        frag.entryPoint = "shaderMaskFragment";
        frag.targets.push_back(cs);
        desc.fragment = frag;

        desc.topology  = GPU::PrimitiveTopology::triangleList;
        desc.cullMode  = GPU::CullMode::none;
        desc.frontFace = GPU::FrontFace::ccw;

        shader_mask_pipeline_ = device_->createRenderPipeline(desc);
    }

    // --- ClipRRect/ClipOval pipeline (reuses quad_bgl_: texture@0, sampler@1) ---
    {
        GPU::ColorState cs{};
        cs.format    = pixel_format;
        cs.writeMask = GPU::ColorWrite::all;
        cs.blend = GPU::BlendState{
            .color = { GPU::BlendFactor::one, GPU::BlendFactor::oneMinusSrcAlpha, GPU::BlendOperation::add },
            .alpha = { GPU::BlendFactor::one, GPU::BlendFactor::oneMinusSrcAlpha, GPU::BlendOperation::add },
        };

        GPU::RenderPipelineDescriptor desc{};
        desc.vertex.module     = shader;
        desc.vertex.entryPoint = "clipShapeVertex";

        // Real per-vertex position(+w)/uv data — same QuadVertex layout as
        // the quad pipeline (see drawTexturedQuad()); reused directly since
        // drawClipShapeComposite() draws QuadVertex data through
        // quad_vertex_pool_ too. Bound to slot 0; ClipShapeUniforms
        // (rect_size/viewport/corner_r/kind) moves to slot 1.
        GPU::VertexAttribute clipPosAttr{};
        clipPosAttr.componentType  = GPU::ComponentType::ctFloat;
        clipPosAttr.accessorType   = GPU::AccessorType::acVec3;
        clipPosAttr.offset         = offsetof(QuadVertex, x);
        clipPosAttr.shaderLocation = 0;

        GPU::VertexAttribute clipUvAttr{};
        clipUvAttr.componentType  = GPU::ComponentType::ctFloat;
        clipUvAttr.accessorType   = GPU::AccessorType::acVec2;
        clipUvAttr.offset         = offsetof(QuadVertex, u);
        clipUvAttr.shaderLocation = 1;

        GPU::VertexLayout clipLayout{};
        clipLayout.arrayStride = sizeof(QuadVertex);
        clipLayout.attributes  = {clipPosAttr, clipUvAttr};
        clipLayout.stepMode    = GPU::StepMode::vertex;

        desc.vertex.buffers = {clipLayout};

        GPU::FragmentDescriptor frag{};
        frag.module     = shader;
        frag.entryPoint = "clipShapeFragment";
        frag.targets.push_back(cs);
        desc.fragment = frag;

        desc.topology  = GPU::PrimitiveTopology::triangleList;
        desc.cullMode  = GPU::CullMode::none;
        desc.frontFace = GPU::FrontFace::ccw;

        clip_shape_pipeline_ = device_->createRenderPipeline(desc);
    }
}

// ---------------------------------------------------------------------------
// applyScissor — converts the logical-point clip rect to physical pixels and
// sets it as the encoder's scissor rect before issuing a draw call.
// ---------------------------------------------------------------------------

bool MetalDrawBackend::applyScissor(const Rect& clip, GPU::RenderPassEncoder& encoder)
{
    // Convert from logical points to physical pixels.
    float x = clip.x      * dpr_;
    float y = clip.y      * dpr_;
    float w = clip.width  * dpr_;
    float h = clip.height * dpr_;

    // Clamp to the viewport so the scissor is always within drawable bounds.
    const float rx = std::min(x + w, vp_w_);
    const float by = std::min(y + h, vp_h_);
    x = std::max(0.0f, x);
    y = std::max(0.0f, y);
    w = std::max(0.0f, rx - x);
    h = std::max(0.0f, by - y);

    // Metal requires width >= 1 and height >= 1. Skip the draw if the clip
    // region is empty (fully scrolled out of view or zero-sized intersection).
    if (w < 1.0f || h < 1.0f)
        return false;

    // Skip redundant setScissorRect calls — Metal API Validation aborts on them.
    if (x == last_scissor_x_ && y == last_scissor_y_ &&
        w == last_scissor_w_ && h == last_scissor_h_)
    {
        return true;
    }

    encoder.setScissorRect(x, y, w, h);
    last_scissor_x_ = x;
    last_scissor_y_ = y;
    last_scissor_w_ = w;
    last_scissor_h_ = h;
    return true;
}

// ---------------------------------------------------------------------------
// UniformBufferPool
// ---------------------------------------------------------------------------

std::shared_ptr<GPU::Buffer> MetalDrawBackend::UniformBufferPool::acquire(
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

void MetalDrawBackend::UniformBufferPool::beginFrame() noexcept
{
    current_generation_ = (current_generation_ + 1) % kGenerations;
    next_index_[current_generation_] = 0;
}

// ---------------------------------------------------------------------------
// drawRect
// ---------------------------------------------------------------------------

void MetalDrawBackend::drawFilledQuad(
    const ProjectedCorner& c00, const ProjectedCorner& c10,
    const ProjectedCorner& c01, const ProjectedCorner& c11,
    const Color& color,
    GPU::RenderPassEncoder& encoder)
{
    // Two triangles matching kQuadCorners' historical winding order.
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

    auto ubuf = rect_uniform_pool_.acquire(*device_, sizeof(RectUniforms), &u);
    if (!ubuf) return;

    encoder.setPipeline(rect_pipeline_);
    encoder.setVertexBuffer(0, vbuf);
    encoder.setVertexBuffer(1, ubuf);
    encoder.draw(6);
}

void MetalDrawBackend::drawFilledRect(
    float x, float y, float w, float h,
    const Color& color,
    GPU::RenderPassEncoder& encoder)
{
    // Axis-aligned (w always 1 — see this function's doc comment in the
    // header for why: used for the stroke path's already-untransformed
    // edge rects, not for ambient-transform-projected content).
    drawFilledQuad(
        ProjectedCorner{x,     y,     1.0f, 0.0f, 0.0f}, ProjectedCorner{x + w, y,     1.0f, 0.0f, 0.0f},
        ProjectedCorner{x,     y + h, 1.0f, 0.0f, 0.0f}, ProjectedCorner{x + w, y + h, 1.0f, 0.0f, 0.0f},
        color, encoder);
}

void MetalDrawBackend::drawFilledVertices(
    const std::vector<RectVertex>& verts,
    const Color& color,
    GPU::RenderPassEncoder& encoder)
{
    if (!rect_pipeline_ || verts.empty()) return;

    auto vbuf = rect_vertex_pool_.acquire(*device_, verts.size() * sizeof(RectVertex), verts.data());
    if (!vbuf) return;

    RectUniforms u{};
    u.color[0]    = color.r;
    u.color[1]    = color.g;
    u.color[2]    = color.b;
    u.color[3]    = color.a;
    u.viewport[0] = vp_w_;
    u.viewport[1] = vp_h_;

    auto ubuf = rect_uniform_pool_.acquire(*device_, sizeof(RectUniforms), &u);
    if (!ubuf) return;

    encoder.setPipeline(rect_pipeline_);
    encoder.setVertexBuffer(0, vbuf);
    encoder.setVertexBuffer(1, ubuf);
    encoder.draw(static_cast<uint32_t>(verts.size()));
}

void MetalDrawBackend::drawRect(
    const DrawRectCmd&    cmd,
    const Matrix4&        transform,
    const Rect&           clip,
    GPU::RenderPassEncoder& encoder)
{
    if (!rect_pipeline_) return;
    if (!applyScissor(clip, encoder)) return;

    // Transform all four corners independently — not collapsed to an
    // axis-aligned bounding box — so a rotated or perspective-projected
    // Transform renders as a genuinely tilted/trapezoidal quad. See
    // ProjectedCorner's doc comment.
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
        // Stroke: four thin filled rects along each edge of the AABB —
        // still axis-aligned even under rotation (see drawFilledRect()'s
        // doc comment in the header; genuine rotated-stroke rendering is
        // out of scope for this pass — see TODO.md's "Real per-vertex
        // quad rendering" entry).
        const float min_x = std::min({c00.x(), c10.x(), c01.x(), c11.x()});
        const float min_y = std::min({c00.y(), c10.y(), c01.y(), c11.y()});
        const float max_x = std::max({c00.x(), c10.x(), c01.x(), c11.x()});
        const float max_y = std::max({c00.y(), c10.y(), c01.y(), c11.y()});

        const float sw = cmd.paint.stroke_width;
        const float w  = max_x - min_x;
        const float h  = max_y - min_y;

        drawFilledRect(min_x,         min_y,         w,  sw, cmd.paint.color, encoder);
        drawFilledRect(min_x,         max_y - sw,    w,  sw, cmd.paint.color, encoder);
        drawFilledRect(min_x,         min_y + sw,    sw, h - 2.0f * sw, cmd.paint.color, encoder);
        drawFilledRect(max_x - sw,    min_y + sw,    sw, h - 2.0f * sw, cmd.paint.color, encoder);
    }
}

// ---------------------------------------------------------------------------
// drawShape — shared helper for circle, oval, and rounded rect
// ---------------------------------------------------------------------------

void MetalDrawBackend::drawShape(
    float x, float y, float w, float h,
    float corner_r, float stroke_w, float kind,
    const Color& color,
    GPU::RenderPassEncoder& encoder)
{
    if (!shape_pipeline_) return;

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

    auto ubuf = shape_uniform_pool_.acquire(*device_, sizeof(ShapeUniforms), &u);
    if (!ubuf) return;

    encoder.setPipeline(shape_pipeline_);
    encoder.setVertexBuffer(0, ubuf);
    encoder.draw(6);
}

// ---------------------------------------------------------------------------
// drawCircle
// ---------------------------------------------------------------------------

void MetalDrawBackend::drawCircle(
    const DrawCircleCmd&    cmd,
    const Matrix4&          transform,
    const Rect&             clip,
    GPU::RenderPassEncoder& encoder)
{
    if (!shape_pipeline_) return;
    if (!applyScissor(clip, encoder)) return;

    // Apply transform to center
    auto tc = transform * vm::Vector4<float>(cmd.center.x, cmd.center.y, 0.0f, 1.0f);
    // Scale: magnitude of transform applied to unit x-vector
    auto tv = transform * vm::Vector4<float>(1.0f, 0.0f, 0.0f, 0.0f);
    float scale = std::sqrt(tv.x() * tv.x() + tv.y() * tv.y());
    float r = cmd.radius * scale;

    float sw = (cmd.paint.style == PaintStyle::stroke) ? cmd.paint.stroke_width * scale : 0.0f;
    drawShape(tc.x() - r, tc.y() - r, r * 2.0f, r * 2.0f,
              0.0f, sw, 1.0f, cmd.paint.color, encoder);
}

// ---------------------------------------------------------------------------
// drawOval
// ---------------------------------------------------------------------------

void MetalDrawBackend::drawOval(
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

// ---------------------------------------------------------------------------
// drawRRect
// ---------------------------------------------------------------------------

void MetalDrawBackend::drawRRect(
    const DrawRRectCmd&     cmd,
    const Matrix4&          transform,
    const Rect&             clip,
    GPU::RenderPassEncoder& encoder)
{
    if (!shape_pipeline_) return;
    if (!applyScissor(clip, encoder)) return;

    auto tl = transform * vm::Vector4<float>(cmd.rrect.rect.left(), cmd.rrect.rect.top(), 0.0f, 1.0f);
    auto br = transform * vm::Vector4<float>(cmd.rrect.rect.right(), cmd.rrect.rect.bottom(), 0.0f, 1.0f);
    // Scale factor for corner radius
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

void MetalDrawBackend::drawLine(
    const DrawLineCmd&      cmd,
    const Matrix4&          transform,
    const Rect&             clip,
    GPU::RenderPassEncoder& encoder)
{
    if (!line_pipeline_) return;
    if (!applyScissor(clip, encoder)) return;

    auto tp1 = transform * vm::Vector4<float>(cmd.p1.x, cmd.p1.y, 0.0f, 1.0f);
    auto tp2 = transform * vm::Vector4<float>(cmd.p2.x, cmd.p2.y, 0.0f, 1.0f);
    // Scale stroke width
    auto tv = transform * vm::Vector4<float>(1.0f, 0.0f, 0.0f, 0.0f);
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

    auto ubuf = line_uniform_pool_.acquire(*device_, sizeof(LineUniforms), &u);
    if (!ubuf) return;

    encoder.setPipeline(line_pipeline_);
    encoder.setVertexBuffer(0, ubuf);
    encoder.draw(6);
}

// ---------------------------------------------------------------------------
// drawArc — tessellate to triangles and draw via rect_pipeline_
// ---------------------------------------------------------------------------

void MetalDrawBackend::drawArc(
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

void MetalDrawBackend::drawPath(
    const DrawPathCmd&      cmd,
    const Matrix4&          transform,
    const Rect&             clip,
    GPU::RenderPassEncoder& encoder)
{
    if (!rect_pipeline_) return;

    auto contours = buildPathContours(cmd.path);
    if (contours.empty()) return;

    std::vector<RectVertex> verts;

    auto tv = transform * vm::Vector4<float>(1.0f, 0.0f, 0.0f, 0.0f);
    float scale = std::sqrt(tv.x() * tv.x() + tv.y() * tv.y());

    if (cmd.paint.style == PaintStyle::stroke)
    {
        float half_sw = std::max(0.5f, cmd.paint.stroke_width * scale * 0.5f);
        for (const auto& contour : contours)
        {
            for (size_t i = 0; i + 1 < contour.size(); ++i)
            {
                const auto& a = contour[i];
                const auto& b = contour[i + 1];
                float dx = b.x - a.x;
                float dy = b.y - a.y;
                float len = std::sqrt(dx * dx + dy * dy);
                if (len < 1e-4f) continue;
                float nx = -dy / len * half_sw;
                float ny =  dx / len * half_sw;

                auto p00 = transform * vm::Vector4<float>(a.x + nx, a.y + ny, 0.0f, 1.0f);
                auto p01 = transform * vm::Vector4<float>(a.x - nx, a.y - ny, 0.0f, 1.0f);
                auto p10 = transform * vm::Vector4<float>(b.x + nx, b.y + ny, 0.0f, 1.0f);
                auto p11 = transform * vm::Vector4<float>(b.x - nx, b.y - ny, 0.0f, 1.0f);

                verts.push_back({p01.x(), p01.y(), p01.w()});
                verts.push_back({p00.x(), p00.y(), p00.w()});
                verts.push_back({p11.x(), p11.y(), p11.w()});
                verts.push_back({p11.x(), p11.y(), p11.w()});
                verts.push_back({p00.x(), p00.y(), p00.w()});
                verts.push_back({p10.x(), p10.y(), p10.w()});
            }
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
}

// ---------------------------------------------------------------------------
// drawPoints — decompose to circles/lines using existing pipelines
// ---------------------------------------------------------------------------

void MetalDrawBackend::drawPoints(
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
            for (size_t i = 0; i + 1 < cmd.points.size(); ++i)
            {
                DrawLineCmd line{cmd.points[i], cmd.points[i + 1], p};
                drawLine(line, transform, clip, encoder);
            }
            DrawLineCmd close{cmd.points.back(), cmd.points.front(), p};
            drawLine(close, transform, clip, encoder);
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// saveLayerComposite — draw the offscreen child texture modulated by opacity
// ---------------------------------------------------------------------------

void MetalDrawBackend::saveLayerComposite(
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
// measureText — query CoreText for the real typographic bounding box
// ---------------------------------------------------------------------------

systems::leal::campello_widgets::Size MetalDrawBackend::measureText(const TextSpan& span) const
{
    if (span.text.empty())
        return Size::zero();

    @autoreleasepool {
        NSString *nsText = [NSString stringWithUTF8String:span.text.c_str()];
        if (!nsText || nsText.length == 0)
            return Size::zero();

        const float fontSize = span.style.font_size > 0.0f ? span.style.font_size : 14.0f;

        NSString *family = span.style.font_family.empty()
                           ? @"Helvetica Neue"
                           : [NSString stringWithUTF8String:span.style.font_family.c_str()];

        CTFontRef ctFont = CreateStyledCTFont(
            family, (CGFloat)fontSize, span.style.font_weight, span.style.italic);
        if (!ctFont)
            return Size::zero();

        NSDictionary *attrs = @{ (__bridge NSString*)kCTFontAttributeName: (__bridge id)ctFont };
        CFRelease(ctFont);

        NSAttributedString *attrStr =
            [[NSAttributedString alloc] initWithString:nsText attributes:attrs];

        // Use CTLine instead of CTFramesetter to get proper width including trailing whitespace.
        // CTFramesetterSuggestFrameSizeWithConstraints excludes trailing whitespace,
        // which causes cursor positioning issues when typing spaces.
        CTLineRef line = CTLineCreateWithAttributedString(
            (__bridge CFAttributedStringRef)attrStr);
        
        CGFloat ascent, descent, leading;
        double width = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);
        CFRelease(line);

        // Height is ascent + descent + leading (line gap)
        CGFloat height = ascent + descent + leading;

        return Size{ (float)width, (float)height };
    }
}

// ---------------------------------------------------------------------------
// rasterizeText — CoreText glyph rasterization only, no caching (Renderer's
// text_texture_cache_ owns that — see its doc comment).
// ---------------------------------------------------------------------------

std::shared_ptr<GPU::Texture> MetalDrawBackend::rasterizeText(
    const TextSpan& span, float /*dpr*/,
    uint32_t& out_width, uint32_t& out_height)
{
    @autoreleasepool {
        NSString *nsText = [NSString stringWithUTF8String:span.text.c_str()];
        if (!nsText || nsText.length == 0) return nullptr;

        // The font_size stored in the span has already been scaled to physical
        // pixels by RenderText/RenderParagraph (they multiply by
        // activeDevicePixelRatio() before emitting the DrawTextCmd).
        // We use it directly so the CoreText bitmap is in physical pixels.
        const float physicalFontSize = span.style.font_size > 0.0f
                                       ? span.style.font_size : 14.0f;

        // Build font
        NSString *family = span.style.font_family.empty()
                           ? @"Helvetica Neue"
                           : [NSString stringWithUTF8String:span.style.font_family.c_str()];

        CTFontRef ctFont = CreateStyledCTFont(
            family, (CGFloat)physicalFontSize, span.style.font_weight, span.style.italic);
        if (!ctFont) return nullptr;

        // Text color
        const Color& tc = span.style.color;
        CGFloat comps[] = { (CGFloat)tc.r, (CGFloat)tc.g, (CGFloat)tc.b, (CGFloat)tc.a };
        CGColorSpaceRef rgbCS = CGColorSpaceCreateDeviceRGB();
        CGColorRef cgTextColor = CGColorCreate(rgbCS, comps);
        CGColorSpaceRelease(rgbCS);

        // Attributed string
        NSDictionary *attrs = @{
            (__bridge NSString*)kCTFontAttributeName:
                (__bridge id)ctFont,
            (__bridge NSString*)kCTForegroundColorAttributeName:
                (__bridge id)cgTextColor,
        };
        CFRelease(cgTextColor);
        CFRelease(ctFont);

        NSAttributedString *attrStr =
            [[NSAttributedString alloc] initWithString:nsText attributes:attrs];

        // Measure using CTLine to match measureText() and include trailing whitespace.
        // CTFramesetterSuggestFrameSizeWithConstraints excludes trailing whitespace
        // which would cause misalignment between rendered text and cursor position.
        CTLineRef measureLine = CTLineCreateWithAttributedString(
            (__bridge CFAttributedStringRef)attrStr);
        CGFloat ascent, descent, leading;
        double width = CTLineGetTypographicBounds(measureLine, &ascent, &descent, &leading);
        CFRelease(measureLine);

        CGSize fitSize = CGSizeMake((CGFloat)width, ascent + descent + leading);

        if (fitSize.width <= 0.0 || fitSize.height <= 0.0) return nullptr;

        // Texture dimensions in physical pixels (add small padding for anti-aliasing)
        uint32_t texW = (uint32_t)ceil(fitSize.width)  + 2;
        uint32_t texH = (uint32_t)ceil(fitSize.height) + 2;

        // Compute baseline offset: CoreText uses Quartz coords (y+ up)
        // Reuse the descent from CTLineGetTypographicBounds above.
        CGFloat fontDescent = fabs(descent);

        // Allocate BGRA8 pixel buffer
        std::vector<uint8_t> pixels(texW * texH * 4, 0);

        // Create CGBitmapContext (BGRA8 premultiplied — matches bgra8unorm)
        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        CGContextRef cgCtx = CGBitmapContextCreate(
            pixels.data(), texW, texH, 8, texW * 4, colorSpace,
            (CGBitmapInfo)((uint32_t)kCGBitmapByteOrder32Little | (uint32_t)kCGImageAlphaPremultipliedFirst));
        CGColorSpaceRelease(colorSpace);
        if (!cgCtx) return nullptr;

        // Draw text — baseline at y = descent + 1 (1px bottom padding)
        CGContextSetTextMatrix(cgCtx, CGAffineTransformIdentity);
        CTLineRef line = CTLineCreateWithAttributedString(
            (__bridge CFAttributedStringRef)attrStr);
        CGContextSetTextPosition(cgCtx, 1.0, fontDescent + 1.0);
        CTLineDraw(line, cgCtx);
        CFRelease(line);
        CGContextRelease(cgCtx);

        // Upload to GPU texture (bgra8unorm)
        auto texture = device_->createTexture(
            GPU::TextureType::tt2d,
            GPU::PixelFormat::bgra8unorm,
            texW, texH, 1, 1, 1,
            GPU::TextureUsage::textureBinding);
        if (!texture) return nullptr;
        texture->upload(0, (uint64_t)pixels.size(), pixels.data());

        out_width  = texW;
        out_height = texH;
        return texture;
    }
}

// ---------------------------------------------------------------------------
// drawTextTexture — draws an already-rasterized text texture (from
// rasterizeText(), cached or fresh) as a quad.
// ---------------------------------------------------------------------------

std::shared_ptr<GPU::BindGroup> MetalDrawBackend::drawTextTexture(
    std::shared_ptr<GPU::Texture>   texture,
    std::shared_ptr<GPU::BindGroup> cached_bind_group,
    uint32_t width, uint32_t height,
    const Offset&            origin,
    const Matrix4&           transform,
    const Rect&              clip,
    GPU::RenderPassEncoder&  encoder)
{
    if (!quad_pipeline_ || !quad_bgl_ || !quad_sampler_) return nullptr;
    if (!texture) return nullptr;
    if (!applyScissor(clip, encoder)) return nullptr;

    // Transform the logical origin to physical pixels.
    auto t_origin = transform * vm::Vector4<float>(origin.x, origin.y, 0.0f, 1.0f);

    // Place the quad at the physical-pixel origin. The texture is already
    // in physical pixels, so its pixel dimensions are the correct quad size.
    // Subtract the 1-physical-pixel padding baked into the rasterized
    // texture. Note: unlike drawImage()/drawBackdropFilter() below, this
    // only transforms the origin — width/height are added post-transform in
    // physical pixels, unaffected by any rotation/scale/perspective in
    // `transform`, matching this function's behavior before the quad
    // pipeline gained real per-vertex corners. Text rotation/perspective
    // is out of scope for this pass; w is always 1 here (no projection).
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

// ---------------------------------------------------------------------------
// drawImage
// ---------------------------------------------------------------------------

void MetalDrawBackend::drawImage(
    const DrawImageCmd&   cmd,
    const Matrix4&        transform,
    const Rect&           clip,
    GPU::RenderPassEncoder& encoder)
{
    if (!quad_pipeline_ || !quad_bgl_ || !quad_sampler_) return;
    if (!cmd.texture) return;
    if (!applyScissor(clip, encoder)) return;

    // Apply the current transform (which includes the DPR scale) to the
    // destination rect's four corners *independently* — not collapsed to
    // an axis-aligned bounding box — so a rotated or perspective-projected
    // Transform renders as a genuinely tilted/trapezoidal quad instead of
    // a resized axis-aligned box. See ProjectedCorner's doc comment.
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
// drawTexturedQuad — shared helper
// ---------------------------------------------------------------------------

std::shared_ptr<GPU::BindGroup> MetalDrawBackend::drawTexturedQuad(
    std::shared_ptr<GPU::Texture>  texture,
    const ProjectedCorner& c00, const ProjectedCorner& c10,
    const ProjectedCorner& c01, const ProjectedCorner& c11,
    float opacity,
    GPU::RenderPassEncoder&        encoder,
    std::shared_ptr<GPU::BindGroup> cached_bind_group)
{
    if (!quad_pipeline_) { std::cerr << "[MetalDrawBackend] No pipeline!\n"; return nullptr; }
    if (!quad_bgl_) { std::cerr << "[MetalDrawBackend] No bind group layout!\n"; return nullptr; }
    if (!quad_sampler_) { std::cerr << "[MetalDrawBackend] No sampler!\n"; return nullptr; }

    // Reuse the caller-supplied bind group if there is one (see
    // Renderer::text_texture_cache_'s doc comment — cached alongside the
    // texture there, same lifetime), otherwise build one (drawImage /
    // drawBackdropFilter, whose source textures aren't cached here).
    auto bindGroup = cached_bind_group;
    if (!bindGroup)
    {
        GPU::BindGroupDescriptor bgDesc{};
        bgDesc.layout  = quad_bgl_;
        bgDesc.entries = {
            GPU::BindGroupEntryDescriptor{ 0, texture },
            GPU::BindGroupEntryDescriptor{ 1, quad_sampler_ },
        };
        bindGroup = device_->createBindGroup(bgDesc);
        if (!bindGroup) { std::cerr << "[MetalDrawBackend] Failed to create bind group!\n"; return nullptr; }
    }

    // Real per-vertex position(+w)/uv data — two triangles matching
    // kQuadCorners' historical winding order: (00,10,01), (01,10,11).
    const QuadVertex verts[6] = {
        {c00.x, c00.y, c00.w, c00.u, c00.v},
        {c10.x, c10.y, c10.w, c10.u, c10.v},
        {c01.x, c01.y, c01.w, c01.u, c01.v},
        {c01.x, c01.y, c01.w, c01.u, c01.v},
        {c10.x, c10.y, c10.w, c10.u, c10.v},
        {c11.x, c11.y, c11.w, c11.u, c11.v},
    };
    auto vbuf = quad_vertex_pool_.acquire(*device_, sizeof(verts), verts);
    if (!vbuf) return bindGroup;

    QuadUniforms u{};
    u.viewport[0] = vp_w_;
    u.viewport[1] = vp_h_;
    u.opacity     = opacity;

    auto ubuf = quad_uniform_pool_.acquire(*device_, sizeof(QuadUniforms), &u);
    if (!ubuf) return bindGroup;

    encoder.setPipeline(quad_pipeline_);
    encoder.setBindGroup(0, bindGroup);
    encoder.setVertexBuffer(0, vbuf);
    encoder.setVertexBuffer(1, ubuf);
    encoder.draw(6);
    return bindGroup;
}

// ---------------------------------------------------------------------------
// Offscreen / compositing support
// ---------------------------------------------------------------------------

struct alignas(16) BlurUniforms {
    float dstRect[4];    // x, y, w, h (pixels, destination quad)
    float srcRect[4];    // u0, v0, u1, v1 (normalised UV)
    float viewport[2];   // framebuffer width, height
    float sigma;
    float horizontal;    // 1.0 = H pass, 0.0 = V pass
    float tex_size[2];   // source texture width, height
    float _pad[2];
};

struct alignas(16) ShaderMaskUniforms {
    float viewport[2];
    float gradient_type;     // 0 = linear, 1 = radial
    float _pad0;
    float gradient_p1[4];    // linear: begin.xy; radial: center.xy
    float gradient_p2[4];    // linear: end.xy;   radial: radius in [0]
    float blend_mode;        // 0 = srcIn, 1 = modulate
    float _pad1[3];
};

struct alignas(16) ClipShapeUniforms {
    float rect_size[2]; // bounds' plain LOGICAL width/height (not physical
                         // pixels, not transform-scaled) — the rounded-rect/
                         // ellipse SDF is evaluated in the shape's own local
                         // space via perspective-correctly-interpolated UV
                         // (see ClipShapeVertexIn's doc comment in
                         // widgets.metal), so it needs the shape's true
                         // logical size, independent of how the destination
                         // quad ends up projected on screen.
    float viewport[2];  // framebuffer width, height — for the vertex
                         // shader's clip-space conversion only
    float corner_r;      // LOGICAL corner radius (rrect); ignored when kind==1
    float kind;          // 0 = rounded rect, 1 = ellipse/oval
    float _pad[2];
};

std::shared_ptr<GPU::Texture> MetalDrawBackend::createOffscreenTexture(
    uint32_t width, uint32_t height)
{
    return offscreen_texture_pool_.acquire(*device_, width, height, pixel_format_,
        static_cast<GPU::TextureUsage>(
            static_cast<int>(GPU::TextureUsage::renderTarget) |
            static_cast<int>(GPU::TextureUsage::textureBinding) |
            static_cast<int>(GPU::TextureUsage::copySrc)),
        frame_counter_);
}

std::shared_ptr<GPU::Texture> MetalDrawBackend::createDedicatedOffscreenTexture(
    uint32_t width, uint32_t height)
{
    // Bypasses offscreen_texture_pool_ entirely — see this method's doc
    // comment on IDrawBackend for why a pooled texture is unsafe for a
    // caller that keeps referencing it (and relying on its content) across
    // many future frames.
    // copyDst (in addition to copySrc) so a RenderDrawSurface-style caller
    // can blit a previous dedicated texture's content into this one on
    // resize (see Renderer::applyDrawSurfaceUpdate()'s blit_source path).
    return device_->createTexture(
        GPU::TextureType::tt2d, pixel_format_, width, height, 1, 1, 1,
        static_cast<GPU::TextureUsage>(
            static_cast<int>(GPU::TextureUsage::renderTarget) |
            static_cast<int>(GPU::TextureUsage::textureBinding) |
            static_cast<int>(GPU::TextureUsage::copySrc) |
            static_cast<int>(GPU::TextureUsage::copyDst)));
}

std::shared_ptr<GPU::Texture> MetalDrawBackend::OffscreenTexturePool::acquire(
    GPU::Device& device, uint32_t width, uint32_t height,
    GPU::PixelFormat format, GPU::TextureUsage usage, uint64_t current_frame)
{
    const SizeKey key{width, height};
    last_used_frame_[key] = current_frame;

    auto&   textures = generations_[current_generation_][key];
    size_t& idx       = next_index_[current_generation_][key];

    if (idx >= textures.size())
        textures.push_back(device.createTexture(
            GPU::TextureType::tt2d, format, width, height, 1, 1, 1, usage));

    return textures[idx++];
}

void MetalDrawBackend::OffscreenTexturePool::beginFrame() noexcept
{
    current_generation_ = (current_generation_ + 1) % kGenerations;
    next_index_[current_generation_].clear();
}

void MetalDrawBackend::OffscreenTexturePool::evictStale(uint64_t current_frame)
{
    for (auto it = last_used_frame_.begin(); it != last_used_frame_.end(); )
    {
        if (current_frame - it->second > kMaxAgeFrames)
        {
            const SizeKey key = it->first;
            for (auto& gen : generations_)       gen.erase(key);
            for (auto& idx : next_index_)         idx.erase(key);
            it = last_used_frame_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

std::shared_ptr<GPU::RenderPassEncoder> MetalDrawBackend::beginOffscreenPass(
    std::shared_ptr<GPU::Texture> tex,
    GPU::CommandEncoder&          encoder,
    bool                          preserve_content)
{
    // New encoder == fresh scissor state.
    last_scissor_x_ = last_scissor_y_ = last_scissor_w_ = last_scissor_h_ = -1.0f;
    if (!tex) return nullptr;

    // arrayLayerCount = 1 (non-array 2D texture) — see runBlurPass() below.
    auto view = tex->createView(pixel_format_, 1);
    if (!view) return nullptr;

    GPU::ColorAttachment ca{};
    ca.view             = view;
    ca.loadOp           = preserve_content ? GPU::LoadOp::load : GPU::LoadOp::clear;
    ca.storeOp          = GPU::StoreOp::store;
    ca.clearValue[0]    = 0.0f;
    ca.clearValue[1]    = 0.0f;
    ca.clearValue[2]    = 0.0f;
    ca.clearValue[3]    = 0.0f;

    GPU::BeginRenderPassDescriptor desc{};
    desc.colorAttachments = {ca};

    return encoder.beginRenderPass(desc);
}

void MetalDrawBackend::runBlurPass(
    std::shared_ptr<GPU::Texture> src,
    std::shared_ptr<GPU::Texture> dst,
    float sigma,
    bool  horizontal,
    GPU::CommandEncoder& encoder)
{
    if (!blur_pipeline_ || !quad_bgl_ || !quad_sampler_ || !src || !dst) return;

    const uint32_t tw = static_cast<uint32_t>(dst->getWidth());
    const uint32_t th = static_cast<uint32_t>(dst->getHeight());

    // arrayLayerCount = 1 (non-array 2D texture) — see beginOffscreenPass() above.
    auto dst_view = dst->createView(pixel_format_, 1);
    if (!dst_view) return;

    GPU::ColorAttachment ca{};
    ca.view    = dst_view;
    ca.loadOp  = GPU::LoadOp::clear;
    ca.storeOp = GPU::StoreOp::store;

    GPU::BeginRenderPassDescriptor desc{};
    desc.colorAttachments = {ca};

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

    auto ubuf = device_->createBuffer(sizeof(BlurUniforms), GPU::BufferUsage::vertex, &u);
    if (!ubuf) { rpe->end(); return; }

    GPU::BindGroupDescriptor bgDesc{};
    bgDesc.layout  = quad_bgl_;
    bgDesc.entries = {
        GPU::BindGroupEntryDescriptor{0, src},
        GPU::BindGroupEntryDescriptor{1, quad_sampler_},
    };
    auto bg = device_->createBindGroup(bgDesc);
    if (!bg) { rpe->end(); return; }

    rpe->setPipeline(blur_pipeline_);
    rpe->setBindGroup(0, bg);
    rpe->setVertexBuffer(0, ubuf);
    rpe->draw(6);
    rpe->end();
}

std::shared_ptr<GPU::Texture> MetalDrawBackend::blurTexture(
    std::shared_ptr<GPU::Texture> source,
    float sigma_x, float sigma_y,
    GPU::CommandEncoder& encoder)
{
    if (!source || !blur_pipeline_) return nullptr;

    const uint32_t tw = static_cast<uint32_t>(source->getWidth());
    const uint32_t th = static_cast<uint32_t>(source->getHeight());

    if (!blur_h_tex_ || blur_tex_w_ != tw || blur_tex_h_ != th)
    {
        blur_h_tex_ = device_->createTexture(
            GPU::TextureType::tt2d, pixel_format_, tw, th, 1, 1, 1,
            static_cast<GPU::TextureUsage>(
            static_cast<int>(GPU::TextureUsage::renderTarget) |
            static_cast<int>(GPU::TextureUsage::textureBinding)));
        blur_v_tex_ = device_->createTexture(
            GPU::TextureType::tt2d, pixel_format_, tw, th, 1, 1, 1,
            static_cast<GPU::TextureUsage>(
            static_cast<int>(GPU::TextureUsage::renderTarget) |
            static_cast<int>(GPU::TextureUsage::textureBinding)));
        blur_tex_w_ = tw;
        blur_tex_h_ = th;
    }

    // Horizontal blur: source → blur_h_tex_
    runBlurPass(source,      blur_h_tex_, sigma_x, /*horizontal=*/true,  encoder);
    // Vertical blur:   blur_h_tex_ → blur_v_tex_
    runBlurPass(blur_h_tex_, blur_v_tex_, sigma_y, /*horizontal=*/false, encoder);

    return blur_v_tex_;
}

void MetalDrawBackend::drawBackdropFilter(
    const DrawBackdropFilterBeginCmd&      cmd,
    std::shared_ptr<GPU::Texture>          blurred_source,
    const Matrix4&                         transform,
    const Rect&                            clip,
    GPU::RenderPassEncoder&                encoder)
{
    if (!blurred_source) return;
    if (!applyScissor(clip, encoder)) return;

    // Transform all four corners independently — not collapsed to an
    // axis-aligned bounding box — so a rotated/perspective-projected
    // BackdropFilter renders as a genuinely tilted shape. Unlike
    // drawImage() (which samples a plain rectangular image texture), each
    // corner here must get its *own* UV: blurred_source is a full-viewport
    // screen-space capture, so "what's behind" a rotated corner is
    // whatever that corner's own on-screen position lands on, not a
    // bilinear interpolation between two shared corner UVs (that
    // shared-rect assumption is exactly the axis-aligned limitation being
    // fixed here — a rotated widget's four corners sample four genuinely
    // different, non-rectangularly-related regions of the backdrop).
    auto c00 = transform * vm::Vector4<float>(cmd.bounds.left(),  cmd.bounds.top(),    0.0f, 1.0f);
    auto c10 = transform * vm::Vector4<float>(cmd.bounds.right(), cmd.bounds.top(),    0.0f, 1.0f);
    auto c01 = transform * vm::Vector4<float>(cmd.bounds.left(),  cmd.bounds.bottom(), 0.0f, 1.0f);
    auto c11 = transform * vm::Vector4<float>(cmd.bounds.right(), cmd.bounds.bottom(), 0.0f, 1.0f);

    const float src_w = static_cast<float>(blurred_source->getWidth());
    const float src_h = static_cast<float>(blurred_source->getHeight());

    // Divide by each corner's own w *before* normalizing by texture size:
    // the UV assigned to a vertex must be the true value for that vertex's
    // final (post-perspective-divide) screen position — the rasterizer's
    // perspective-correct interpolation reconstructs the correct value for
    // fragments *between* vertices, but only if the corner values
    // themselves are already correct. A no-op when w==1 (every existing,
    // non-perspective use of BackdropFilter), matching prior behavior.
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
        encoder);
}

std::shared_ptr<GPU::Texture> MetalDrawBackend::buildGradientLUT(
    const std::vector<Color>& colors,
    const std::vector<float>& stops)
{
    if (colors.empty()) return nullptr;

    constexpr int kLutSize = 256;
    std::vector<uint8_t> data(kLutSize * 4);

    for (int i = 0; i < kLutSize; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(kLutSize - 1);

        // Find the two stops that bracket t.
        Color c;
        if (colors.size() == 1 || stops.empty())
        {
            c = colors[0];
        }
        else
        {
            int lo = 0;
            int hi = static_cast<int>(colors.size()) - 1;
            for (int s = 0; s < static_cast<int>(stops.size()) - 1; ++s)
            {
                if (t >= stops[s] && t <= stops[s + 1])
                {
                    lo = s;
                    hi = s + 1;
                    break;
                }
            }
            const float range = stops[hi] - stops[lo];
            const float f     = (range > 0.0001f) ? (t - stops[lo]) / range : 0.0f;
            const Color& ca   = colors[lo];
            const Color& cb   = colors[hi];
            c = Color::fromRGBA(
                ca.r + f * (cb.r - ca.r),
                ca.g + f * (cb.g - ca.g),
                ca.b + f * (cb.b - ca.b),
                ca.a + f * (cb.a - ca.a));
        }

        // BGRA layout (matching Metal bgra8unorm).
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

void MetalDrawBackend::drawShaderMaskComposite(
    std::shared_ptr<GPU::Texture>   child_tex,
    const DrawShaderMaskBeginCmd&   cmd,
    const Matrix4&                  transform,
    const Rect&                     clip,
    GPU::RenderPassEncoder&         encoder)
{
    if (!shader_mask_pipeline_ || !shader_mask_bgl_ || !quad_sampler_ || !child_tex)
        return;
    if (!applyScissor(clip, encoder)) return;

    // Transform all four corners independently — real per-vertex quad, not
    // an axis-aligned bounding box — so a rotated or mirrored ShaderMask's
    // *destination shape and sampled child content* render correctly. `tl`/
    // `br` are kept for the gradient's own parameters below, which are
    // deliberately left evaluated in screen space (via the fragment
    // shader's `in.pos.xy`), unchanged from before this pass — see the
    // comment on ShaderMaskVertOut in widgets.metal for why that's a
    // separate, pre-existing scope boundary, not an oversight.
    auto tl = transform * vm::Vector4<float>(cmd.bounds.left(),  cmd.bounds.top(),    0.0f, 1.0f);
    auto br = transform * vm::Vector4<float>(cmd.bounds.right(), cmd.bounds.bottom(), 0.0f, 1.0f);
    auto c00 = tl;
    auto c10 = transform * vm::Vector4<float>(cmd.bounds.right(), cmd.bounds.top(),    0.0f, 1.0f);
    auto c01 = transform * vm::Vector4<float>(cmd.bounds.left(),  cmd.bounds.bottom(), 0.0f, 1.0f);
    auto c11 = br;

    // Build gradient LUT from shader variant.
    std::shared_ptr<GPU::Texture> lut_tex;
    float gradient_type = 0.0f;
    float p1[2] = {0.0f, 0.0f};
    float p2[2] = {br.x() - tl.x(), 0.0f};

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
            lut_tex = buildGradientLUT(s.colors, s.stops);
        }
    }, cmd.shader);

    if (!lut_tex) return;

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

    ShaderMaskUniforms u{};
    u.viewport[0]     = vp_w_;
    u.viewport[1]     = vp_h_;
    u.gradient_type   = gradient_type;
    u.gradient_p1[0]  = p1[0];
    u.gradient_p1[1]  = p1[1];
    u.gradient_p1[2]  = 0.0f;
    u.gradient_p1[3]  = 0.0f;
    u.gradient_p2[0]  = p2[0];
    u.gradient_p2[1]  = p2[1];
    u.gradient_p2[2]  = 0.0f;
    u.gradient_p2[3]  = 0.0f;
    u.blend_mode      = (cmd.blend_mode == BlendMode::modulate) ? 1.0f : 0.0f;

    auto ubuf = shader_mask_uniform_pool_.acquire(*device_, sizeof(ShaderMaskUniforms), &u);
    if (!ubuf) return;

    GPU::BindGroupDescriptor bgDesc{};
    bgDesc.layout  = shader_mask_bgl_;
    bgDesc.entries = {
        GPU::BindGroupEntryDescriptor{0, child_tex},
        GPU::BindGroupEntryDescriptor{1, lut_tex},
        GPU::BindGroupEntryDescriptor{2, quad_sampler_},
    };
    auto bg = device_->createBindGroup(bgDesc);
    if (!bg) return;

    encoder.setPipeline(shader_mask_pipeline_);
    encoder.setBindGroup(0, bg);
    encoder.setVertexBuffer(0, vbuf);
    encoder.setVertexBuffer(1, ubuf);
    encoder.draw(6);
}

void MetalDrawBackend::drawClipShapeComposite(
    std::shared_ptr<GPU::Texture> child_tex,
    const Rect&                   bounds,
    float                         corner_radius,
    bool                          is_oval,
    const Matrix4&                transform,
    const Rect&                   clip,
    GPU::RenderPassEncoder&       encoder)
{
    if (!clip_shape_pipeline_ || !quad_bgl_ || !quad_sampler_ || !child_tex)
        return;
    if (!applyScissor(clip, encoder)) return;

    // Transform all four corners independently — real per-vertex quad, not
    // an axis-aligned bounding box (which is what silently made the clip
    // shape's content vanish under a mirrored/negative-scale ambient
    // transform — see TODO.md's "Bug: Transform content vanishes..."
    // entry for the full root-cause trace — and what would have made
    // genuine rotation/perspective render as a resized box instead of a
    // tilted shape). See ProjectedCorner's doc comment.
    auto c00 = transform * vm::Vector4<float>(bounds.left(),  bounds.top(),    0.0f, 1.0f);
    auto c10 = transform * vm::Vector4<float>(bounds.right(), bounds.top(),    0.0f, 1.0f);
    auto c01 = transform * vm::Vector4<float>(bounds.left(),  bounds.bottom(), 0.0f, 1.0f);
    auto c11 = transform * vm::Vector4<float>(bounds.right(), bounds.bottom(), 0.0f, 1.0f);

    // The rounded-rect/ellipse SDF is evaluated in the shape's own local
    // (pre-transform) space, via UV — perspective-correctly interpolated
    // by the hardware regardless of how the destination quad is projected
    // — so it needs bounds' plain logical size and corner radius, not a
    // transform-scaled physical-pixel equivalent. This also means no
    // per-axis "flip" workaround is needed anymore (unlike the fix
    // shipped earlier today for the AABB-based version of this function):
    // real corners naturally handle mirroring; nothing to work around.
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

    auto ubuf = clip_shape_uniform_pool_.acquire(*device_, sizeof(ClipShapeUniforms), &u);
    if (!ubuf) return;

    GPU::BindGroupDescriptor bgDesc{};
    bgDesc.layout  = quad_bgl_;
    bgDesc.entries = {
        GPU::BindGroupEntryDescriptor{0, child_tex},
        GPU::BindGroupEntryDescriptor{1, quad_sampler_},
    };
    auto bg = device_->createBindGroup(bgDesc);
    if (!bg) return;

    encoder.setPipeline(clip_shape_pipeline_);
    encoder.setBindGroup(0, bg);
    encoder.setVertexBuffer(0, vbuf);
    encoder.setVertexBuffer(1, ubuf);
    encoder.draw(6);
}
