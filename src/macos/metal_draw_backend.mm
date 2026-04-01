#import "metal_draw_backend.hpp"

#import <campello_gpu/device.hpp>
#import <campello_gpu/render_pass_encoder.hpp>
#import <campello_gpu/texture.hpp>
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

#import <CoreText/CoreText.h>
#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

#import "shaders/metal_widgets.h"

#include <vector>
#include <cmath>
#include <cstring>
#include <vector_math/vector4.hpp>

namespace GPU = systems::leal::campello_gpu;
namespace vm  = systems::leal::vector_math;

using namespace systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// Uniform buffer layouts (must match the Metal structs in widgets.metal)
// ---------------------------------------------------------------------------

struct alignas(16) RectUniforms {
    float rect[4];      // x, y, width, height
    float color[4];     // r, g, b, a
    float viewport[2];  // width, height
    float _pad[2];
};

struct alignas(16) QuadUniforms {
    float dstRect[4];   // x, y, width, height (pixels)
    float srcRect[4];   // u0, v0, u1, v1 (normalised UV)
    float viewport[2];  // width, height (pixels)
    float opacity;      // [0, 1] — scales all pixel channels
    float _pad;
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
        // No vertex buffers — procedural geometry from vertex_id

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
}

// ---------------------------------------------------------------------------
// drawRect
// ---------------------------------------------------------------------------

void MetalDrawBackend::drawFilledRect(
    float x, float y, float w, float h,
    const Color& color,
    GPU::RenderPassEncoder& encoder)
{
    RectUniforms u{};
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

    auto ubuf = device_->createBuffer(
        sizeof(RectUniforms), GPU::BufferUsage::vertex, &u);
    if (!ubuf) return;

    encoder.setPipeline(rect_pipeline_);
    encoder.setVertexBuffer(0, ubuf);
    encoder.draw(6);
}

void MetalDrawBackend::drawRect(
    const DrawRectCmd&    cmd,
    const Matrix4&        transform,
    const Rect&           /*clip*/,
    GPU::RenderPassEncoder& encoder)
{
    if (!rect_pipeline_) return;

    // Transform the four corners and use their axis-aligned bounding box.
    // This is exact for translate and scale transforms. For rotation the AABB
    // will be larger than the actual rotated quad, but rotation of plain rects
    // is intentionally avoided in those tests (use circles/lines instead).
    auto c00 = transform * vm::Vector4<float>(cmd.rect.left(),  cmd.rect.top(),    0.0f, 1.0f);
    auto c10 = transform * vm::Vector4<float>(cmd.rect.right(), cmd.rect.top(),    0.0f, 1.0f);
    auto c01 = transform * vm::Vector4<float>(cmd.rect.left(),  cmd.rect.bottom(), 0.0f, 1.0f);
    auto c11 = transform * vm::Vector4<float>(cmd.rect.right(), cmd.rect.bottom(), 0.0f, 1.0f);

    const float min_x = std::min({c00.x(), c10.x(), c01.x(), c11.x()});
    const float min_y = std::min({c00.y(), c10.y(), c01.y(), c11.y()});
    const float max_x = std::max({c00.x(), c10.x(), c01.x(), c11.x()});
    const float max_y = std::max({c00.y(), c10.y(), c01.y(), c11.y()});

    if (cmd.paint.style == PaintStyle::fill)
    {
        drawFilledRect(min_x, min_y, max_x - min_x, max_y - min_y,
                       cmd.paint.color, encoder);
    }
    else
    {
        // Stroke: four thin filled rects along each edge of the transformed AABB.
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

    auto ubuf = device_->createBuffer(sizeof(ShapeUniforms), GPU::BufferUsage::vertex, &u);
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
    const Rect&             /*clip*/,
    GPU::RenderPassEncoder& encoder)
{
    if (!shape_pipeline_) return;

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
    const Rect&             /*clip*/,
    GPU::RenderPassEncoder& encoder)
{
    if (!shape_pipeline_) return;

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
    const Rect&             /*clip*/,
    GPU::RenderPassEncoder& encoder)
{
    if (!shape_pipeline_) return;

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
    const Rect&             /*clip*/,
    GPU::RenderPassEncoder& encoder)
{
    if (!line_pipeline_) return;

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

    auto ubuf = device_->createBuffer(sizeof(LineUniforms), GPU::BufferUsage::vertex, &u);
    if (!ubuf) return;

    encoder.setPipeline(line_pipeline_);
    encoder.setVertexBuffer(0, ubuf);
    encoder.draw(6);
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

        CTFontRef ctFont = CTFontCreateWithName(
            (__bridge CFStringRef)family, (CGFloat)fontSize, nullptr);
        if (!ctFont)
            return Size::zero();

        NSDictionary *attrs = @{ (__bridge NSString*)kCTFontAttributeName: (__bridge id)ctFont };
        CFRelease(ctFont);

        NSAttributedString *attrStr =
            [[NSAttributedString alloc] initWithString:nsText attributes:attrs];

        CTFramesetterRef framesetter =
            CTFramesetterCreateWithAttributedString(
                (__bridge CFAttributedStringRef)attrStr);
        CGSize fitSize = CTFramesetterSuggestFrameSizeWithConstraints(
            framesetter, CFRangeMake(0, 0), nullptr,
            CGSizeMake(CGFLOAT_MAX, CGFLOAT_MAX), nullptr);
        CFRelease(framesetter);

        return Size{ (float)fitSize.width, (float)fitSize.height };
    }
}

// ---------------------------------------------------------------------------
// drawText — rasterise with CoreText into a BGRA8 texture, then draw quad
// ---------------------------------------------------------------------------

void MetalDrawBackend::drawText(
    const DrawTextCmd&    cmd,
    const Matrix4&        transform,
    const Rect&           /*clip*/,
    GPU::RenderPassEncoder& encoder)
{
    if (!quad_pipeline_ || !quad_bgl_ || !quad_sampler_) return;
    if (cmd.span.text.empty()) return;

    // Transform the logical origin to physical pixels.
    auto t_origin = transform * vm::Vector4<float>(cmd.origin.x, cmd.origin.y, 0.0f, 1.0f);

    @autoreleasepool {
        NSString *nsText = [NSString stringWithUTF8String:cmd.span.text.c_str()];
        if (!nsText || nsText.length == 0) return;

        // The font_size stored in the span has already been scaled to physical
        // pixels by RenderText/RenderParagraph (they multiply by
        // activeDevicePixelRatio() before emitting the DrawTextCmd).
        // We use it directly so the CoreText bitmap is in physical pixels.
        const float physicalFontSize = cmd.span.style.font_size > 0.0f
                                       ? cmd.span.style.font_size : 14.0f;

        // Build font
        NSString *family = cmd.span.style.font_family.empty()
                           ? @"Helvetica Neue"
                           : [NSString stringWithUTF8String:cmd.span.style.font_family.c_str()];

        CTFontRef ctFont = CTFontCreateWithName(
            (__bridge CFStringRef)family, (CGFloat)physicalFontSize, nullptr);
        if (!ctFont) return;

        // Text color
        const Color& tc = cmd.span.style.color;
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

        // Measure
        CTFramesetterRef framesetter =
            CTFramesetterCreateWithAttributedString(
                (__bridge CFAttributedStringRef)attrStr);
        CGSize fitSize = CTFramesetterSuggestFrameSizeWithConstraints(
            framesetter, CFRangeMake(0, 0), nullptr,
            CGSizeMake(CGFLOAT_MAX, CGFLOAT_MAX), nullptr);
        CFRelease(framesetter);

        if (fitSize.width <= 0.0 || fitSize.height <= 0.0) return;

        // Texture dimensions in physical pixels (add small padding for anti-aliasing)
        uint32_t texW = (uint32_t)ceil(fitSize.width)  + 2;
        uint32_t texH = (uint32_t)ceil(fitSize.height) + 2;

        // Compute baseline offset: CoreText uses Quartz coords (y+ up)
        CTFontRef measureFont = CTFontCreateWithName(
            (__bridge CFStringRef)family, (CGFloat)physicalFontSize, nullptr);
        CGFloat descent = fabs(CTFontGetDescent(measureFont));
        CFRelease(measureFont);

        // Allocate BGRA8 pixel buffer
        std::vector<uint8_t> pixels(texW * texH * 4, 0);

        // Create CGBitmapContext (BGRA8 premultiplied — matches bgra8unorm)
        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        CGContextRef cgCtx = CGBitmapContextCreate(
            pixels.data(), texW, texH, 8, texW * 4, colorSpace,
            (CGBitmapInfo)((uint32_t)kCGBitmapByteOrder32Little | (uint32_t)kCGImageAlphaPremultipliedFirst));
        CGColorSpaceRelease(colorSpace);
        if (!cgCtx) return;

        // Draw text — baseline at y = descent + 1 (1px bottom padding)
        CGContextSetTextMatrix(cgCtx, CGAffineTransformIdentity);
        CTLineRef line = CTLineCreateWithAttributedString(
            (__bridge CFAttributedStringRef)attrStr);
        CGContextSetTextPosition(cgCtx, 1.0, descent + 1.0);
        CTLineDraw(line, cgCtx);
        CFRelease(line);
        CGContextRelease(cgCtx);

        // Upload to GPU texture (bgra8unorm)
        auto texture = device_->createTexture(
            GPU::TextureType::tt2d,
            GPU::PixelFormat::bgra8unorm,
            texW, texH, 1, 1, 1,
            GPU::TextureUsage::textureBinding);
        if (!texture) return;
        texture->upload(0, (uint64_t)pixels.size(), pixels.data());

        // Place the quad at the physical-pixel origin.  The texture is already
        // in physical pixels, so its pixel dimensions are the correct quad size.
        // Subtract the 1-physical-pixel padding used above on each side.
        drawTexturedQuad(
            texture,
            t_origin.x() - 1.0f, t_origin.y() - 1.0f,
            (float)texW, (float)texH,
            0.0f, 0.0f, 1.0f, 1.0f,
            1.0f,  // text colour alpha is already baked into the glyph texture
            encoder);
    }
}

// ---------------------------------------------------------------------------
// drawImage
// ---------------------------------------------------------------------------

void MetalDrawBackend::drawImage(
    const DrawImageCmd&   cmd,
    const Matrix4&        transform,
    const Rect&           /*clip*/,
    GPU::RenderPassEncoder& encoder)
{
    if (!quad_pipeline_ || !quad_bgl_ || !quad_sampler_) return;
    if (!cmd.texture) return;

    // Apply the current transform (which includes the DPR scale) to the
    // destination rect so the quad lands in physical pixels.
    auto tl = transform * vm::Vector4<float>(cmd.dst_rect.left(),  cmd.dst_rect.top(),    0.0f, 1.0f);
    auto br = transform * vm::Vector4<float>(cmd.dst_rect.right(), cmd.dst_rect.bottom(), 0.0f, 1.0f);

    drawTexturedQuad(
        cmd.texture,
        tl.x(), tl.y(), br.x() - tl.x(), br.y() - tl.y(),
        cmd.src_rect.x, cmd.src_rect.y,
        cmd.src_rect.right(), cmd.src_rect.bottom(),
        cmd.opacity,
        encoder);
}

// ---------------------------------------------------------------------------
// drawTexturedQuad — shared helper
// ---------------------------------------------------------------------------

void MetalDrawBackend::drawTexturedQuad(
    std::shared_ptr<GPU::Texture>  texture,
    float dst_x, float dst_y, float dst_w, float dst_h,
    float src_u0, float src_v0, float src_u1, float src_v1,
    float opacity,
    GPU::RenderPassEncoder&        encoder)
{
    // Build bind group for this texture
    GPU::BindGroupDescriptor bgDesc{};
    bgDesc.layout  = quad_bgl_;
    bgDesc.entries = {
        GPU::BindGroupEntryDescriptor{ 0, texture },
        GPU::BindGroupEntryDescriptor{ 1, quad_sampler_ },
    };
    auto bindGroup = device_->createBindGroup(bgDesc);
    if (!bindGroup) return;

    // Fill uniform buffer
    QuadUniforms u{};
    u.dstRect[0]  = dst_x;
    u.dstRect[1]  = dst_y;
    u.dstRect[2]  = dst_w;
    u.dstRect[3]  = dst_h;
    u.srcRect[0]  = src_u0;
    u.srcRect[1]  = src_v0;
    u.srcRect[2]  = src_u1;
    u.srcRect[3]  = src_v1;
    u.viewport[0] = vp_w_;
    u.viewport[1] = vp_h_;
    u.opacity     = opacity;

    auto ubuf = device_->createBuffer(
        sizeof(QuadUniforms), GPU::BufferUsage::vertex, &u);
    if (!ubuf) return;

    encoder.setPipeline(quad_pipeline_);
    encoder.setBindGroup(0, bindGroup);
    encoder.setVertexBuffer(0, ubuf);
    encoder.draw(6);
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
    float dstRect[4];        // bounds in viewport pixels
    float viewport[2];
    float gradient_type;     // 0 = linear, 1 = radial
    float _pad0;
    float gradient_p1[4];    // linear: begin.xy; radial: center.xy
    float gradient_p2[4];    // linear: end.xy;   radial: radius in [0]
    float blend_mode;        // 0 = srcIn, 1 = modulate
    float _pad1[3];
};

std::shared_ptr<GPU::Texture> MetalDrawBackend::createOffscreenTexture(
    uint32_t width, uint32_t height)
{
    return device_->createTexture(
        GPU::TextureType::tt2d, pixel_format_,
        width, height, 1, 1, 1,
        static_cast<GPU::TextureUsage>(
            static_cast<int>(GPU::TextureUsage::renderTarget) |
            static_cast<int>(GPU::TextureUsage::textureBinding) |
            static_cast<int>(GPU::TextureUsage::copySrc)));
}

std::shared_ptr<GPU::RenderPassEncoder> MetalDrawBackend::beginOffscreenPass(
    std::shared_ptr<GPU::Texture> tex,
    GPU::CommandEncoder&          encoder)
{
    if (!tex) return nullptr;

    auto view = tex->createView(pixel_format_);
    if (!view) return nullptr;

    GPU::ColorAttachment ca{};
    ca.view             = view;
    ca.loadOp           = GPU::LoadOp::clear;
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

    auto dst_view = dst->createView(pixel_format_);
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
    const Rect&                            /*clip*/,
    GPU::RenderPassEncoder&                encoder)
{
    if (!blurred_source) return;

    // Transform the logical bounds to physical pixels.
    auto tl = transform * vm::Vector4<float>(cmd.bounds.left(),  cmd.bounds.top(),    0.0f, 1.0f);
    auto br = transform * vm::Vector4<float>(cmd.bounds.right(), cmd.bounds.bottom(), 0.0f, 1.0f);

    // UV maps the physical-pixel bounds region of the blurred texture.
    const float src_w = static_cast<float>(blurred_source->getWidth());
    const float src_h = static_cast<float>(blurred_source->getHeight());

    const float u0 = tl.x() / src_w;
    const float v0 = tl.y() / src_h;
    const float u1 = br.x() / src_w;
    const float v1 = br.y() / src_h;

    drawTexturedQuad(
        blurred_source,
        tl.x(), tl.y(), br.x() - tl.x(), br.y() - tl.y(),
        u0, v0, u1, v1,
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
    GPU::RenderPassEncoder&         encoder)
{
    if (!shader_mask_pipeline_ || !shader_mask_bgl_ || !quad_sampler_ || !child_tex)
        return;

    // Transform the logical bounds to physical pixels.
    auto tl = transform * vm::Vector4<float>(cmd.bounds.left(),  cmd.bounds.top(),    0.0f, 1.0f);
    auto br = transform * vm::Vector4<float>(cmd.bounds.right(), cmd.bounds.bottom(), 0.0f, 1.0f);

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

    ShaderMaskUniforms u{};
    u.dstRect[0]      = tl.x();
    u.dstRect[1]      = tl.y();
    u.dstRect[2]      = br.x() - tl.x();
    u.dstRect[3]      = br.y() - tl.y();
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

    auto ubuf = device_->createBuffer(sizeof(ShaderMaskUniforms), GPU::BufferUsage::vertex, &u);
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
    encoder.setVertexBuffer(0, ubuf);
    encoder.draw(6);
}
