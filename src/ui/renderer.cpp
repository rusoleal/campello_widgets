#include <campello_widgets/ui/renderer.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/ui/debug_flags.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/ticker.hpp>
#include <campello_widgets/ui/text_span.hpp>
#include <campello_widgets/ui/text_style.hpp>
#include <campello_widgets/ui/thread_checker.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>

#include <campello_gpu/device.hpp>
#include <campello_gpu/command_encoder.hpp>
#include <campello_gpu/render_pass_encoder.hpp>
#include <campello_gpu/texture.hpp>
#include <campello_gpu/texture_view.hpp>
#include <campello_gpu/constants/texture_type.hpp>
#include <campello_gpu/constants/texture_usage.hpp>
#include <campello_gpu/descriptors/begin_render_pass_descriptor.hpp>

#include <variant>

namespace GPU = systems::leal::campello_gpu;

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
        detail::currentRenderer().store(this, std::memory_order_release);
    }

    Renderer::~Renderer()
    {
        if (detail::currentRenderer().load(std::memory_order_acquire) == this)
            detail::currentRenderer().store(nullptr, std::memory_order_release);
    }

    // ------------------------------------------------------------------
    // Public API
    // ------------------------------------------------------------------

    void Renderer::setDevicePixelRatio(float dpr) noexcept
    {
        // Clamp to reasonable range to avoid division by zero or nonsense values
        if (dpr < 0.1f) dpr = 0.1f;
        if (dpr > 10.0f) dpr = 10.0f;
        
        if (device_pixel_ratio_ != dpr) {
            device_pixel_ratio_ = dpr;
            // Mark root as needing layout since the coordinate system changed
            if (root_) root_->markNeedsLayout();
        }
    }

    bool Renderer::renderFrame(
        std::shared_ptr<campello_gpu::TextureView> target,
        float viewport_width,
        float viewport_height)
    {
        ThreadChecker::instance().assertOnBoundThread("Renderer::renderFrame");
        const auto now_tp = std::chrono::steady_clock::now().time_since_epoch();
        const uint64_t now_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(now_tp).count();

        if (auto* d = PointerDispatcher::activeDispatcher())
            d->tick(now_ms);

        if (auto* ts = TickerScheduler::active())
            ts->tick(now_ms);

        // "Build" phase: widget rebuild, layout, and paint-command recording —
        // mirrors Flutter's UI-thread timing. Bracketed with wall-clock
        // timestamps so the overlay shows actual work cost, not request rate.
        const auto build_start = std::chrono::steady_clock::now();

        Element::buildScope();

        layoutPass(viewport_width, viewport_height);

        if (!root_ || !root_->needsPaint())
            return false;

        // Generate the draw list once (headless — no encoder).
        const DrawList draw_list = generateDrawList(viewport_width, viewport_height);

        const auto build_end = std::chrono::steady_clock::now();
        build_sampler_.recordDuration(
            std::chrono::duration<float, std::milli>(build_end - build_start).count());

        if (draw_list.empty())
            return false;

        // "Raster" phase: GPU command encoding + submission — mirrors
        // Flutter's raster-thread timing. Note this measures CPU-side
        // encode/submit cost, not full async GPU execution time (which would
        // need a completion-handler timestamp).
        const auto raster_start = std::chrono::steady_clock::now();
        const bool print_sub_phases = DebugFlags::printRasterSubPhaseTimings;
        auto       sub_phase_start  = raster_start;
        auto markSubPhase = [&](const char* label)
        {
            if (!print_sub_phases) return;
            const auto now = std::chrono::steady_clock::now();
            std::fprintf(stderr, "  [raster] %-16s %6.3f ms\n", label,
                std::chrono::duration<float, std::milli>(now - sub_phase_start).count());
            sub_phase_start = now;
        };

        auto encoder = device_->createCommandEncoder();
        markSubPhase("create encoder");

        // Store frame-scoped context so flushDrawList can restart render passes.
        frame_encoder_ = encoder.get();
        frame_target_  = target;

        // ------------------------------------------------------------------
        // Pass 1 (optional): backdrop capture
        // Render the full scene to `backdrop_tex_` with backdrop-filter
        // children skipped.  The result is then blurred for use in Pass 2.
        // ------------------------------------------------------------------
        if (has_backdrop_filter_ && draw_backend_)
        {
            const uint32_t tw = static_cast<uint32_t>(viewport_width);
            const uint32_t th = static_cast<uint32_t>(viewport_height);

            // Allocate (or reuse) the backdrop texture.
            if (!backdrop_tex_ || backdrop_tex_w_ != tw || backdrop_tex_h_ != th)
            {
                const GPU::PixelFormat fmt = draw_backend_->offscreenPixelFormat();
                backdrop_tex_ = device_->createTexture(
                    GPU::TextureType::tt2d, fmt, tw, th, 1, 1, 1,
                    static_cast<GPU::TextureUsage>(
                        static_cast<int>(GPU::TextureUsage::renderTarget) |
                        static_cast<int>(GPU::TextureUsage::textureBinding) |
                        static_cast<int>(GPU::TextureUsage::copySrc)));
                backdrop_tex_w_ = tw;
                backdrop_tex_h_ = th;
            }

            // Render scene (backdrop-only mode) into backdrop_tex_.
            auto bd_view = backdrop_tex_->createView(draw_backend_->offscreenPixelFormat());
            GPU::ColorAttachment bd_ca{};
            bd_ca.view          = bd_view;
            bd_ca.loadOp        = GPU::LoadOp::clear;
            bd_ca.storeOp       = GPU::StoreOp::store;
            bd_ca.clearValue[0] = clear_color_.r;
            bd_ca.clearValue[1] = clear_color_.g;
            bd_ca.clearValue[2] = clear_color_.b;
            bd_ca.clearValue[3] = clear_color_.a;

            GPU::BeginRenderPassDescriptor bd_desc{};
            bd_desc.colorAttachments = {bd_ca};

            auto bd_rpe = encoder->beginRenderPass(bd_desc);
            markSubPhase("backdrop begin");
            flushDrawList(draw_list, bd_rpe,
                          viewport_width, viewport_height,
                          /*backdrop_pass=*/true);
            markSubPhase("backdrop flush");
            bd_rpe->end();

            // Blur the captured backdrop.
            blurred_backdrop_tex_ = draw_backend_->blurTexture(
                backdrop_tex_, max_sigma_x_, max_sigma_y_, *encoder);
            markSubPhase("backdrop blur");
        }

        // ------------------------------------------------------------------
        // Pass 2 (main): render to the swapchain target.
        // BackdropFilter regions draw the pre-blurred backdrop_tex_.
        // ShaderMask regions spawn their own offscreen sub-passes.
        // ------------------------------------------------------------------
        GPU::ColorAttachment main_ca{};
        main_ca.view          = target;
        main_ca.loadOp        = GPU::LoadOp::clear;
        main_ca.storeOp       = GPU::StoreOp::store;
        main_ca.clearValue[0] = clear_color_.r;
        main_ca.clearValue[1] = clear_color_.g;
        main_ca.clearValue[2] = clear_color_.b;
        main_ca.clearValue[3] = clear_color_.a;

        GPU::BeginRenderPassDescriptor main_desc{};
        main_desc.colorAttachments = {main_ca};

        auto main_rpe = encoder->beginRenderPass(main_desc);
        markSubPhase("main begin");
        flushDrawList(draw_list, main_rpe,
                      viewport_width, viewport_height,
                      /*backdrop_pass=*/false);
        markSubPhase("main flush");
        main_rpe->end();
        markSubPhase("main end");

        frame_encoder_ = nullptr;
        frame_target_.reset();

        // Schedule swapchain presentation just before our submit() so that
        // nested submit() calls (e.g. from offscreen renderers triggered
        // during the layout pass) cannot steal the drawable.
        if (pending_drawable_) {
            device_->scheduleNextPresent(pending_drawable_);
            pending_drawable_ = nullptr;
        }

        auto cmd_buffer = encoder->finish();
        markSubPhase("finish");
        device_->submit(std::move(cmd_buffer));
        markSubPhase("submit");

        if (print_sub_phases)
            std::fprintf(stderr, "  [raster] %-16s %6.3f ms\n", "TOTAL",
                std::chrono::duration<float, std::milli>(
                    std::chrono::steady_clock::now() - raster_start).count());

        const auto raster_end = std::chrono::steady_clock::now();
        raster_sampler_.recordDuration(
            std::chrono::duration<float, std::milli>(raster_end - raster_start).count());

        return true;
    }

    // ------------------------------------------------------------------
    // Private
    // ------------------------------------------------------------------

    void Renderer::layoutPass(float viewport_width, float viewport_height)
    {
        if (!root_) return;

        // Reset per-frame backdrop tracking before traversal.
        has_backdrop_filter_ = false;
        max_sigma_x_         = 0.0f;
        max_sigma_y_         = 0.0f;

        // Convert physical viewport dimensions to logical pixels for layout.
        // All widget layout operates in logical (device-independent) pixels.
        const float logical_viewport_width  = viewport_width  / device_pixel_ratio_;
        const float logical_viewport_height = viewport_height / device_pixel_ratio_;

        // view_insets_ is already in logical points (set from platform
        // safeAreaInsets which are in points, not physical pixels).
        const EdgeInsets& logical_insets = view_insets_;

        const float safe_width  = logical_viewport_width  - logical_insets.horizontal();
        const float safe_height = logical_viewport_height - logical_insets.vertical();

        const BoxConstraints screen_constraints =
            BoxConstraints::tight(safe_width, safe_height);

        RenderObject::setActiveBackend(draw_backend_.get());
        RenderObject::setActiveDevicePixelRatio(device_pixel_ratio_);
        root_->layout(screen_constraints);
        // Note: We don't clear the backend here because it's needed for text
        // measurement during hit testing (e.g., TextField cursor positioning).
        // The backend pointer remains valid for the lifetime of the Renderer.
    }

    DrawList Renderer::generateDrawList(float viewport_width, float viewport_height)
    {
        if (!root_) return {};

        // Headless PaintContext: collects draw commands without a GPU encoder.
        // The backend must be active during paint so that render objects that call
        // measureText() (e.g. RenderTextField for cursor/selection positioning)
        // get real metrics instead of the fallback estimate.
        PaintContext ctx(viewport_width, viewport_height);
        // Backend and DPR are already set from layoutPass, but we set them again
        // in case layoutPass was skipped.
        RenderObject::setActiveBackend(draw_backend_.get());
        RenderObject::setActiveDevicePixelRatio(device_pixel_ratio_);
        root_->paint(ctx, Offset{view_insets_.left, view_insets_.top});
        // Note: We don't clear the backend here because it's needed for text
        // measurement during hit testing (e.g., TextField cursor positioning).
        // The backend pointer remains valid for the lifetime of the Renderer.

        if (DebugFlags::showPerformanceOverlay)
            paintPerformanceOverlay(ctx, viewport_width, viewport_height);

        if (DebugFlags::showDebugBanner)
        {
            constexpr float kBannerH  = 24.0f;
            constexpr float kBannerW  = 96.0f;
            const float     bx        = viewport_width - kBannerW;

            ctx.canvas().drawRect(
                Rect::fromLTWH(bx, 0.0f, kBannerW, kBannerH),
                Paint::filled(Color::fromRGBA(0.85f, 0.08f, 0.08f, 0.90f)));

            ctx.canvas().drawText(
                TextSpan{"DEBUG", TextStyle{Color::white(), 11.0f, {}}},
                Offset{bx + 18.0f, 6.0f});
        }

        if (DebugFlags::paintPointersEnabled)
        {
            if (auto* pd = PointerDispatcher::activeDispatcher())
            {
                const auto& positions = pd->recentPointerPositions();
                for (const auto& pos : positions)
                {
                    const float radius = 6.0f;
                    ctx.canvas().drawCircle(
                        Offset{pos.x, pos.y}, radius,
                        Paint::filled(Color::fromRGBA(0.90f, 0.20f, 0.20f, 0.60f)));
                }
            }
        }

        return ctx.commands();
    }

    void Renderer::flushDrawList(
        const DrawList&                                    commands,
        std::shared_ptr<campello_gpu::RenderPassEncoder>& rpe,
        float viewport_width,
        float viewport_height,
        bool  backdrop_pass)
    {
        if (!draw_backend_) return;

        // Seed the transform with the DPR scale so all logical draw-command
        // coordinates are converted to physical pixels before the Metal
        // shaders divide by the physical viewport to produce NDC.
        Matrix4 current_transform = Matrix4::identity();
        current_transform.data[0] = device_pixel_ratio_;
        current_transform.data[5] = device_pixel_ratio_;
        Rect                 current_clip      = Rect::fromLTWH(0, 0, 1e9f, 1e9f);
        std::vector<Matrix4> transform_stack;
        std::vector<Rect>    clip_stack;

        // ShaderMask accumulation state.
        bool                  in_shader_mask  = false;
        DrawList              shader_mask_cmds;
        DrawShaderMaskBeginCmd shader_mask_info{Rect{}, LinearGradient{}};

        // BackdropFilter child-skip counter (can nest theoretically).
        int backdrop_skip_depth = 0;

        // Per-draw-command-type timing breakdown (diagnostic only — see
        // DebugFlags::printRasterSubPhaseTimings doc comment).
        const bool print_breakdown = !backdrop_pass && DebugFlags::printRasterSubPhaseTimings;
        struct TypeStats { int count = 0; float ms = 0.0f; };
        TypeStats rect_stats, text_stats, image_stats, circle_stats, oval_stats, rrect_stats, line_stats;
        auto timeDraw = [&](TypeStats& stats, auto&& fn)
        {
            if (!print_breakdown) { fn(); return; }
            const auto t0 = std::chrono::steady_clock::now();
            fn();
            stats.ms += std::chrono::duration<float, std::milli>(
                std::chrono::steady_clock::now() - t0).count();
            ++stats.count;
        };

        for (const auto& cmd : commands)
        {
            std::visit([&](auto&& c)
            {
                using T = std::decay_t<decltype(c)>;

                // ── ShaderMask child accumulation ────────────────────────
                if (in_shader_mask)
                {
                    if constexpr (std::is_same_v<T, DrawShaderMaskEndCmd>)
                    {
                        in_shader_mask = false;
                        applyShaderMask(shader_mask_info, shader_mask_cmds,
                                        rpe, viewport_width, viewport_height,
                                        current_transform, current_clip);
                        shader_mask_cmds.clear();
                    }
                    else
                    {
                        // Accumulate all commands (including nested begin/end).
                        shader_mask_cmds.push_back(c);
                    }
                    return;
                }

                // ── BackdropFilter child skipping (backdrop-capture pass) ─
                if (backdrop_pass)
                {
                    if constexpr (std::is_same_v<T, DrawBackdropFilterBeginCmd>)
                    {
                        ++backdrop_skip_depth;
                        return;
                    }
                    if constexpr (std::is_same_v<T, DrawBackdropFilterEndCmd>)
                    {
                        if (backdrop_skip_depth > 0) --backdrop_skip_depth;
                        return;
                    }
                    if (backdrop_skip_depth > 0) return; // skip children
                }

                // ── Main dispatch ────────────────────────────────────────
                if constexpr (std::is_same_v<T, DrawRectCmd>)
                {
                    timeDraw(rect_stats, [&] { draw_backend_->drawRect(c, current_transform, current_clip, *rpe); });
                }
                else if constexpr (std::is_same_v<T, DrawTextCmd>)
                {
                    timeDraw(text_stats, [&] { draw_backend_->drawText(c, current_transform, current_clip, *rpe); });
                }
                else if constexpr (std::is_same_v<T, DrawImageCmd>)
                {
                    timeDraw(image_stats, [&] { draw_backend_->drawImage(c, current_transform, current_clip, *rpe); });
                }
                else if constexpr (std::is_same_v<T, DrawCircleCmd>)
                {
                    timeDraw(circle_stats, [&] { draw_backend_->drawCircle(c, current_transform, current_clip, *rpe); });
                }
                else if constexpr (std::is_same_v<T, DrawOvalCmd>)
                {
                    timeDraw(oval_stats, [&] { draw_backend_->drawOval(c, current_transform, current_clip, *rpe); });
                }
                else if constexpr (std::is_same_v<T, DrawRRectCmd>)
                {
                    timeDraw(rrect_stats, [&] { draw_backend_->drawRRect(c, current_transform, current_clip, *rpe); });
                }
                else if constexpr (std::is_same_v<T, DrawLineCmd>)
                {
                    timeDraw(line_stats, [&] { draw_backend_->drawLine(c, current_transform, current_clip, *rpe); });
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
                else if constexpr (std::is_same_v<T, DrawBackdropFilterBeginCmd>)
                {
                    // Main pass: draw the pre-blurred backdrop region.
                    if (!backdrop_pass && blurred_backdrop_tex_)
                    {
                        draw_backend_->drawBackdropFilter(
                            c, blurred_backdrop_tex_,
                            current_transform, current_clip, *rpe);
                    }
                    // Children that follow will render on top of the blur.
                }
                else if constexpr (std::is_same_v<T, DrawBackdropFilterEndCmd>)
                {
                    // No-op in the main pass.
                }
                else if constexpr (std::is_same_v<T, DrawShaderMaskBeginCmd>)
                {
                    in_shader_mask  = true;
                    shader_mask_info = c;
                }
                else if constexpr (std::is_same_v<T, DrawShaderMaskEndCmd>)
                {
                    // Mismatched end — ignore.
                }
                // SaveLayerCmd, DrawPathCmd, DrawShadowCmd, etc. are planned
                // for a later phase and silently fall through for now.

            }, cmd);
        }

        if (print_breakdown)
        {
            auto printType = [](const char* label, const TypeStats& s)
            {
                if (s.count == 0) return;
                std::fprintf(stderr, "    %-8s n=%-5d total=%6.3f ms  avg=%6.4f ms\n",
                    label, s.count, s.ms, s.ms / s.count);
            };
            std::fprintf(stderr, "  [flush breakdown]\n");
            printType("rect",   rect_stats);
            printType("text",   text_stats);
            printType("image",  image_stats);
            printType("circle", circle_stats);
            printType("oval",   oval_stats);
            printType("rrect",  rrect_stats);
            printType("line",   line_stats);
        }
    }

    void Renderer::applyShaderMask(
        const DrawShaderMaskBeginCmd&                      cmd,
        const DrawList&                                    child_cmds,
        std::shared_ptr<campello_gpu::RenderPassEncoder>& rpe,
        float /*viewport_width*/,
        float /*viewport_height*/,
        const Matrix4& transform,
        const Rect&    /*clip*/)
    {
        if (!draw_backend_ || !frame_encoder_ || child_cmds.empty()) return;

        // Offscreen texture must be in physical pixels so that the DPR-scaled
        // draw commands (from flushDrawList's initial DPR transform) fill it correctly.
        const uint32_t tw = static_cast<uint32_t>(std::ceil(cmd.bounds.width  * device_pixel_ratio_));
        const uint32_t th = static_cast<uint32_t>(std::ceil(cmd.bounds.height * device_pixel_ratio_));
        if (tw == 0 || th == 0) return;

        auto child_tex = draw_backend_->createOffscreenTexture(tw, th);
        if (!child_tex) return;

        // End the current main render pass.
        rpe->end();

        // Render the ShaderMask's children into child_tex.
        auto child_rpe = draw_backend_->beginOffscreenPass(child_tex, *frame_encoder_);
        if (child_rpe)
        {
            // Translate child commands so they paint at (0,0) in the offscreen tex.
            // The translation is in logical pixels; flushDrawList's DPR initial
            // transform scales it to physical pixels automatically.
            Matrix4 offset_mat = Matrix4::identity();
            offset_mat.data[12] = -cmd.bounds.x;
            offset_mat.data[13] = -cmd.bounds.y;

            DrawList translated;
            translated.push_back(PushTransformCmd{offset_mat});
            for (const auto& cc : child_cmds) translated.push_back(cc);
            translated.push_back(PopTransformCmd{});

            flushDrawList(translated, child_rpe,
                          static_cast<float>(tw), static_cast<float>(th),
                          /*backdrop_pass=*/false);
            child_rpe->end();
        }

        // Restart the main render pass preserving existing content.
        rpe = restartMainRenderPass();

        // Composite child_tex × shader mask → main pass.
        // Pass the DPR transform so the compositor places the result in physical pixels.
        if (child_rpe)
        {
            draw_backend_->drawShaderMaskComposite(
                child_tex, cmd, transform, *rpe);
        }
    }

    std::shared_ptr<campello_gpu::RenderPassEncoder> Renderer::restartMainRenderPass()
    {
        if (!frame_encoder_ || !frame_target_) return nullptr;

        GPU::ColorAttachment ca{};
        ca.view    = frame_target_;
        ca.loadOp  = GPU::LoadOp::load;    // preserve what was drawn before
        ca.storeOp = GPU::StoreOp::store;

        GPU::BeginRenderPassDescriptor desc{};
        desc.colorAttachments = {ca};

        return frame_encoder_->beginRenderPass(desc);
    }

    // ------------------------------------------------------------------
    // Performance overlay — Flutter-style two-lane timing chart.
    //
    // Each lane shows the *measured duration* of one frame phase (raster
    // encode/submit on top, UI build+layout+paint on bottom), not how often
    // a frame was requested — see the build_sampler_/raster_sampler_
    // recordDuration() calls in renderFrame() for where these are bracketed.
    // ------------------------------------------------------------------

    void Renderer::paintTimingLane(
        PaintContext&                         ctx,
        const campello_gpu::FrameTimeSampler& sampler,
        const char*                           label,
        float                                 lane_top,
        float                                 lane_w,
        float                                 panel_h,
        float                                 label_h,
        float                                 target_ms,
        float                                 max_ms,
        float                                 bar_w)
    {
        const int   n      = sampler.count();
        const float avg_ms = (n > 0) ? sampler.averageMs() : 0.0f;

        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s: %.1f ms", label, avg_ms);
        ctx.canvas().drawText(
            TextSpan{buf, TextStyle{Color::white(), 11.0f, {}}},
            Offset{6.0f, lane_top + 4.0f});

        const float chart_top    = lane_top + label_h;
        const float chart_bottom = chart_top + panel_h;

        for (int i = 0; i < n; ++i)
        {
            const float ms    = sampler.at(i);
            const float frac  = std::min(ms / max_ms, 1.0f);
            const float bar_h = frac * panel_h;
            const float bx    = static_cast<float>(i) * bar_w;
            const float by    = chart_bottom - bar_h;

            const Color bar_color = (ms > target_ms)
                ? Color::fromRGBA(0.90f, 0.20f, 0.20f, 1.0f)
                : Color::fromRGBA(0.20f, 0.75f, 0.20f, 1.0f);

            ctx.canvas().drawRect(
                Rect::fromLTWH(bx, by, bar_w - 1.0f, bar_h),
                Paint::filled(bar_color));
        }

        {
            const float line_y = chart_bottom - (target_ms / max_ms) * panel_h;
            ctx.canvas().drawRect(
                Rect::fromLTWH(0.0f, line_y, lane_w, 1.0f),
                Paint::filled(Color::fromRGBA(0.20f, 0.90f, 0.20f, 0.90f)));
        }
        {
            const float line_y = chart_bottom - (2.0f * target_ms / max_ms) * panel_h;
            ctx.canvas().drawRect(
                Rect::fromLTWH(0.0f, line_y, lane_w, 1.0f),
                Paint::filled(Color::fromRGBA(0.90f, 0.20f, 0.20f, 0.90f)));
        }
    }

    void Renderer::paintPerformanceOverlay(
        PaintContext& ctx,
        float         /*viewport_width*/,
        float         viewport_height)
    {
        constexpr float kOverlayW  = 300.0f;
        constexpr float kPanelH    = 70.0f;
        constexpr float kLabelH    = 16.0f;
        constexpr float kLaneH     = kPanelH + kLabelH;
        constexpr float kTotalH    = 2.0f * kLaneH;
        constexpr float kTargetMs  = 1000.0f / 60.0f;
        constexpr float kMaxMs     = 3.0f * kTargetMs;
        constexpr float kBarW      = kOverlayW / static_cast<float>(campello_gpu::FrameTimeSampler::kCapacity);

        const float oy = viewport_height - kTotalH;

        ctx.canvas().drawRect(
            Rect::fromLTWH(0.0f, oy, kOverlayW, kTotalH),
            Paint::filled(Color::fromRGBA(0.10f, 0.10f, 0.10f, 0.80f)));

        // Top lane: raster (GPU encode/submit) — matches Flutter's top "GPU" graph.
        paintTimingLane(ctx, raster_sampler_, "RASTER", oy,
                        kOverlayW, kPanelH, kLabelH, kTargetMs, kMaxMs, kBarW);

        // Bottom lane: UI (build + layout + paint recording).
        paintTimingLane(ctx, build_sampler_, "UI", oy + kLaneH,
                        kOverlayW, kPanelH, kLabelH, kTargetMs, kMaxMs, kBarW);

        {
            const float divider_y = oy + kLaneH;
            ctx.canvas().drawRect(
                Rect::fromLTWH(0.0f, divider_y, kOverlayW, 1.0f),
                Paint::filled(Color::fromRGBA(0.35f, 0.35f, 0.35f, 0.80f)));
        }
    }

} // namespace systems::leal::campello_widgets
