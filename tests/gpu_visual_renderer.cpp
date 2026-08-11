#include "gpu_visual_renderer.hpp"
#include "gpu_visual_renderer_backend.hpp"

#include <campello_widgets/ui/canvas.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/draw_command.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <campello_widgets/ui/path.hpp>
#include <campello_widgets/ui/rrect.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <campello_widgets/ui/shader.hpp>
#include <campello_widgets/ui/text_span.hpp>

#include <campello_gpu/device.hpp>
#include <campello_gpu/texture.hpp>
#include <campello_gpu/texture_view.hpp>
#include <campello_gpu/command_encoder.hpp>
#include <campello_gpu/render_pass_encoder.hpp>
#include <campello_gpu/descriptors/begin_render_pass_descriptor.hpp>
#include <campello_gpu/constants/texture_type.hpp>
#include <campello_gpu/constants/texture_usage.hpp>
#include <campello_gpu/constants/pixel_format.hpp>

#include <vector_math/matrix4.hpp>
#include <vector_math/vector3.hpp>
#include <vector_math/vector4.hpp>

// stb_image_write — implementation included here
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../tests/third_party/stb_image_write.h"

#include <algorithm>
#include <memory>
#include <vector>
#include <variant>
#include <cmath>
#include <iostream>

namespace GPU = systems::leal::campello_gpu;
namespace vm  = systems::leal::vector_math;
namespace cw  = systems::leal::campello_widgets;
namespace cwt = systems::leal::campello_widgets::testing;

using Matrix4 = cw::Matrix4;

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

struct cwt::GpuVisualRenderer::Impl
{
    std::shared_ptr<GPU::Device>  device;
    std::shared_ptr<GPU::Texture> color_tex;
    std::unique_ptr<cw::IDrawBackend> backend;

    int   width  = 0;
    int   height = 0;
    bool  valid  = false;
    cw::Color clear_color{1.0f, 1.0f, 1.0f, 1.0f};
};

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

std::shared_ptr<GPU::Device> cwt::GpuVisualRenderer::sharedDevice()
{
    // Function-local static: created once per process, reused by every
    // GpuVisualRenderer and by any test that needs to build GPU resources
    // (e.g. an image texture) on the same logical device. See the header
    // doc comment — mixing devices is invalid, not just slow.
    static std::shared_ptr<GPU::Device> device = GPU::Device::createDefaultDevice(nullptr);
    return device;
}

cwt::GpuVisualRenderer::GpuVisualRenderer(int width, int height)
    : impl_(std::make_unique<Impl>())
{
    impl_->width  = width;
    impl_->height = height;

    impl_->device = sharedDevice();
    if (!impl_->device) {
        std::cerr << "[GpuVisualRenderer] No GPU device available — falling back to CPU\n";
        return;
    }

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

    impl_->backend = createVisualRendererBackend(
        impl_->device,
        cw::Color::white(),
        GPU::PixelFormat::rgba8unorm);

    if (!impl_->backend) {
        std::cerr << "[GpuVisualRenderer] Backend creation failed — falling back to CPU\n";
        return;
    }

    impl_->backend->setViewport(static_cast<float>(width),
                                static_cast<float>(height));

    impl_->valid = true;
}

cwt::GpuVisualRenderer::~GpuVisualRenderer() = default;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool cwt::GpuVisualRenderer::isValid() const { return impl_ && impl_->valid; }

void cwt::GpuVisualRenderer::setClearColor(const cw::Color& c) {
    if (impl_) impl_->clear_color = c;
}

int cwt::GpuVisualRenderer::width()  const { return impl_ ? impl_->width  : 0; }
int cwt::GpuVisualRenderer::height() const { return impl_ ? impl_->height : 0; }

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

namespace {

std::shared_ptr<GPU::RenderPassEncoder> beginRenderPassOnTarget(
    GPU::CommandEncoder& encoder,
    const std::shared_ptr<GPU::TextureView>& target_view,
    const cw::Color& clear_color,
    GPU::LoadOp load_op)
{
    GPU::ColorAttachment ca{};
    ca.view           = target_view;
    ca.loadOp         = load_op;
    ca.storeOp        = GPU::StoreOp::store;
    ca.clearValue[0]  = clear_color.r;
    ca.clearValue[1]  = clear_color.g;
    ca.clearValue[2]  = clear_color.b;
    ca.clearValue[3]  = clear_color.a;
    ca.depthSlice     = 0;

    GPU::BeginRenderPassDescriptor rp_desc{};
    rp_desc.colorAttachments = {ca};

    return encoder.beginRenderPass(rp_desc);
}

struct FlushState
{
    cw::IDrawBackend& backend;
    GPU::CommandEncoder& encoder;
    std::shared_ptr<GPU::TextureView> main_target_view;
    std::shared_ptr<GPU::TextureView> current_target_view;
    std::shared_ptr<GPU::RenderPassEncoder>& rpe;
    float main_viewport_w = 0.0f;
    float main_viewport_h = 0.0f;
};

bool restartMainPass(FlushState& s, const cw::Color& /*clear_color*/)
{
    s.rpe->end();
    s.rpe = beginRenderPassOnTarget(
        s.encoder,
        s.main_target_view,
        cw::Color{0, 0, 0, 0},
        GPU::LoadOp::load);
    s.current_target_view = s.main_target_view;
    return s.rpe != nullptr;
}

// Forward declaration — defined after the apply helpers that use it.
bool flushDrawListImpl(
    const cw::DrawList& commands,
    cw::IDrawBackend& backend,
    GPU::CommandEncoder& encoder,
    const std::shared_ptr<GPU::TextureView>& main_target_view,
    const std::shared_ptr<GPU::TextureView>& initial_target_view,
    std::shared_ptr<GPU::RenderPassEncoder>& rpe,
    float viewport_width,
    float viewport_height,
    const Matrix4& base_transform,
    float dpr);

bool renderChildCommandsToTexture(
    FlushState& s,
    const std::shared_ptr<GPU::Texture>& child_tex,
    const cw::Rect& bounds,
    const cw::DrawList& child_cmds,
    float dpr)
{
    auto child_rpe = s.backend.beginOffscreenPass(child_tex, s.encoder);
    if (!child_rpe) return false;

    // Mirrors src/ui/renderer.cpp's translateChildCommands() — see its doc
    // comment for the full explanation. Short version: a bare
    // PushTransformCmd{translate(-bounds.x,-bounds.y)} shifts ordinary draw
    // commands correctly (the backend applies current_transform to their
    // geometry), but PushClipRectCmd/PushClipRRectCmd/PushClipOvalCmd store
    // an already-absolute rect that flushDrawListImpl() assigns directly to
    // current_clip without applying current_transform — correct for the
    // normal (non-offscreen) case, where the transform was already baked in
    // at capture time, but wrong here, where this offset didn't exist yet
    // when child_cmds was captured. Left uncorrected, a clip nested inside
    // this offscreen child (e.g. RenderImage's own clipRect for
    // BoxFit.none/fitHeight, nested inside a ClipRRect ancestor) clips to a
    // region entirely outside the child texture's small (0,0)-origin
    // bounds, hiding the content completely.
    Matrix4 offset_mat = Matrix4::translate(
        vm::Vector3<float>(-bounds.x, -bounds.y, 0.0f));

    cw::DrawList translated;
    translated.push_back(cw::PushTransformCmd{offset_mat});
    for (const auto& cc : child_cmds)
    {
        std::visit([&](auto arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, cw::PushClipRectCmd>)
            {
                arg.rect.x -= bounds.x;
                arg.rect.y -= bounds.y;
            }
            else if constexpr (std::is_same_v<T, cw::PushClipRRectCmd>)
            {
                arg.rrect.rect.x -= bounds.x;
                arg.rrect.rect.y -= bounds.y;
            }
            else if constexpr (std::is_same_v<T, cw::PushClipOvalCmd>)
            {
                arg.rect.x -= bounds.x;
                arg.rect.y -= bounds.y;
            }
            translated.push_back(arg);
        }, cc);
    }
    translated.push_back(cw::PopTransformCmd{});

    const uint32_t tw = static_cast<uint32_t>(std::ceil(bounds.width  * dpr));
    const uint32_t th = static_cast<uint32_t>(std::ceil(bounds.height * dpr));

    s.backend.setViewportSize(static_cast<float>(tw), static_cast<float>(th));

    auto child_view = child_tex->createView(s.backend.offscreenPixelFormat(), 1);

    bool ok = flushDrawListImpl(
        translated,
        s.backend,
        s.encoder,
        child_view,
        child_view,
        child_rpe,
        static_cast<float>(tw),
        static_cast<float>(th),
        Matrix4::identity(),
        dpr);

    child_rpe->end();
    return ok;
}

void applySaveLayer(
    FlushState& s,
    const cw::SaveLayerCmd& cmd,
    const cw::DrawList& child_cmds,
    const cw::Color& clear_color,
    float dpr)
{
    if (child_cmds.empty()) return;
    if (!std::isfinite(cmd.bounds.width) || !std::isfinite(cmd.bounds.height) ||
        cmd.bounds.width <= 0.0f || cmd.bounds.height <= 0.0f)
        return;

    const uint32_t tw = static_cast<uint32_t>(std::ceil(cmd.bounds.width  * dpr));
    const uint32_t th = static_cast<uint32_t>(std::ceil(cmd.bounds.height * dpr));
    if (tw == 0 || th == 0) return;

    auto child_tex = s.backend.createOffscreenTexture(tw, th);
    if (!child_tex) return;

    s.rpe->end();

    bool child_ok = renderChildCommandsToTexture(s, child_tex, cmd.bounds, child_cmds, dpr);

    s.backend.setViewportSize(s.main_viewport_w, s.main_viewport_h);

    if (!restartMainPass(s, clear_color)) return;

    if (child_ok) {
        s.backend.saveLayerComposite(child_tex, cmd,
            Matrix4::identity(), cw::Rect::fromLTWH(0, 0, s.main_viewport_w, s.main_viewport_h), *s.rpe);
    }
}

void applyShaderMask(
    FlushState& s,
    const cw::DrawShaderMaskBeginCmd& cmd,
    const cw::DrawList& child_cmds,
    const cw::Color& clear_color,
    float dpr)
{
    if (child_cmds.empty()) return;
    if (!std::isfinite(cmd.bounds.width) || !std::isfinite(cmd.bounds.height) ||
        cmd.bounds.width <= 0.0f || cmd.bounds.height <= 0.0f)
        return;

    const uint32_t tw = static_cast<uint32_t>(std::ceil(cmd.bounds.width  * dpr));
    const uint32_t th = static_cast<uint32_t>(std::ceil(cmd.bounds.height * dpr));
    if (tw == 0 || th == 0) return;

    auto child_tex = s.backend.createOffscreenTexture(tw, th);
    if (!child_tex) return;

    s.rpe->end();

    bool child_ok = renderChildCommandsToTexture(s, child_tex, cmd.bounds, child_cmds, dpr);

    s.backend.setViewportSize(s.main_viewport_w, s.main_viewport_h);

    if (!restartMainPass(s, clear_color)) return;

    if (child_ok) {
        s.backend.drawShaderMaskComposite(child_tex, cmd,
            Matrix4::identity(), cw::Rect::fromLTWH(0, 0, s.main_viewport_w, s.main_viewport_h), *s.rpe);
    }
}

void applyClipShape(
    FlushState& s,
    const cw::Rect& bounds,
    float corner_radius,
    bool is_oval,
    const cw::DrawList& child_cmds,
    const cw::Color& clear_color,
    float dpr)
{
    if (child_cmds.empty()) return;
    if (!std::isfinite(bounds.width) || !std::isfinite(bounds.height) ||
        bounds.width <= 0.0f || bounds.height <= 0.0f)
        return;

    const uint32_t tw = static_cast<uint32_t>(std::ceil(bounds.width  * dpr));
    const uint32_t th = static_cast<uint32_t>(std::ceil(bounds.height * dpr));
    if (tw == 0 || th == 0) return;

    auto child_tex = s.backend.createOffscreenTexture(tw, th);
    if (!child_tex) return;

    s.rpe->end();

    bool child_ok = renderChildCommandsToTexture(s, child_tex, bounds, child_cmds, dpr);

    s.backend.setViewportSize(s.main_viewport_w, s.main_viewport_h);

    if (!restartMainPass(s, clear_color)) return;

    if (child_ok) {
        s.backend.drawClipShapeComposite(child_tex, bounds, corner_radius, is_oval,
            Matrix4::identity(), cw::Rect::fromLTWH(0, 0, s.main_viewport_w, s.main_viewport_h), *s.rpe);
    }
}

bool flushDrawListImpl(
    const cw::DrawList& commands,
    cw::IDrawBackend& backend,
    GPU::CommandEncoder& encoder,
    const std::shared_ptr<GPU::TextureView>& main_target_view,
    const std::shared_ptr<GPU::TextureView>& initial_target_view,
    std::shared_ptr<GPU::RenderPassEncoder>& rpe,
    float viewport_width,
    float viewport_height,
    const Matrix4& base_transform,
    float dpr)
{
    backend.onBeginFlush();
    backend.setViewportSize(viewport_width, viewport_height);

    Matrix4              current_transform = base_transform;
    std::vector<Matrix4> transform_stack;
    std::vector<cw::Rect> clip_stack;
    cw::Rect current_clip = cw::Rect::fromLTWH(0, 0, viewport_width, viewport_height);

    bool in_shader_mask = false;
    cw::DrawList shader_mask_cmds;
    cw::DrawShaderMaskBeginCmd shader_mask_info{cw::Rect{}, cw::LinearGradient{}};

    bool in_save_layer = false;
    cw::DrawList save_layer_cmds;
    cw::SaveLayerCmd save_layer_info{cw::Rect{}, cw::Paint{}};

    bool     in_clip_shape    = false;
    int      clip_shape_depth = 0;
    cw::DrawList clip_shape_cmds;
    cw::Rect clip_shape_bounds;
    float    clip_shape_corner_r = 0.0f;
    bool     clip_shape_is_oval  = false;

    FlushState state{
        backend,
        encoder,
        main_target_view,
        initial_target_view,
        rpe,
        viewport_width,
        viewport_height
    };

    // Dummy clear color for restartMainPass — load op is load, so clear values are ignored.
    const cw::Color restart_clear{};

    for (const auto& cmd : commands)
    {
        std::visit([&](const auto& c)
        {
            using T = std::decay_t<decltype(c)>;

            // ── ShaderMask child accumulation ────────────────────────────────
            if (in_shader_mask)
            {
                if constexpr (std::is_same_v<T, cw::DrawShaderMaskEndCmd>)
                {
                    in_shader_mask = false;
                    applyShaderMask(state, shader_mask_info, shader_mask_cmds, restart_clear, dpr);
                    shader_mask_cmds.clear();
                }
                else
                {
                    shader_mask_cmds.push_back(c);
                }
                return;
            }

            // ── SaveLayer child accumulation ─────────────────────────────────
            if (in_save_layer)
            {
                if constexpr (std::is_same_v<T, cw::SaveLayerEndCmd>)
                {
                    in_save_layer = false;
                    applySaveLayer(state, save_layer_info, save_layer_cmds, restart_clear, dpr);
                    save_layer_cmds.clear();
                }
                else
                {
                    save_layer_cmds.push_back(c);
                }
                return;
            }

            // ── ClipRRect/ClipOval/ClipPath child accumulation ───────────────
            if (in_clip_shape)
            {
                if constexpr (std::is_same_v<T, cw::PushClipRectCmd>   ||
                              std::is_same_v<T, cw::PushClipRRectCmd>  ||
                              std::is_same_v<T, cw::PushClipOvalCmd>   ||
                              std::is_same_v<T, cw::PushClipPathCmd>)
                {
                    ++clip_shape_depth;
                    clip_shape_cmds.push_back(c);
                }
                else if constexpr (std::is_same_v<T, cw::PopClipRectCmd>)
                {
                    --clip_shape_depth;
                    if (clip_shape_depth == 0)
                    {
                        in_clip_shape = false;
                        applyClipShape(state, clip_shape_bounds, clip_shape_corner_r,
                                       clip_shape_is_oval, clip_shape_cmds, restart_clear, dpr);
                        clip_shape_cmds.clear();
                    }
                    else
                    {
                        clip_shape_cmds.push_back(c);
                    }
                }
                else
                {
                    clip_shape_cmds.push_back(c);
                }
                return;
            }

            // ── Main dispatch ────────────────────────────────────────────────
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
            else if constexpr (std::is_same_v<T, cw::DrawPointsCmd>)
                backend.drawPoints(c, current_transform, current_clip, *rpe);
            else if constexpr (std::is_same_v<T, cw::DrawArcCmd>)
                backend.drawArc(c, current_transform, current_clip, *rpe);
            else if constexpr (std::is_same_v<T, cw::DrawPathCmd>)
                backend.drawPath(c, current_transform, current_clip, *rpe);
            else if constexpr (std::is_same_v<T, cw::DrawTextCmd>)
            {
                uint32_t w = 0, h = 0;
                auto tex = backend.rasterizeText(c.span, 1.0f, w, h);
                if (tex)
                    backend.drawTextTexture(tex, nullptr, w, h, c.origin,
                        current_transform, current_clip, *rpe);
            }
            else if constexpr (std::is_same_v<T, cw::DrawImageCmd>)
                backend.drawImage(c, current_transform, current_clip, *rpe);
            else if constexpr (std::is_same_v<T, cw::PushTransformCmd>)
            {
                transform_stack.push_back(current_transform);
                current_transform = current_transform * c.transform;
            }
            else if constexpr (std::is_same_v<T, cw::PopTransformCmd>)
            {
                if (!transform_stack.empty())
                {
                    current_transform = transform_stack.back();
                    transform_stack.pop_back();
                }
            }
            else if constexpr (std::is_same_v<T, cw::PushClipRectCmd>)
            {
                clip_stack.push_back(current_clip);
                current_clip = c.rect;
            }
            else if constexpr (std::is_same_v<T, cw::PushClipPathCmd>)
            {
                clip_stack.push_back(current_clip);
                current_clip = current_clip.intersection(c.path.getBounds());
            }
            else if constexpr (std::is_same_v<T, cw::PopClipRectCmd>)
            {
                if (!clip_stack.empty())
                {
                    current_clip = clip_stack.back();
                    clip_stack.pop_back();
                }
            }
            else if constexpr (std::is_same_v<T, cw::PushClipRRectCmd>)
            {
                in_clip_shape    = true;
                clip_shape_depth = 1;
                clip_shape_cmds.clear();
                clip_shape_bounds   = c.rrect.rect;
                clip_shape_corner_r = c.rrect.radius_x;
                clip_shape_is_oval  = false;
            }
            else if constexpr (std::is_same_v<T, cw::PushClipOvalCmd>)
            {
                in_clip_shape    = true;
                clip_shape_depth = 1;
                clip_shape_cmds.clear();
                clip_shape_bounds   = c.rect;
                clip_shape_corner_r = 0.0f;
                clip_shape_is_oval  = true;
            }
            else if constexpr (std::is_same_v<T, cw::SaveLayerCmd>)
            {
                in_save_layer  = true;
                save_layer_info = c;
            }
            else if constexpr (std::is_same_v<T, cw::SaveLayerEndCmd>)
            {
                // Mismatched end — ignore.
            }
            else if constexpr (std::is_same_v<T, cw::DrawShaderMaskBeginCmd>)
            {
                in_shader_mask  = true;
                shader_mask_info = c;
            }
            else if constexpr (std::is_same_v<T, cw::DrawShaderMaskEndCmd>)
            {
                // Mismatched end — ignore.
            }
            else if constexpr (std::is_same_v<T, cw::DrawBackdropFilterBeginCmd> ||
                               std::is_same_v<T, cw::DrawBackdropFilterEndCmd>   ||
                               std::is_same_v<T, cw::DrawSurfaceUpdateBeginCmd> ||
                               std::is_same_v<T, cw::DrawSurfaceUpdateEndCmd>   ||
                               std::is_same_v<T, cw::CacheReplayBeginCmd>       ||
                               std::is_same_v<T, cw::CacheReplayEndCmd>         ||
                               std::is_same_v<T, cw::DrawShadowCmd>)
            {
                // Unsupported in GpuVisualRenderer — skip silently.
            }
        }, cmd);
    }

    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// renderDrawList
// ---------------------------------------------------------------------------

bool cwt::GpuVisualRenderer::renderDrawList(const DrawList& commands)
{
    if (!isValid()) return false;

    auto& dev = *impl_->device;
    auto  encoder = dev.createCommandEncoder();
    if (!encoder) return false;

    auto target_view = impl_->color_tex->createView(
        GPU::PixelFormat::rgba8unorm,
        1);
    if (!target_view) return false;

    auto rpe = beginRenderPassOnTarget(
        *encoder,
        target_view,
        impl_->clear_color,
        GPU::LoadOp::clear);
    if (!rpe) return false;

    const bool ok = flushDrawListImpl(
        commands,
        *impl_->backend,
        *encoder,
        target_view,
        target_view,
        rpe,
        static_cast<float>(impl_->width),
        static_cast<float>(impl_->height),
        Matrix4::identity(),
        1.0f);

    if (ok)
        rpe->end();

    auto cmd_buf = encoder->finish();
    impl_->device->submit(std::move(cmd_buf));

    return ok;
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

    if (!impl_->color_tex->download(0, 0, pixels.data(), byte_count)) {
        std::cerr << "[GpuVisualRenderer] Texture::download failed\n";
        return false;
    }

    // The render target stores premultiplied-alpha color (as produced by our
    // shaders), but PNG — and the Flutter goldens we diff against — use
    // straight (unpremultiplied) alpha. Unpremultiply before writing so a
    // byte-for-byte comparison against a Flutter golden is meaningful.
    for (uint64_t i = 0; i < byte_count; i += 4) {
        const uint8_t a = pixels[i + 3];
        if (a == 0 || a == 255) continue;
        for (int c = 0; c < 3; ++c) {
            const int straight = (static_cast<int>(pixels[i + c]) * 255 + a / 2) / a;
            pixels[i + c] = static_cast<uint8_t>(std::min(255, straight));
        }
    }

    int result = stbi_write_png(
        filepath.c_str(),
        impl_->width, impl_->height,
        4, pixels.data(),
        impl_->width * 4);

    return result != 0;
}
