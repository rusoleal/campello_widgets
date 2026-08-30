#import "metal_draw_backend.hpp"
#include "gpu/path_tessellation.hpp"
#include "gpu/stroke_geometry.hpp"
#include "gpu/path_fill_aa.hpp"

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

// MSAA sample count for Path::fillType()'s stencil-then-cover fill (see
// renderPathFillWinding()) -- 4 is WebGPU's other allowed value besides 1
// (RenderPipelineDescriptor::sampleCount's doc comment), used here for both
// the pipelines and the color+stencil textures/resolve target.
static constexpr uint32_t kPathFillMsaaSamples = 4;

// ---------------------------------------------------------------------------
// Font resolution
// ---------------------------------------------------------------------------

namespace
{
    // CTFontCreateWithName always returns the family's regular face — bold
    // and italic must be requested explicitly via symbolic traits, or every
    // TextStyle renders identically regardless of font_weight/italic.
    //
    // `family == nil` means "no explicit font_family was requested" and
    // resolves to the OS UI system font (San Francisco on macOS/iOS) via
    // CTFontCreateUIFontForLanguage — matching what UIFont.systemFont(...)
    // resolves to on the iOS side, rather than hardcoding a fixed family
    // name (which wouldn't track San Francisco across OS releases and
    // doesn't match the reference screenshots' actual glyph shapes/metrics).
    CTFontRef CreateStyledCTFont(NSString* family, CGFloat size, FontWeight weight, bool italic)
    {
        CTFontRef base = family
            ? CTFontCreateWithName((__bridge CFStringRef)family, size, nullptr)
            : CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, size, nullptr);
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

// drawVertices()'s pipeline — no shared color (each vertex carries its own),
// so this is just the viewport NDC-conversion constant RectUniforms also
// carries.
struct alignas(16) VerticesUniforms {
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

// Mirrors widgets.metal's IconUniforms field-for-field.
struct alignas(16) IconUniforms {
    float viewport[2];  // width, height (pixels)
    float opacity;      // [0, 1] — scales the final alpha
    float _pad;
    float tint[4];      // straight-alpha RGBA recolor
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

    // --- Rect-AA pipeline (premultiplied-alpha blend) — same shape as the
    //     rect pipeline above, plus a per-vertex alpha; see widgets.metal's
    //     rect-AA pipeline doc comment and drawFillAA() below. A separate
    //     pipeline (not an addition to rect_pipeline_ itself) so every
    //     existing rect_pipeline_ call site stays untouched. ---
    {
        GPU::ColorState rectAACs{};
        rectAACs.format    = pixel_format;
        rectAACs.writeMask = GPU::ColorWrite::all;
        rectAACs.blend = GPU::BlendState{
            .color = { GPU::BlendFactor::one, GPU::BlendFactor::oneMinusSrcAlpha, GPU::BlendOperation::add },
            .alpha = { GPU::BlendFactor::one, GPU::BlendFactor::oneMinusSrcAlpha, GPU::BlendOperation::add },
        };

        GPU::RenderPipelineDescriptor desc{};
        desc.vertex.module     = shader;
        desc.vertex.entryPoint = "rectAAVertex";

        // Real per-vertex (x, y, w, alpha) — see RectAAVertex's doc comment
        // in metal_draw_backend.hpp and RectAAVertexIn's in widgets.metal.
        GPU::VertexAttribute rectAAPosAttr{};
        rectAAPosAttr.componentType  = GPU::ComponentType::ctFloat;
        rectAAPosAttr.accessorType   = GPU::AccessorType::acVec4;
        rectAAPosAttr.offset         = 0;
        rectAAPosAttr.shaderLocation = 0;

        GPU::VertexLayout rectAALayout{};
        rectAALayout.arrayStride = sizeof(RectAAVertex);
        rectAALayout.attributes  = {rectAAPosAttr};
        rectAALayout.stepMode    = GPU::StepMode::vertex;

        desc.vertex.buffers = {rectAALayout};

        GPU::FragmentDescriptor frag{};
        frag.module     = shader;
        frag.entryPoint = "rectAAFragment";
        frag.targets.push_back(rectAACs);
        desc.fragment = frag;

        desc.topology  = GPU::PrimitiveTopology::triangleList;
        desc.cullMode  = GPU::CullMode::none;
        desc.frontFace = GPU::FrontFace::ccw;

        rect_aa_pipeline_ = device_->createRenderPipeline(desc);
    }

    // --- Vertices pipeline (drawVertices()) — premultiplied-alpha blend,
    //     per-vertex color instead of rect_aa's shared-uniform color +
    //     per-vertex alpha. See VerticesVertex's doc comment. ---
    {
        GPU::ColorState verticesCs{};
        verticesCs.format    = pixel_format;
        verticesCs.writeMask = GPU::ColorWrite::all;
        verticesCs.blend = GPU::BlendState{
            .color = { GPU::BlendFactor::one, GPU::BlendFactor::oneMinusSrcAlpha, GPU::BlendOperation::add },
            .alpha = { GPU::BlendFactor::one, GPU::BlendFactor::oneMinusSrcAlpha, GPU::BlendOperation::add },
        };

        GPU::RenderPipelineDescriptor desc{};
        desc.vertex.module     = shader;
        desc.vertex.entryPoint = "verticesVertex";

        GPU::VertexAttribute posAttr{};
        posAttr.componentType  = GPU::ComponentType::ctFloat;
        posAttr.accessorType   = GPU::AccessorType::acVec3;
        posAttr.offset         = offsetof(VerticesVertex, x);
        posAttr.shaderLocation = 0;

        GPU::VertexAttribute colorAttr{};
        colorAttr.componentType  = GPU::ComponentType::ctFloat;
        colorAttr.accessorType   = GPU::AccessorType::acVec4;
        colorAttr.offset         = offsetof(VerticesVertex, r);
        colorAttr.shaderLocation = 1;

        GPU::VertexLayout verticesLayout{};
        verticesLayout.arrayStride = sizeof(VerticesVertex);
        verticesLayout.attributes  = {posAttr, colorAttr};
        verticesLayout.stepMode    = GPU::StepMode::vertex;

        desc.vertex.buffers = {verticesLayout};

        GPU::FragmentDescriptor frag{};
        frag.module     = shader;
        frag.entryPoint = "verticesFragment";
        frag.targets.push_back(verticesCs);
        desc.fragment = frag;

        desc.topology  = GPU::PrimitiveTopology::triangleList;
        desc.cullMode  = GPU::CullMode::none;
        desc.frontFace = GPU::FrontFace::ccw;

        vertices_pipeline_ = device_->createRenderPipeline(desc);
    }

    // --- Path::fillType() stencil-then-cover pipelines -- see
    //     path_fill_stencil_write_winding_pipeline_'s doc comment in
    //     metal_draw_backend.hpp. All three reuse rectVertex/rectFragment
    //     and RectVertex/RectUniforms verbatim -- only ColorState/
    //     depthStencil differ from rect_pipeline_ above. ---
    {
        GPU::VertexAttribute rectPosAttr{};
        rectPosAttr.componentType  = GPU::ComponentType::ctFloat;
        rectPosAttr.accessorType   = GPU::AccessorType::acVec3;
        rectPosAttr.offset         = 0;
        rectPosAttr.shaderLocation = 0;

        GPU::VertexLayout rectLayout{};
        rectLayout.arrayStride = sizeof(RectVertex);
        rectLayout.attributes  = {rectPosAttr};
        rectLayout.stepMode    = GPU::StepMode::vertex;

        // Stencil-write pipelines: color writes disabled entirely (this
        // pass only accumulates winding/parity into the stencil buffer),
        // stencil test always passes (every fragment inside a triangle
        // contributes), depth disabled.
        GPU::ColorState writeCs{};
        writeCs.format    = pixel_format;
        writeCs.writeMask = static_cast<GPU::ColorWrite>(0); // none
        writeCs.blend     = GPU::BlendState{};

        auto buildWritePipeline = [&](GPU::StencilOp front_pass, GPU::StencilOp back_pass)
            -> std::shared_ptr<GPU::RenderPipeline>
        {
            GPU::RenderPipelineDescriptor desc{};
            desc.vertex.module     = shader;
            desc.vertex.entryPoint = "rectVertex";
            desc.vertex.buffers    = {rectLayout};

            GPU::FragmentDescriptor frag{};
            frag.module     = shader;
            frag.entryPoint = "rectFragment";
            frag.targets.push_back(writeCs);
            desc.fragment = frag;

            desc.topology  = GPU::PrimitiveTopology::triangleList;
            desc.cullMode  = GPU::CullMode::none;
            desc.frontFace = GPU::FrontFace::ccw;

            GPU::DepthStencilDescriptor ds{};
            ds.format            = GPU::PixelFormat::depth24plus_stencil8;
            ds.depthCompare      = GPU::CompareOp::always;
            ds.depthWriteEnabled = false;
            ds.stencilReadMask   = 0xFFFFFFFF;
            ds.stencilWriteMask  = 0xFFFFFFFF;
            ds.stencilFront = GPU::StencilDescriptor{
                GPU::CompareOp::always, GPU::StencilOp::keep, GPU::StencilOp::keep, front_pass};
            ds.stencilBack = GPU::StencilDescriptor{
                GPU::CompareOp::always, GPU::StencilOp::keep, GPU::StencilOp::keep, back_pass};
            desc.depthStencil = ds;
            desc.sampleCount  = kPathFillMsaaSamples;

            return device_->createRenderPipeline(desc);
        };

        // nonZero winding: front faces (CCW, as ear-clipping produces for a
        // contour authored in its "natural" direction) increment, back
        // faces (a contour wound the opposite way) decrement -- the
        // classic stencil winding-count technique.
        path_fill_stencil_write_winding_pipeline_ =
            buildWritePipeline(GPU::StencilOp::incrementWrap, GPU::StencilOp::decrementWrap);
        // evenOdd: every crossing flips parity regardless of face.
        path_fill_stencil_write_evenodd_pipeline_ =
            buildWritePipeline(GPU::StencilOp::invert, GPU::StencilOp::invert);

        // Cover pipeline: normal premultiplied-alpha color output, gated by
        // "stencil != reference(0)" -- true for both a nonzero winding
        // count *and* evenOdd's toggled-to-nonzero-byte state (invert
        // starts at 0x00, flips to 0xFF on the first crossing) -- so one
        // pipeline serves both fill types (see renderPathFillWinding()'s
        // doc comment). passOp=zero resets the stencil back to 0 in the
        // same draw that reads it, so no separate clear pass is needed.
        GPU::ColorState coverCs{};
        coverCs.format    = pixel_format;
        coverCs.writeMask = GPU::ColorWrite::all;
        coverCs.blend = GPU::BlendState{
            .color = { GPU::BlendFactor::one, GPU::BlendFactor::oneMinusSrcAlpha, GPU::BlendOperation::add },
            .alpha = { GPU::BlendFactor::one, GPU::BlendFactor::oneMinusSrcAlpha, GPU::BlendOperation::add },
        };

        GPU::RenderPipelineDescriptor coverDesc{};
        coverDesc.vertex.module     = shader;
        coverDesc.vertex.entryPoint = "rectVertex";
        coverDesc.vertex.buffers    = {rectLayout};

        GPU::FragmentDescriptor coverFrag{};
        coverFrag.module     = shader;
        coverFrag.entryPoint = "rectFragment";
        coverFrag.targets.push_back(coverCs);
        coverDesc.fragment = coverFrag;

        coverDesc.topology  = GPU::PrimitiveTopology::triangleList;
        coverDesc.cullMode  = GPU::CullMode::none;
        coverDesc.frontFace = GPU::FrontFace::ccw;

        GPU::DepthStencilDescriptor coverDs{};
        coverDs.format            = GPU::PixelFormat::depth24plus_stencil8;
        coverDs.depthCompare      = GPU::CompareOp::always;
        coverDs.depthWriteEnabled = false;
        coverDs.stencilReadMask   = 0xFFFFFFFF;
        coverDs.stencilWriteMask  = 0xFFFFFFFF;
        coverDs.stencilFront = GPU::StencilDescriptor{
            GPU::CompareOp::notEqual, GPU::StencilOp::zero, GPU::StencilOp::zero, GPU::StencilOp::zero};
        coverDs.stencilBack = coverDs.stencilFront;
        coverDesc.depthStencil = coverDs;
        coverDesc.sampleCount  = kPathFillMsaaSamples;

        path_fill_stencil_cover_pipeline_ = device_->createRenderPipeline(coverDesc);
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

    // --- Icon pipeline — tinted template images, premultiplied-alpha blend ---
    {
        GPU::RenderPipelineDescriptor desc{};
        desc.vertex.module     = shader;
        desc.vertex.entryPoint = "iconVertex";

        // Same vertex layout as the quad pipeline (QuadVertex: x,y,w,u,v)
        // — iconVertex consumes QuadVertexIn identically, just with a
        // different (larger) uniform struct at buffer(1).
        GPU::VertexAttribute posAttr{};
        posAttr.componentType  = GPU::ComponentType::ctFloat;
        posAttr.accessorType   = GPU::AccessorType::acVec3;
        posAttr.offset         = offsetof(QuadVertex, x);
        posAttr.shaderLocation = 0;

        GPU::VertexAttribute uvAttr{};
        uvAttr.componentType = GPU::ComponentType::ctFloat;
        uvAttr.accessorType  = GPU::AccessorType::acVec2;
        uvAttr.offset        = offsetof(QuadVertex, u);
        uvAttr.shaderLocation = 1;

        GPU::VertexLayout iconLayout{};
        iconLayout.arrayStride = sizeof(QuadVertex);
        iconLayout.attributes  = {posAttr, uvAttr};
        iconLayout.stepMode    = GPU::StepMode::vertex;

        desc.vertex.buffers = {iconLayout};

        GPU::ColorState iconCs{};
        iconCs.format    = pixel_format;
        iconCs.writeMask = GPU::ColorWrite::all;
        iconCs.blend = GPU::BlendState{
            .color = { GPU::BlendFactor::one, GPU::BlendFactor::oneMinusSrcAlpha, GPU::BlendOperation::add },
            .alpha = { GPU::BlendFactor::one, GPU::BlendFactor::oneMinusSrcAlpha, GPU::BlendOperation::add },
        };

        GPU::FragmentDescriptor frag{};
        frag.module     = shader;
        frag.entryPoint = "iconFragment";
        frag.targets.push_back(iconCs);
        desc.fragment = frag;

        desc.topology  = GPU::PrimitiveTopology::triangleList;
        desc.cullMode  = GPU::CullMode::none;
        desc.frontFace = GPU::FrontFace::ccw;

        icon_pipeline_ = device_->createRenderPipeline(desc);
    }

    // --- Liquid Glass pipeline — premultiplied-alpha blend ---
    {
        GPU::VertexAttribute lgPosAttr{};
        lgPosAttr.componentType  = GPU::ComponentType::ctFloat;
        lgPosAttr.accessorType   = GPU::AccessorType::acVec3;
        lgPosAttr.offset         = offsetof(LiquidGlassVertex, x);
        lgPosAttr.shaderLocation = 0;

        GPU::VertexAttribute lgUvAttr{};
        lgUvAttr.componentType  = GPU::ComponentType::ctFloat;
        lgUvAttr.accessorType   = GPU::AccessorType::acVec2;
        lgUvAttr.offset         = offsetof(LiquidGlassVertex, u);
        lgUvAttr.shaderLocation = 1;

        GPU::VertexAttribute lgLocalUvAttr{};
        lgLocalUvAttr.componentType  = GPU::ComponentType::ctFloat;
        lgLocalUvAttr.accessorType   = GPU::AccessorType::acVec2;
        lgLocalUvAttr.offset         = offsetof(LiquidGlassVertex, lu);
        lgLocalUvAttr.shaderLocation = 2;

        GPU::VertexLayout lgLayout{};
        lgLayout.arrayStride = sizeof(LiquidGlassVertex);
        lgLayout.attributes  = {lgPosAttr, lgUvAttr, lgLocalUvAttr};
        lgLayout.stepMode    = GPU::StepMode::vertex;

        GPU::RenderPipelineDescriptor desc{};
        desc.vertex.module     = shader;
        desc.vertex.entryPoint = "liquidGlassVertex";
        desc.vertex.buffers    = {lgLayout};

        GPU::ColorState lgCs{};
        lgCs.format    = pixel_format;
        lgCs.writeMask = GPU::ColorWrite::all;
        lgCs.blend = GPU::BlendState{
            .color = { GPU::BlendFactor::one, GPU::BlendFactor::oneMinusSrcAlpha, GPU::BlendOperation::add },
            .alpha = { GPU::BlendFactor::one, GPU::BlendFactor::oneMinusSrcAlpha, GPU::BlendOperation::add },
        };

        GPU::FragmentDescriptor frag{};
        frag.module     = shader;
        frag.entryPoint = "liquidGlassFragment";
        frag.targets.push_back(lgCs);
        desc.fragment = frag;

        desc.topology  = GPU::PrimitiveTopology::triangleList;
        desc.cullMode  = GPU::CullMode::none;
        desc.frontFace = GPU::FrontFace::ccw;

        liquid_glass_pipeline_ = device_->createRenderPipeline(desc);
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

    // --- Line pipeline — antialiased rotated-box SDF (lineVertex/lineFragment) ---
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
        frag.entryPoint = "lineFragment";
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

    // --- Nearest-neighbor sampler (clamp-to-edge) --- used only by
    // drawImage()/drawTintedImage() when FilterQuality::none is requested;
    // quad_sampler_ (linear) stays the shared default for every other
    // texture-sampling draw (blur, liquid glass, shader mask, clip shape).
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
    else if (buffers[idx]->getLength() < size)
        // A ring slot's buffer was allocated for an earlier, smaller
        // request — reusing it via upload() would memcpy past its actual
        // GPU allocation (SIGBUS). Pools are shared across call sites with
        // varying sizes (e.g. rect_vertex_pool_ backs both the fixed
        // 6-vertex drawFilledQuad() and the variable-length
        // drawFilledVertices() used by drawArc/drawPath), so a slot's
        // required size isn't guaranteed constant across frames even
        // within one pool instance. Replace it with a large-enough buffer
        // instead of assuming the old one still fits.
        buffers[idx] = device.createBuffer(size, GPU::BufferUsage::vertex, const_cast<void*>(data));
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

void MetalDrawBackend::drawFillAA(
    const std::vector<RectAAVertex>& verts,
    const Color& color,
    GPU::RenderPassEncoder& encoder)
{
    if (!rect_aa_pipeline_ || verts.empty()) return;

    auto vbuf = rect_vertex_pool_.acquire(*device_, verts.size() * sizeof(RectAAVertex), verts.data());
    if (!vbuf) return;

    // RectUniforms layout (color, viewport) is shared with rect_pipeline_ --
    // see widgets.metal's rect-AA pipeline doc comment.
    RectUniforms u{};
    u.color[0]    = color.r;
    u.color[1]    = color.g;
    u.color[2]    = color.b;
    u.color[3]    = color.a;
    u.viewport[0] = vp_w_;
    u.viewport[1] = vp_h_;

    auto ubuf = rect_uniform_pool_.acquire(*device_, sizeof(RectUniforms), &u);
    if (!ubuf) return;

    encoder.setPipeline(rect_aa_pipeline_);
    encoder.setVertexBuffer(0, vbuf);
    encoder.setVertexBuffer(1, ubuf);
    encoder.draw(static_cast<uint32_t>(verts.size()));
}

void MetalDrawBackend::drawVertices(
    const DrawVerticesCmd&  cmd,
    const Matrix4&          transform,
    const Rect&              clip,
    GPU::RenderPassEncoder& encoder)
{
    if (!vertices_pipeline_ || cmd.vertices.empty() || cmd.indices.empty()) return;
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

    VerticesUniforms u{};
    u.viewport[0] = vp_w_;
    u.viewport[1] = vp_h_;

    auto ubuf = vertices_uniform_pool_.acquire(*device_, sizeof(VerticesUniforms), &u);
    if (!ubuf) return;

    encoder.setPipeline(vertices_pipeline_);
    encoder.setVertexBuffer(0, vbuf);
    encoder.setVertexBuffer(1, ubuf);
    encoder.setIndexBuffer(ibuf, GPU::IndexFormat::uint32);
    encoder.drawIndexed(static_cast<uint32_t>(cmd.indices.size()));
}

std::shared_ptr<GPU::Texture> MetalDrawBackend::renderPathFillWinding(
    const std::vector<Offset>& triangles,
    Path::FillType              fill_type,
    const Color&                color,
    uint32_t                    width,
    uint32_t                    height,
    GPU::CommandEncoder&        encoder)
{
    if (!path_fill_stencil_cover_pipeline_ || triangles.empty() || width == 0 || height == 0)
        return nullptr;
    auto write_pipeline = (fill_type == Path::FillType::evenOdd)
        ? path_fill_stencil_write_evenodd_pipeline_
        : path_fill_stencil_write_winding_pipeline_;
    if (!write_pipeline) return nullptr;

    // Dedicated textures, not pooled -- see IDrawBackend::renderPathFillWinding()'s
    // doc comment: these aren't reused/replayed across frames like
    // shadow/blur/clip results, so the shared size-keyed pool (which exists
    // to amortize repeated same-size allocation across frames) isn't the
    // right fit here yet.
    //
    // color_tex/stencil_tex are multisampled (kPathFillMsaaSamples) and
    // purely transient -- MSAA requires every attachment in the pass to
    // share one sample count, but neither is ever read back directly.
    // resolve_tex is the real single-sample output: the render pass resolves
    // color_tex into it (ColorAttachment::resolveTarget below), and that's
    // what gets returned/composited.
    auto color_tex = device_->createTexture(
        GPU::TextureType::tt2d, pixel_format_, width, height, 1, 1, kPathFillMsaaSamples,
        GPU::TextureUsage::renderTarget);
    if (!color_tex) return nullptr;

    auto stencil_tex = device_->createTexture(
        GPU::TextureType::tt2d, GPU::PixelFormat::depth24plus_stencil8, width, height, 1, 1, kPathFillMsaaSamples,
        GPU::TextureUsage::renderTarget);
    if (!stencil_tex) return nullptr;

    auto resolve_tex = device_->createTexture(
        GPU::TextureType::tt2d, pixel_format_, width, height, 1, 1, 1,
        static_cast<GPU::TextureUsage>(
            static_cast<int>(GPU::TextureUsage::renderTarget) |
            static_cast<int>(GPU::TextureUsage::textureBinding) |
            static_cast<int>(GPU::TextureUsage::copySrc)));
    if (!resolve_tex) return nullptr;

    auto color_view   = color_tex->createView(pixel_format_, 1);
    auto stencil_view = stencil_tex->createView(GPU::PixelFormat::depth24plus_stencil8, 1);
    auto resolve_view = resolve_tex->createView(pixel_format_, 1);
    if (!color_view || !stencil_view || !resolve_view) return nullptr;

    GPU::ColorAttachment ca{};
    ca.view          = color_view;
    ca.resolveTarget = resolve_view;
    ca.loadOp        = GPU::LoadOp::clear;
    ca.storeOp       = GPU::StoreOp::discard; // multisampled content itself is never read back, only the resolve
    ca.clearValue[0] = ca.clearValue[1] = ca.clearValue[2] = ca.clearValue[3] = 0.0f;

    GPU::DepthStencilAttachment dsa{};
    dsa.view              = stencil_view;
    dsa.stencilLoadOp     = GPU::LoadOp::clear;
    dsa.stencilClearValue = 0;
    dsa.stencilStoreOp    = GPU::StoreOp::discard;
    dsa.stencilReadOnly   = false;
    // depth24plus_stencil8, not a pure stencil8 format -- Metal's pipeline
    // validation rejects stencil8 as a depthAttachmentPixelFormat even when
    // depth is otherwise unused (campello_gpu's DepthStencilDescriptor.format
    // covers both aspects on Metal). The depth channel itself goes
    // unused -- these fields are
    // unused by the pipelines above (depthWriteEnabled=false, compare=
    // always) but still need a well-formed value.
    dsa.depthLoadOp  = GPU::LoadOp::load;
    dsa.depthStoreOp = GPU::StoreOp::discard;
    dsa.depthReadOnly = true;

    GPU::BeginRenderPassDescriptor desc{};
    desc.colorAttachments      = {ca};
    desc.depthStencilAttachment = dsa;

    auto rpe = encoder.beginRenderPass(desc);
    if (!rpe) return nullptr;

    // Pass 1: stencil-write -- every contour's ear-clipped triangles,
    // color writes disabled. `triangles` already carries physical-pixel
    // positions local to this texture (see Renderer::applyPathFillWinding()'s
    // doc comment) -- reused as-is via RectUniforms.viewport = (width,height).
    std::vector<RectVertex> write_verts;
    write_verts.reserve(triangles.size());
    for (const auto& p : triangles) write_verts.push_back({p.x, p.y, 1.0f});

    auto write_vbuf = rect_vertex_pool_.acquire(*device_, write_verts.size() * sizeof(RectVertex), write_verts.data());
    if (write_vbuf)
    {
        RectUniforms wu{};
        wu.viewport[0] = static_cast<float>(width);
        wu.viewport[1] = static_cast<float>(height);
        auto write_ubuf = rect_uniform_pool_.acquire(*device_, sizeof(RectUniforms), &wu);
        if (write_ubuf)
        {
            rpe->setPipeline(write_pipeline);
            rpe->setVertexBuffer(0, write_vbuf);
            rpe->setVertexBuffer(1, write_ubuf);
            rpe->draw(static_cast<uint32_t>(write_verts.size()));
        }
    }

    // Pass 2: cover -- one quad spanning the whole texture, painted with
    // the real fill color, gated + reset by the stencil test (see
    // path_fill_stencil_cover_pipeline_'s doc comment).
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    const RectVertex cover_verts[6] = {
        {0.0f, 0.0f, 1.0f}, {w, 0.0f, 1.0f}, {0.0f, h, 1.0f},
        {0.0f, h, 1.0f}, {w, 0.0f, 1.0f}, {w, h, 1.0f},
    };
    auto cover_vbuf = rect_vertex_pool_.acquire(*device_, sizeof(cover_verts), cover_verts);
    if (cover_vbuf)
    {
        RectUniforms cu{};
        cu.color[0] = color.r; cu.color[1] = color.g; cu.color[2] = color.b; cu.color[3] = color.a;
        cu.viewport[0] = w; cu.viewport[1] = h;
        auto cover_ubuf = rect_uniform_pool_.acquire(*device_, sizeof(RectUniforms), &cu);
        if (cover_ubuf)
        {
            rpe->setPipeline(path_fill_stencil_cover_pipeline_);
            rpe->setVertexBuffer(0, cover_vbuf);
            rpe->setVertexBuffer(1, cover_ubuf);
            rpe->setStencilReference(0);
            rpe->draw(6);
        }
    }

    rpe->end();
    return resolve_tex;
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
        // Stroke: the 4 corners as a closed polyline through
        // strokePolyline() — correctly handles rotation (unlike the old
        // always-axis-aligned 4-separate-rects approach: see TODO.md's
        // former "Real per-vertex quad rendering" entry, now resolved) and
        // gets real caps/joins for free. Local (pre-transform) corners:
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
// Stroke primitives — see this backend's header for the overall design.
// ---------------------------------------------------------------------------

void MetalDrawBackend::drawStrokeSegmentBody(
    const Offset& p0, const Offset& p1, float half_width,
    const Color& color, const Matrix4& transform,
    GPU::RenderPassEncoder& encoder)
{
    if (!line_pipeline_) return;

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

    auto ubuf = line_uniform_pool_.acquire(*device_, sizeof(LineUniforms), &u);
    if (!ubuf) return;

    encoder.setPipeline(line_pipeline_);
    encoder.setVertexBuffer(0, ubuf);
    encoder.draw(6);
}

void MetalDrawBackend::drawStrokeRoundPrimitive(
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

void MetalDrawBackend::strokePolyline(
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

void MetalDrawBackend::appendStrokePolylineBatched(
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
    // A line is just a 2-point open polyline -- strokePolyline() gives it
    // real caps (butt/round/square) for free; a plain 2-point stroke has no
    // interior vertex, so stroke_join never applies here.
    strokePolyline({cmd.p1, cmd.p2}, /*closed=*/false, cmd.paint, clip, transform, encoder);
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
    // already antialiased via strokePolyline()/appendStrokePolylineBatched().
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
                           ? nil // CreateStyledCTFont resolves nil to the OS system font
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
// measureTextInkBounds — tight glyph-path bounds, for single-line UI labels
// that want to center on the visible ink rather than the full typographic
// ascent+descent+leading box. See TextStyle::tight_vertical_bounds's doc.
//
// `span` must carry the same (already dpr-scaled) font_size RenderText
// passes to rasterizeText() at paint time — not the layout-time logical
// size — because the offset computed here has to land the ink exactly
// where rasterizeText()/drawTextTexture() actually draw it, and that
// pipeline rounds to physical pixels (ceil()) and bakes in a small
// raster/composite padding. Deriving this offset from logical (unscaled)
// metrics and only converting the *result* to logical afterwards was
// tried first and left a several-pixel residual — the two pixel-rounding
// steps don't commute with a later divide-by-dpr, so this mirrors
// rasterizeText()'s exact physical-pixel arithmetic instead of
// approximating it.
// ---------------------------------------------------------------------------

systems::leal::campello_widgets::Rect MetalDrawBackend::measureTextInkBounds(const TextSpan& span) const
{
    if (span.text.empty())
        return Rect{0.0f, 0.0f, 0.0f, 0.0f};

    @autoreleasepool {
        NSString *nsText = [NSString stringWithUTF8String:span.text.c_str()];
        if (!nsText || nsText.length == 0)
            return Rect{0.0f, 0.0f, 0.0f, 0.0f};

        const float fontSize = span.style.font_size > 0.0f ? span.style.font_size : 14.0f;

        NSString *family = span.style.font_family.empty()
                           ? nil // CreateStyledCTFont resolves nil to the OS system font
                           : [NSString stringWithUTF8String:span.style.font_family.c_str()];

        CTFontRef ctFont = CreateStyledCTFont(
            family, (CGFloat)fontSize, span.style.font_weight, span.style.italic);
        if (!ctFont)
            return Rect{0.0f, 0.0f, 0.0f, 0.0f};

        NSDictionary *attrs = @{ (__bridge NSString*)kCTFontAttributeName: (__bridge id)ctFont };
        CFRelease(ctFont);

        NSAttributedString *attrStr =
            [[NSAttributedString alloc] initWithString:nsText attributes:attrs];

        CTLineRef line = CTLineCreateWithAttributedString(
            (__bridge CFAttributedStringRef)attrStr);

        CGFloat ascent, descent, leading;
        CTLineGetTypographicBounds(line, &ascent, &descent, &leading);

        // Tight glyph-outline bounds, in the line's own coordinate space
        // (origin at the text position, i.e. baseline; y+ is up, matching
        // Quartz convention) — excludes the ascent/leading space reserved
        // for glyphs this particular string doesn't contain.
        CGRect inkRect = CTLineGetBoundsWithOptions(line, kCTLineBoundsUseGlyphPathBounds);
        CFRelease(line);

        // Exactly rasterizeText()'s own arithmetic (texH, baseline-from-
        // bottom) plus drawTextTexture()'s -1px composite shift, so the
        // baseline position computed here is guaranteed consistent with
        // where that pipeline actually draws it — see rasterizeText()'s
        // texH/fontDescent and drawTextTexture()'s y0 = t_origin.y - 1.
        const CGFloat fontDescent      = fabs(descent);
        const CGFloat texH             = ceil(ascent + descent + leading) + 2.0;
        const CGFloat baselineFromTop  = (texH - (fontDescent + 1.0)) - 1.0;
        const CGFloat inkTopAboveBase  = inkRect.origin.y + inkRect.size.height;
        const CGFloat inkTopFromTop    = baselineFromTop - inkTopAboveBase;

        return Rect{
            (float)inkRect.origin.x, (float)inkTopFromTop,
            (float)inkRect.size.width, (float)inkRect.size.height
        };
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
                           ? nil // CreateStyledCTFont resolves nil to the OS system font
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
        encoder,
        /*cached_bind_group=*/nullptr,
        cmd.filter_quality == FilterQuality::none ? nearest_sampler_ : quad_sampler_);
}

// ---------------------------------------------------------------------------
// drawTintedImage — mirrors drawImage() exactly, see DrawTintedImageCmd
// ---------------------------------------------------------------------------

void MetalDrawBackend::drawTintedImage(
    const DrawTintedImageCmd& cmd,
    const Matrix4&        transform,
    const Rect&           clip,
    GPU::RenderPassEncoder& encoder)
{
    if (!icon_pipeline_ || !quad_bgl_ || !quad_sampler_) return;
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

void MetalDrawBackend::drawTintedTexturedQuad(
    std::shared_ptr<GPU::Texture>  texture,
    const ProjectedCorner& c00, const ProjectedCorner& c10,
    const ProjectedCorner& c01, const ProjectedCorner& c11,
    const Color& tint,
    float opacity,
    GPU::RenderPassEncoder&        encoder,
    std::shared_ptr<GPU::Sampler>  sampler)
{
    if (!icon_pipeline_) { std::cerr << "[MetalDrawBackend] No icon pipeline!\n"; return; }
    if (!quad_bgl_)      { std::cerr << "[MetalDrawBackend] No bind group layout!\n"; return; }
    if (!quad_sampler_)  { std::cerr << "[MetalDrawBackend] No sampler!\n"; return; }
    if (!sampler) sampler = quad_sampler_;

    GPU::BindGroupDescriptor bgDesc{};
    bgDesc.layout  = quad_bgl_;
    bgDesc.entries = {
        GPU::BindGroupEntryDescriptor{ 0, texture },
        GPU::BindGroupEntryDescriptor{ 1, sampler },
    };
    auto bindGroup = device_->createBindGroup(bgDesc);
    if (!bindGroup) { std::cerr << "[MetalDrawBackend] Failed to create bind group!\n"; return; }

    // Same corner/winding layout as drawTexturedQuad() — geometry shape is
    // identical between the quad and icon pipelines, only the pipeline/
    // uniforms bound below differ.
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

    auto ubuf = icon_uniform_pool_.acquire(*device_, sizeof(IconUniforms), &u);
    if (!ubuf) return;

    encoder.setPipeline(icon_pipeline_);
    encoder.setBindGroup(0, bindGroup);
    encoder.setVertexBuffer(0, vbuf);
    encoder.setVertexBuffer(1, ubuf);
    encoder.draw(6);
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
    std::shared_ptr<GPU::BindGroup> cached_bind_group,
    std::shared_ptr<GPU::Sampler>   sampler)
{
    if (!quad_pipeline_) { std::cerr << "[MetalDrawBackend] No pipeline!\n"; return nullptr; }
    if (!quad_bgl_) { std::cerr << "[MetalDrawBackend] No bind group layout!\n"; return nullptr; }
    if (!quad_sampler_) { std::cerr << "[MetalDrawBackend] No sampler!\n"; return nullptr; }
    if (!sampler) sampler = quad_sampler_;

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
            GPU::BindGroupEntryDescriptor{ 1, sampler },
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

// Mirrors widgets.metal's LiquidGlassUniforms field-for-field.
struct alignas(16) LiquidGlassUniforms {
    float viewport[2];
    float size[2];
    float tint[4];
    float corner_radius;
    float refraction_strength;
    float specular_intensity;
    float _pad;
};

struct alignas(16) ShaderMaskUniforms {
    float viewport[2];
    float gradient_type;     // 0 = linear, 1 = radial, 2 = sweep
    float tile_mode;         // 0 = clamp, 1 = repeated, 2 = mirror
    float gradient_p1[4];    // linear: begin.xy; radial/sweep: center.xy
    float gradient_p2[4];    // linear: end.xy; radial: radius in [0]; sweep: start/end angle in [0]/[1]
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

    if (cmd.filter.kind == ImageFilterKind::liquidGlass &&
        liquid_glass_pipeline_ && quad_bgl_ && quad_sampler_)
    {
        GPU::BindGroupDescriptor bgDesc{};
        bgDesc.layout  = quad_bgl_;
        bgDesc.entries = {
            GPU::BindGroupEntryDescriptor{0, blurred_source},
            GPU::BindGroupEntryDescriptor{1, quad_sampler_},
        };
        auto bindGroup = device_->createBindGroup(bgDesc);
        if (!bindGroup) return;

        // local_uv is a plain 0..1 parametrization of the widget's own
        // rect — see LiquidGlassVertex's doc comment — matching
        // kQuadCorners' winding order: (00,10,01), (01,10,11).
        const LiquidGlassVertex verts[6] = {
            {c00.x(), c00.y(), c00.w(), uv00.x(), uv00.y(), 0.0f, 0.0f},
            {c10.x(), c10.y(), c10.w(), uv10.x(), uv10.y(), 1.0f, 0.0f},
            {c01.x(), c01.y(), c01.w(), uv01.x(), uv01.y(), 0.0f, 1.0f},
            {c01.x(), c01.y(), c01.w(), uv01.x(), uv01.y(), 0.0f, 1.0f},
            {c10.x(), c10.y(), c10.w(), uv10.x(), uv10.y(), 1.0f, 0.0f},
            {c11.x(), c11.y(), c11.w(), uv11.x(), uv11.y(), 1.0f, 1.0f},
        };
        auto vbuf = liquid_glass_vertex_pool_.acquire(*device_, sizeof(verts), verts);
        if (!vbuf) return;

        LiquidGlassUniforms u{};
        u.viewport[0]            = vp_w_;
        u.viewport[1]            = vp_h_;
        u.size[0]                = cmd.bounds.width;
        u.size[1]                = cmd.bounds.height;
        u.tint[0]                = cmd.filter.tint.r;
        u.tint[1]                = cmd.filter.tint.g;
        u.tint[2]                = cmd.filter.tint.b;
        u.tint[3]                = cmd.filter.tint.a;
        u.corner_radius          = cmd.filter.corner_radius;
        u.refraction_strength    = cmd.filter.refraction_strength;
        u.specular_intensity     = cmd.filter.specular_intensity;

        auto ubuf = liquid_glass_uniform_pool_.acquire(*device_, sizeof(LiquidGlassUniforms), &u);
        if (!ubuf) return;

        encoder.setPipeline(liquid_glass_pipeline_);
        encoder.setBindGroup(0, bindGroup);
        encoder.setVertexBuffer(0, vbuf);
        encoder.setVertexBuffer(1, ubuf);
        encoder.draw(6);
        return;
    }

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

        // Find the two stops that bracket t.
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
    float tile_mode      = 0.0f;
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
    u.tile_mode       = tile_mode;
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
