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

namespace GPU = systems::leal::campello_gpu;

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

// ---------------------------------------------------------------------------
// Construction — compile pipelines
// ---------------------------------------------------------------------------

MetalDrawBackend::MetalDrawBackend(
    std::shared_ptr<GPU::Device> device,
    Color                        bg_color,
    GPU::PixelFormat             pixel_format)
    : device_(std::move(device))
    , bg_color_(bg_color)
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
    const Matrix4&        /*transform*/,
    const Rect&           /*clip*/,
    GPU::RenderPassEncoder& encoder)
{
    if (!rect_pipeline_) return;

    if (cmd.paint.style == PaintStyle::fill)
    {
        drawFilledRect(cmd.rect.x, cmd.rect.y,
                       cmd.rect.width, cmd.rect.height,
                       cmd.paint.color, encoder);
    }
    else
    {
        // Stroke: draw four thin filled rects along each edge.
        const float sw = cmd.paint.stroke_width;
        const float x  = cmd.rect.x;
        const float y  = cmd.rect.y;
        const float w  = cmd.rect.width;
        const float h  = cmd.rect.height;

        // Top
        drawFilledRect(x, y, w, sw, cmd.paint.color, encoder);
        // Bottom
        drawFilledRect(x, y + h - sw, w, sw, cmd.paint.color, encoder);
        // Left
        drawFilledRect(x, y + sw, sw, h - 2.0f * sw, cmd.paint.color, encoder);
        // Right
        drawFilledRect(x + w - sw, y + sw, sw, h - 2.0f * sw, cmd.paint.color, encoder);
    }
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
    const Matrix4&        /*transform*/,
    const Rect&           /*clip*/,
    GPU::RenderPassEncoder& encoder)
{
    if (!quad_pipeline_ || !quad_bgl_ || !quad_sampler_) return;
    if (cmd.span.text.empty()) return;

    @autoreleasepool {
        NSString *nsText = [NSString stringWithUTF8String:cmd.span.text.c_str()];
        if (!nsText || nsText.length == 0) return;

        const float fontSize = cmd.span.style.font_size > 0.0f
                               ? cmd.span.style.font_size : 14.0f;

        // Build font
        NSString *family = cmd.span.style.font_family.empty()
                           ? @"Helvetica Neue"
                           : [NSString stringWithUTF8String:cmd.span.style.font_family.c_str()];

        CTFontRef ctFont = CTFontCreateWithName(
            (__bridge CFStringRef)family, (CGFloat)fontSize, nullptr);
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

        // Texture dimensions (add small padding for anti-aliasing)
        uint32_t texW = (uint32_t)ceil(fitSize.width)  + 2;
        uint32_t texH = (uint32_t)ceil(fitSize.height) + 2;

        // Compute baseline offset: CoreText uses Quartz coords (y+ up)
        CTFontRef measureFont = CTFontCreateWithName(
            (__bridge CFStringRef)family, (CGFloat)fontSize, nullptr);
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

        // CGBitmapContext stores rows top-to-bottom in memory (its y+ up
        // coordinate system maps CG y=height-1 → row 0), matching Metal's
        // UV convention (0,0) = top-left.  No V-flip needed.
        drawTexturedQuad(
            texture,
            cmd.origin.x - 1.0f, cmd.origin.y - 1.0f,
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
    const Matrix4&        /*transform*/,
    const Rect&           /*clip*/,
    GPU::RenderPassEncoder& encoder)
{
    if (!quad_pipeline_ || !quad_bgl_ || !quad_sampler_) return;
    if (!cmd.texture) return;

    drawTexturedQuad(
        cmd.texture,
        cmd.dst_rect.x, cmd.dst_rect.y,
        cmd.dst_rect.width, cmd.dst_rect.height,
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
