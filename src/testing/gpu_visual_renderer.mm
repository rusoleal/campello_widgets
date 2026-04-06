#import "gpu_visual_renderer.hpp"
#import <campello_widgets/ui/draw_command.hpp>
#import <campello_widgets/ui/color.hpp>
#import <campello_widgets/ui/rect.hpp>

#import <campello_gpu/device.hpp>
#import <campello_gpu/texture.hpp>
#import <campello_gpu/command_encoder.hpp>
#import <campello_gpu/render_pass_encoder.hpp>
#import <campello_gpu/descriptors/begin_render_pass_descriptor.hpp>
#import <campello_gpu/constants/texture_type.hpp>
#import <campello_gpu/constants/texture_usage.hpp>
#import <campello_gpu/constants/pixel_format.hpp>
#import <campello_gpu/constants/buffer_usage.hpp>

// Metal draw backend (private header — same library)
#import "../../src/macos/metal_draw_backend.hpp"

#import <vector_math/vector4.hpp>

// stb_image_write — implementation included here
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../tests/third_party/stb_image_write.h"

#include <variant>
#include <cmath>
#include <iostream>

namespace GPU = systems::leal::campello_gpu;
namespace vm  = systems::leal::vector_math;
namespace cw  = systems::leal::campello_widgets;
namespace cwt = systems::leal::campello_widgets::testing;

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

struct cwt::GpuVisualRenderer::Impl
{
    std::shared_ptr<GPU::Device>  device;
    std::shared_ptr<GPU::Texture> color_tex;
    std::unique_ptr<MetalDrawBackend> backend;

    int   width  = 0;
    int   height = 0;
    bool  valid  = false;
    Color clear_color{1.0f, 1.0f, 1.0f, 1.0f};
};

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

cwt::GpuVisualRenderer::GpuVisualRenderer(int width, int height)
    : impl_(std::make_unique<Impl>())
{
    impl_->width  = width;
    impl_->height = height;

    // Create headless device (nullptr = no swapchain needed)
    impl_->device = GPU::Device::createDefaultDevice(nullptr);
    if (!impl_->device) {
        std::cerr << "[GpuVisualRenderer] No GPU device available — falling back to CPU\n";
        return;
    }

    // Offscreen texture: rgba8unorm, renderTarget | copySrc
    impl_->color_tex = impl_->device->createTexture(
        GPU::TextureType::tt2d,
        GPU::PixelFormat::rgba8unorm,
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height),
        1, 1, 1,
        static_cast<GPU::TextureUsage>(
            static_cast<int>(GPU::TextureUsage::renderTarget) |
            static_cast<int>(GPU::TextureUsage::copySrc)));
    if (!impl_->color_tex) {
        std::cerr << "[GpuVisualRenderer] Failed to create offscreen texture\n";
        return;
    }

    // MetalDrawBackend compiled for rgba8unorm (matches offscreen texture)
    impl_->backend = std::make_unique<MetalDrawBackend>(
        impl_->device,
        cw::Color::white(),
        GPU::PixelFormat::rgba8unorm);

    impl_->backend->setViewport(static_cast<float>(width),
                                static_cast<float>(height));

    if (!impl_->backend->isValid()) {
        std::cerr << "[GpuVisualRenderer] MetalDrawBackend pipelines failed to compile — falling back to CPU\n";
        return;
    }
    impl_->valid = true;
}

cwt::GpuVisualRenderer::~GpuVisualRenderer() = default;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool cwt::GpuVisualRenderer::isValid() const { return impl_ && impl_->valid; }

void cwt::GpuVisualRenderer::setClearColor(const Color& c) {
    if (impl_) impl_->clear_color = c;
}

int cwt::GpuVisualRenderer::width()  const { return impl_ ? impl_->width  : 0; }
int cwt::GpuVisualRenderer::height() const { return impl_ ? impl_->height : 0; }

// ---------------------------------------------------------------------------
// renderDrawList
// ---------------------------------------------------------------------------

bool cwt::GpuVisualRenderer::renderDrawList(const DrawList& commands)
{
    if (!isValid()) return false;

    // --- Pre-flight: reject unsupported commands so caller can use CPU ---
    for (const auto& cmd : commands)
    {
        bool unsupported = std::visit([](const auto& c) -> bool {
            using T = std::decay_t<decltype(c)>;
            return std::is_same_v<T, cw::DrawPathCmd>   ||
                   std::is_same_v<T, cw::DrawShadowCmd> ||
                   std::is_same_v<T, cw::PushClipPathCmd> ||
                   std::is_same_v<T, cw::SaveLayerCmd>  ||
                   std::is_same_v<T, cw::DrawArcCmd>    ||
                   std::is_same_v<T, cw::DrawPointsCmd>;
        }, cmd);
        if (unsupported) return false;
    }

    auto& dev = *impl_->device;
    auto  encoder = dev.createCommandEncoder();
    if (!encoder) return false;

    // Render pass targeting the offscreen texture
    auto tex_view = impl_->color_tex->createView(
        GPU::PixelFormat::rgba8unorm,
        1);  // arrayLayerCount = 1 (non-array 2D texture)
    if (!tex_view) return false;

    const Color& cc = impl_->clear_color;
    GPU::ColorAttachment ca{};
    ca.view           = tex_view;
    ca.loadOp         = GPU::LoadOp::clear;
    ca.storeOp        = GPU::StoreOp::store;
    ca.clearValue[0]  = cc.r;
    ca.clearValue[1]  = cc.g;
    ca.clearValue[2]  = cc.b;
    ca.clearValue[3]  = cc.a;
    ca.depthSlice     = 0;

    GPU::BeginRenderPassDescriptor rp_desc{};
    rp_desc.colorAttachments = {ca};

    auto rpe = encoder->beginRenderPass(rp_desc);
    if (!rpe) return false;

    auto& backend = *impl_->backend;

    // Replay draw list — accumulate transforms, pre-apply to each command
    using Matrix4 = cw::Matrix4;
    Matrix4              current_transform = Matrix4::identity();
    std::vector<Matrix4> transform_stack;
    std::vector<cw::Rect> clip_stack;
    cw::Rect current_clip = cw::Rect::fromLTWH(0, 0,
        static_cast<float>(impl_->width),
        static_cast<float>(impl_->height));

    for (const auto& cmd : commands)
    {
        std::visit([&](const auto& c) {
            using T = std::decay_t<decltype(c)>;

            if constexpr (std::is_same_v<T, cw::DrawRectCmd>)
                backend.drawRect(c, current_transform, current_clip, *rpe);
            else if constexpr (std::is_same_v<T, cw::DrawCircleCmd>)
                backend.drawCircle(c, current_transform, current_clip, *rpe);
            else if constexpr (std::is_same_v<T, cw::DrawOvalCmd>)
                backend.drawOval(c, current_transform, current_clip, *rpe);
            else if constexpr (std::is_same_v<T, cw::DrawRRectCmd>)
                backend.drawRRect(c, current_transform, current_clip, *rpe);
            else if constexpr (std::is_same_v<T, cw::DrawLineCmd>)
                backend.drawLine(c, current_transform, current_clip, *rpe);
            else if constexpr (std::is_same_v<T, cw::DrawTextCmd>)
                backend.drawText(c, current_transform, current_clip, *rpe);
            else if constexpr (std::is_same_v<T, cw::DrawImageCmd>)
                backend.drawImage(c, current_transform, current_clip, *rpe);
            else if constexpr (std::is_same_v<T, cw::PushTransformCmd>)
            {
                transform_stack.push_back(current_transform);
                current_transform = current_transform * c.transform;
            }
            else if constexpr (std::is_same_v<T, cw::PopTransformCmd>)
            {
                if (!transform_stack.empty()) {
                    current_transform = transform_stack.back();
                    transform_stack.pop_back();
                }
            }
            else if constexpr (std::is_same_v<T, cw::PushClipRectCmd>)
            {
                clip_stack.push_back(current_clip);
                current_clip = c.rect;
            }
            else if constexpr (std::is_same_v<T, cw::PushClipRRectCmd>)
            {
                clip_stack.push_back(current_clip);
                current_clip = c.rrect.rect;   // approximate: use bounding rect
            }
            else if constexpr (std::is_same_v<T, cw::PopClipRectCmd>)
            {
                if (!clip_stack.empty()) {
                    current_clip = clip_stack.back();
                    clip_stack.pop_back();
                }
            }
            // Unsupported types already filtered above — nothing to do here.
        }, cmd);
    }

    rpe->end();

    auto cmd_buf = encoder->finish();
    impl_->device->submit(std::move(cmd_buf));

    return true;
}

// ---------------------------------------------------------------------------
// saveToPng
// ---------------------------------------------------------------------------

bool cwt::GpuVisualRenderer::saveToPng(const std::string& filepath)
{
    if (!isValid() || !impl_->color_tex) return false;

    const uint64_t byte_count =
        static_cast<uint64_t>(impl_->width) *
        static_cast<uint64_t>(impl_->height) * 4;

    std::vector<uint8_t> pixels(byte_count);

    // Synchronous GPU → CPU readback (blocks until GPU work completes)
    if (!impl_->color_tex->download(0, 0, pixels.data(), byte_count)) {
        std::cerr << "[GpuVisualRenderer] Texture::download failed\n";
        return false;
    }

    int result = stbi_write_png(
        filepath.c_str(),
        impl_->width, impl_->height,
        4, pixels.data(),
        impl_->width * 4);

    return result != 0;
}
