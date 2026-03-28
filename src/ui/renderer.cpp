#include <campello_widgets/ui/renderer.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/debug_flags.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/ticker.hpp>
#include <campello_widgets/ui/text_span.hpp>
#include <campello_widgets/ui/text_style.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>

#include <campello_gpu/device.hpp>
#include <campello_gpu/command_encoder.hpp>
#include <campello_gpu/render_pass_encoder.hpp>
#include <campello_gpu/texture_view.hpp>
#include <campello_gpu/descriptors/begin_render_pass_descriptor.hpp>

#include <variant>

namespace systems::leal::campello_widgets
{

    Renderer::Renderer(
        std::shared_ptr<campello_gpu::Device> device,
        std::shared_ptr<RenderBox>            root_render_object,
        Color                                 clear_color)
        : device_(std::move(device))
        , root_(std::move(root_render_object))
        , clear_color_(clear_color)
    {
        // Register as current renderer for view insets queries.
        detail::currentRenderer() = this;
    }

    Renderer::~Renderer()
    {
        // Unregister if we were the current renderer.
        if (detail::currentRenderer() == this) {
            detail::currentRenderer() = nullptr;
        }
    }

    // ------------------------------------------------------------------
    // Public API
    // ------------------------------------------------------------------

    bool Renderer::renderFrame(
        std::shared_ptr<campello_gpu::TextureView> target,
        float viewport_width,
        float viewport_height)
    {
        const auto now_tp = std::chrono::steady_clock::now().time_since_epoch();
        const uint64_t now_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(now_tp).count();

        // Record frame delta for the performance overlay.
        if (last_frame_ms_ != 0)
        {
            const float delta_ms = static_cast<float>(now_ms - last_frame_ms_);
            frame_times_ms_[perf_head_] = delta_ms;
            perf_head_ = (perf_head_ + 1) % kPerfSamples;
            if (perf_count_ < kPerfSamples) ++perf_count_;
        }
        last_frame_ms_ = now_ms;

        if (auto* d = PointerDispatcher::activeDispatcher())
            d->tick(now_ms);

        if (auto* ts = TickerScheduler::active())
            ts->tick(now_ms);

        layoutPass(viewport_width, viewport_height);

        if (!root_ || !root_->needsPaint())
            return false;

        auto encoder = device_->createCommandEncoder();

        // Begin render pass targeting the provided texture view.
        campello_gpu::ColorAttachment ca{};
        ca.view             = target;
        ca.loadOp           = campello_gpu::LoadOp::clear;
        ca.storeOp          = campello_gpu::StoreOp::store;
        ca.clearValue[0]    = clear_color_.r;
        ca.clearValue[1]    = clear_color_.g;
        ca.clearValue[2]    = clear_color_.b;
        ca.clearValue[3]    = clear_color_.a;
        ca.depthSlice       = 0;

        campello_gpu::BeginRenderPassDescriptor rp_desc{};
        rp_desc.colorAttachments = {ca};

        auto rpe = encoder->beginRenderPass(rp_desc);

        paintPass(*rpe, viewport_width, viewport_height);

        rpe->end();

        auto cmd_buffer = encoder->finish();
        device_->submit(std::move(cmd_buffer));
        return true;
    }

    // ------------------------------------------------------------------
    // Private
    // ------------------------------------------------------------------

    void Renderer::layoutPass(float viewport_width, float viewport_height)
    {
        if (!root_) return;

        // Apply view insets (safe area) to reduce available space.
        // The root widget is laid out within the "safe" area only.
        const float safe_width  = viewport_width - view_insets_.horizontal();
        const float safe_height = viewport_height - view_insets_.vertical();

        const BoxConstraints screen_constraints =
            BoxConstraints::tight(safe_width, safe_height);

        RenderObject::setActiveBackend(draw_backend_.get());
        root_->layout(screen_constraints);
        RenderObject::setActiveBackend(nullptr);
    }

    void Renderer::paintPass(
        campello_gpu::RenderPassEncoder& encoder,
        float viewport_width,
        float viewport_height)
    {
        if (!root_) return;

        PaintContext ctx(encoder, viewport_width, viewport_height);
        // Offset by view insets so root widget paints within safe area.
        root_->paint(ctx, Offset{view_insets_.left, view_insets_.top});

        if (DebugFlags::showPerformanceOverlay)
            paintPerformanceOverlay(ctx, viewport_width, viewport_height);

        if (DebugFlags::showDebugBanner)
        {
            constexpr float kBannerH  = 24.0f;
            constexpr float kBannerW  = 96.0f;
            constexpr float kPadRight = 0.0f;
            const float     bx        = viewport_width - kBannerW - kPadRight;

            // Red background ribbon.
            ctx.canvas().drawRect(
                Rect::fromLTWH(bx, 0.0f, kBannerW, kBannerH),
                Paint::filled(Color::fromRGBA(0.85f, 0.08f, 0.08f, 0.90f)));

            // "DEBUG" label.
            ctx.canvas().drawText(
                TextSpan{"DEBUG", TextStyle{Color::white(), 11.0f, {}}},
                Offset{bx + 18.0f, 6.0f});
        }

        flushDrawList(ctx.commands(), encoder);
    }

    void Renderer::flushDrawList(
        const DrawList&                  commands,
        campello_gpu::RenderPassEncoder& encoder)
    {
        if (!draw_backend_) return;

        // Replay the command stream, maintaining local transform and clip state
        // to pass the resolved values to the backend.
        Matrix4              current_transform = Matrix4::identity();
        Rect                 current_clip      = Rect::fromLTWH(0, 0, 1e9f, 1e9f);
        std::vector<Matrix4> transform_stack;
        std::vector<Rect>    clip_stack;

        for (const auto& cmd : commands)
        {
            std::visit([&](auto&& c)
            {
                using T = std::decay_t<decltype(c)>;

                if constexpr (std::is_same_v<T, DrawRectCmd>)
                {
                    draw_backend_->drawRect(c, current_transform, current_clip, encoder);
                }
                else if constexpr (std::is_same_v<T, DrawTextCmd>)
                {
                    draw_backend_->drawText(c, current_transform, current_clip, encoder);
                }
                else if constexpr (std::is_same_v<T, DrawImageCmd>)
                {
                    draw_backend_->drawImage(c, current_transform, current_clip, encoder);
                }
                else if constexpr (std::is_same_v<T, DrawCircleCmd>)
                {
                    draw_backend_->drawCircle(c, current_transform, current_clip, encoder);
                }
                else if constexpr (std::is_same_v<T, DrawOvalCmd>)
                {
                    draw_backend_->drawOval(c, current_transform, current_clip, encoder);
                }
                else if constexpr (std::is_same_v<T, DrawRRectCmd>)
                {
                    draw_backend_->drawRRect(c, current_transform, current_clip, encoder);
                }
                else if constexpr (std::is_same_v<T, DrawLineCmd>)
                {
                    draw_backend_->drawLine(c, current_transform, current_clip, encoder);
                }
                else if constexpr (std::is_same_v<T, PushTransformCmd>)
                {
                    transform_stack.push_back(current_transform);
                    current_transform = current_transform * c.transform;
                }
                else if constexpr (std::is_same_v<T, PopTransformCmd>)
                {
                    if (!transform_stack.empty())
                    {
                        current_transform = transform_stack.back();
                        transform_stack.pop_back();
                    }
                }
                else if constexpr (std::is_same_v<T, PushClipRectCmd>)
                {
                    clip_stack.push_back(current_clip);
                    current_clip = c.rect;
                }
                else if constexpr (std::is_same_v<T, PopClipRectCmd>)
                {
                    if (!clip_stack.empty())
                    {
                        current_clip = clip_stack.back();
                        clip_stack.pop_back();
                    }
                }
            }, cmd);
        }
    }

    void Renderer::paintPerformanceOverlay(
        PaintContext& ctx,
        float         /*viewport_width*/,
        float         viewport_height)
    {
        // Layout constants (all in logical pixels).
        constexpr float kOverlayW  = 300.0f;
        constexpr float kPanelH    = 80.0f;   // bar-chart area height
        constexpr float kLabelH    = 20.0f;   // text row height above bars
        constexpr float kTotalH    = kPanelH + kLabelH;
        constexpr float kTargetMs  = 1000.0f / 60.0f;   // ~16.67 ms
        constexpr float kMaxMs     = 3.0f * kTargetMs;  // ~50 ms = full bar height
        constexpr float kBarW      = kOverlayW / static_cast<float>(kPerfSamples);

        const float oy = viewport_height - kTotalH;  // overlay top-left Y

        // --- semi-transparent dark background ---
        ctx.canvas().drawRect(
            Rect::fromLTWH(0.0f, oy, kOverlayW, kTotalH),
            Paint::filled(Color::fromRGBA(0.10f, 0.10f, 0.10f, 0.80f)));

        // --- compute average frame time and FPS ---
        float avg_ms = kTargetMs;
        if (perf_count_ > 0)
        {
            float sum = 0.0f;
            for (int i = 0; i < perf_count_; ++i)
                sum += frame_times_ms_[i];
            avg_ms = sum / static_cast<float>(perf_count_);
        }
        const float fps = (avg_ms > 0.0f) ? (1000.0f / avg_ms) : 0.0f;

        // --- FPS / ms label ---
        char buf[64];
        std::snprintf(buf, sizeof(buf), "FPS: %.0f   frame: %.1f ms", fps, avg_ms);
        ctx.canvas().drawText(
            TextSpan{buf, TextStyle{Color::white(), 11.0f, {}}},
            Offset{6.0f, oy + 4.0f});

        // --- bars (oldest left → newest right, growing upward from bottom) ---
        const float chart_bottom = viewport_height;
        for (int i = 0; i < perf_count_; ++i)
        {
            // Oldest sample first: walk backward from perf_head_.
            const int sample_idx =
                (perf_head_ - perf_count_ + i + kPerfSamples) % kPerfSamples;

            const float ms    = frame_times_ms_[sample_idx];
            const float frac  = std::min(ms / kMaxMs, 1.0f);
            const float bar_h = frac * kPanelH;
            const float bx    = static_cast<float>(i) * kBarW;
            const float by    = chart_bottom - bar_h;

            const Color bar_color = (ms > kTargetMs)
                ? Color::fromRGBA(0.90f, 0.20f, 0.20f, 1.0f)   // red  — over budget
                : Color::fromRGBA(0.20f, 0.75f, 0.20f, 1.0f);  // green — on budget

            ctx.canvas().drawRect(
                Rect::fromLTWH(bx, by, kBarW - 1.0f, bar_h),
                Paint::filled(bar_color));
        }

        // --- threshold guide lines ---
        // 1× target (~16 ms) — green
        {
            const float line_y =
                chart_bottom - (kTargetMs / kMaxMs) * kPanelH;
            ctx.canvas().drawRect(
                Rect::fromLTWH(0.0f, line_y, kOverlayW, 1.0f),
                Paint::filled(Color::fromRGBA(0.20f, 0.90f, 0.20f, 0.90f)));
        }
        // 2× target (~32 ms) — red
        {
            const float line_y =
                chart_bottom - (2.0f * kTargetMs / kMaxMs) * kPanelH;
            ctx.canvas().drawRect(
                Rect::fromLTWH(0.0f, line_y, kOverlayW, 1.0f),
                Paint::filled(Color::fromRGBA(0.90f, 0.20f, 0.20f, 0.90f)));
        }
    }

} // namespace systems::leal::campello_widgets
