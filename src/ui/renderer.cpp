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
#include <vector_math/vector3.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <utility>

#include <campello_gpu/device.hpp>
#include <campello_gpu/command_encoder.hpp>
#include <campello_gpu/render_pass_encoder.hpp>
#include <campello_gpu/texture.hpp>
#include <campello_gpu/texture_view.hpp>
#include <campello_gpu/constants/texture_type.hpp>
#include <campello_gpu/constants/texture_usage.hpp>
#include <campello_gpu/descriptors/begin_render_pass_descriptor.hpp>

#include <atomic>
#include <variant>

namespace GPU = systems::leal::campello_gpu;

namespace systems::leal::campello_widgets
{
    namespace
    {
        // Mirrors the Gaussian kernel radius the blur shader actually uses
        // (shaders/metal/widgets.metal: RADIUS = min(ceil(2.5 * sigma), 12))
        // so the dirty-region margin matches how far blur sampling actually
        // reaches, on each axis, beyond a BackdropFilter's exact footprint.
        constexpr float kBlurMarginFactor = 2.5f;
        constexpr float kBlurMarginCap    = 12.0f;
    }

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

    void Renderer::setDisplayRefreshHz(float hz) noexcept
    {
        if (hz < 1.0f) hz = 1.0f;
        if (hz > 1000.0f) hz = 1000.0f;
        display_refresh_hz_ = hz;
    }

    std::optional<FramePackage> Renderer::buildFrame(float viewport_width, float viewport_height)
    {
        ThreadChecker::instance().assertOnBoundThread("Renderer::buildFrame");
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
        const bool print_sub_phases = DebugFlags::printRasterSubPhaseTimings;
        auto       sub_phase_start  = build_start;
        auto markBuildSubPhase = [&](const char* label)
        {
            if (!print_sub_phases) return;
            const auto now = std::chrono::steady_clock::now();
            std::fprintf(stderr, "  [build]  %-16s %6.3f ms\n", label,
                std::chrono::duration<float, std::milli>(now - sub_phase_start).count());
            sub_phase_start = now;
        };

        Element::buildScope();
        markBuildSubPhase("buildScope");

        layoutPass(viewport_width, viewport_height);
        markBuildSubPhase("layoutPass");

        // Consume the paint-requested latch instead of checking
        // root_->needsPaint(): markNeedsPaint() now stops propagating once
        // it reaches a repaint boundary (see RenderObject::
        // isRepaintBoundary()'s doc), so a dirty leaf under a boundary
        // never reaches root — root's own flag can no longer answer "does
        // anything need painting." See notePaintRequested()'s doc.
        if (!root_ || !std::exchange(paint_requested_, false)) {
            if (print_sub_phases)
                std::fprintf(stderr, "[buildFrame] skip: paint_not_requested\n");
            return std::nullopt;
        }

        // Generate the draw list once (headless — no encoder). This also
        // paints the performance overlay, which reads raster_sampler_ —
        // guard with sampler_mutex_ since rasterFrame() may be writing it
        // concurrently on a separate raster thread.
        DrawList draw_list;
        {
            std::lock_guard<std::mutex> lock(sampler_mutex_);
            draw_list = generateDrawList(viewport_width, viewport_height);
        }
        markBuildSubPhase("generateDrawList");

        const auto build_end = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> lock(sampler_mutex_);
            build_sampler_.recordDuration(
                std::chrono::duration<float, std::milli>(build_end - build_start).count());
        }

        if (draw_list.empty()) {
            if (print_sub_phases)
                std::fprintf(stderr, "[buildFrame] skip: draw_list empty\n");
            return std::nullopt;
        }
        if (print_sub_phases)
            std::fprintf(stderr, "[buildFrame] submitting draw_list size=%zu\n", draw_list.size());

        // Snapshot everything the raster phase needs into an immutable
        // package — the raster phase must never read these Renderer
        // members live, since buildFrame() may be called again (for the
        // next frame) while rasterFrame() is still processing this one.
        //
        // has_backdrop_filter_ tells us at least one BackdropFilter painted
        // this frame, but the capture pass it gates is only worth its cost
        // if content within blur range of one of those filters actually
        // changed — evaluated now, after the full paint walk, since a
        // filter can sample content painted anywhere in the frame,
        // including subtrees walked after it (see anyRegionDirty()'s doc).
        const float margin_x = std::min(max_sigma_x_ * kBlurMarginFactor, kBlurMarginCap);
        const float margin_y = std::min(max_sigma_y_ * kBlurMarginFactor, kBlurMarginCap);
        const bool  needs_backdrop_capture = has_backdrop_filter_ &&
            anyRegionDirty(backdrop_regions_, margin_x, margin_y,
                           dirty_rects_, dirty_region_overflowed_);

        if (DebugFlags::printDirtyRegionTrace)
        {
            static uint64_t frame_no = 0;
            std::fprintf(stderr,
                "[dirty] frame %llu: dirty_rects=%zu overflowed=%d backdrop_regions=%zu "
                "margin=(%.1f,%.1f) has_backdrop_filter=%d -> needs_capture=%d\n",
                static_cast<unsigned long long>(frame_no++), dirty_rects_.size(),
                dirty_region_overflowed_ ? 1 : 0, backdrop_regions_.size(),
                margin_x, margin_y, has_backdrop_filter_ ? 1 : 0,
                needs_backdrop_capture ? 1 : 0);
            for (const Rect& r : backdrop_regions_)
                std::fprintf(stderr, "[dirty]   backdrop region (%.0f,%.0f %.0fx%.0f)\n",
                             r.x, r.y, r.width, r.height);
        }

        FramePackage package;
        package.draw_list           = std::move(draw_list);
        package.viewport_width      = viewport_width;
        package.viewport_height     = viewport_height;
        package.device_pixel_ratio  = device_pixel_ratio_;
        package.clear_color         = clear_color_;
        package.has_backdrop_filter = needs_backdrop_capture;
        package.max_sigma_x         = max_sigma_x_;
        package.max_sigma_y         = max_sigma_y_;

        // Consume-and-clear the pending drawable, mirroring the old
        // single-threaded contract exactly. Wrapped in a non-owning
        // shared_ptr<void> so back-compat callers (setPendingDrawable() +
        // renderFrame(), used by iOS/Android/Windows/Linux today) need no
        // changes; platforms adopting a real raster thread should instead
        // assign package.retained_drawable directly with an owning deleter
        // after buildFrame() returns (see RasterThread-based platforms).
        if (pending_drawable_) {
            package.retained_drawable = std::shared_ptr<void>(pending_drawable_, [](void*) {});
            pending_drawable_ = nullptr;
        }

        return package;
    }

    bool Renderer::rasterFrame(const FramePackage& package)
    {
        if (ThreadChecker::rasterInstance().hasBinding())
            ThreadChecker::rasterInstance().assertOnBoundThread("Renderer::rasterFrame");

        // target may be nullptr on Vulkan — that signals "render to swapchain".
        // campello_gpu's beginRenderPass acquires the swapchain image automatically
        // when view == nullptr, so we do not guard against null here.
        std::shared_ptr<campello_gpu::TextureView> target = package.target;

        ++frame_counter_;
        evictStaleGpuCaches();

        const float dpr = package.device_pixel_ratio;
        if (draw_backend_) {
            draw_backend_->setViewport(package.viewport_width, package.viewport_height);
            draw_backend_->setDevicePixelRatio(dpr);
        }

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
        if (!encoder) { std::fprintf(stderr, "[raster] createCommandEncoder returned null\n"); return false; }

        // Store frame-scoped context so flushDrawList can restart render passes.
        frame_encoder_ = encoder.get();
        frame_target_  = target;

        // Age out frame_bd_view_ from two generations ago — see its doc
        // comment in renderer.hpp for why this can no longer just be a
        // rasterFrame()-local variable. Unconditional (not just when this
        // frame has a backdrop filter) so a stretch of filter-less frames
        // still ages the retained views out instead of holding them forever.
        prev2_frame_bd_view_.reset();
        prev2_frame_bd_view_ = std::move(prev_frame_bd_view_);
        prev_frame_bd_view_  = std::move(frame_bd_view_);

        // ------------------------------------------------------------------
        // Pass 1 (optional): backdrop capture
        // Render the full scene to `backdrop_tex_` with backdrop-filter
        // children skipped.  The result is then blurred for use in Pass 2.
        // ------------------------------------------------------------------
        // frame_bd_view_ (declared in renderer.hpp) stays alive across the
        // two generations after this frame — see its doc comment there. A
        // rasterFrame()-local variable was only safe while
        // Device::submit() blocked until the GPU finished before returning.
        if (package.has_backdrop_filter && draw_backend_)
        {
            const uint32_t tw = static_cast<uint32_t>(package.viewport_width);
            const uint32_t th = static_cast<uint32_t>(package.viewport_height);

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
            // arrayLayerCount = 1 (non-array 2D texture) — the default (-1,
            // an unsigned sentinel) produces an out-of-bounds slice range
            // that Metal's argument validation rejects.
            frame_bd_view_ = backdrop_tex_->createView(draw_backend_->offscreenPixelFormat(), 1);
            GPU::ColorAttachment bd_ca{};
            bd_ca.view          = frame_bd_view_;
            bd_ca.loadOp        = GPU::LoadOp::clear;
            bd_ca.storeOp       = GPU::StoreOp::store;
            bd_ca.clearValue[0] = package.clear_color.r;
            bd_ca.clearValue[1] = package.clear_color.g;
            bd_ca.clearValue[2] = package.clear_color.b;
            bd_ca.clearValue[3] = package.clear_color.a;

            GPU::BeginRenderPassDescriptor bd_desc{};
            bd_desc.colorAttachments = {bd_ca};

            auto bd_rpe = encoder->beginRenderPass(bd_desc);
            markSubPhase("backdrop begin");
            flushDrawList(package.draw_list, bd_rpe, frame_bd_view_,
                          package.viewport_width, package.viewport_height, dpr,
                          /*backdrop_pass=*/true);
            markSubPhase("backdrop flush");
            bd_rpe->end();

            // Blur the captured backdrop.
            blurred_backdrop_tex_ = draw_backend_->blurTexture(
                backdrop_tex_, package.max_sigma_x, package.max_sigma_y, *encoder);
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
        main_ca.clearValue[0] = package.clear_color.r;
        main_ca.clearValue[1] = package.clear_color.g;
        main_ca.clearValue[2] = package.clear_color.b;
        main_ca.clearValue[3] = package.clear_color.a;

        GPU::BeginRenderPassDescriptor main_desc{};
        main_desc.colorAttachments = {main_ca};

        auto main_rpe = encoder->beginRenderPass(main_desc);
        markSubPhase("main begin");

        if (!main_rpe) {
            std::fprintf(stderr, "[raster] beginRenderPass returned null\n");
            frame_encoder_ = nullptr;
            frame_target_.reset();
            // createCommandEncoder() already advanced the frames-in-flight
            // ring and reset this generation's fence before we ever got
            // here (see DeviceData::beginFrameRing()). That fence can only
            // be signaled by a submission referencing it — discarding the
            // encoder without submitting leaves it permanently unsignaled,
            // and every future frame that cycles back to this same ring
            // slot (or, if a swapchain image was acquired, this same image
            // index) then hangs forever waiting on it. Submit the
            // (possibly empty) command buffer so the ring stays healthy;
            // this drops one frame instead of freezing the app.
            auto cmd_buffer = encoder->finish();
            if (cmd_buffer) {
                device_->submit(std::move(cmd_buffer));
            }
            return false;
        }

        flushDrawList(package.draw_list, main_rpe, target,
                      package.viewport_width, package.viewport_height, dpr,
                      /*backdrop_pass=*/false);
        markSubPhase("main flush");
        main_rpe->end();
        markSubPhase("main end");

        frame_encoder_ = nullptr;
        frame_target_.reset();

        // Schedule swapchain presentation just before our submit() so that
        // nested submit() calls (e.g. from offscreen renderers triggered
        // during the layout pass) cannot steal the drawable.
        if (package.retained_drawable) {
            device_->scheduleNextPresent(package.retained_drawable.get());
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
        {
            std::lock_guard<std::mutex> lock(sampler_mutex_);
            raster_sampler_.recordDuration(
                std::chrono::duration<float, std::milli>(raster_end - raster_start).count());

            const uint64_t present_now_ms = std::chrono::duration_cast<
                std::chrono::milliseconds>(raster_end.time_since_epoch()).count();
            recordPresentSample(present_fps_sampler_, last_present_ms_, present_now_ms);
        }

        return true;
    }

    void Renderer::recordPresentSample(
        campello_gpu::FrameTimeSampler& sampler,
        uint64_t&                       last_ms,
        uint64_t                        now_ms,
        uint64_t                        idle_gap_reset_ms)
    {
        if (last_ms != 0 && now_ms - last_ms > idle_gap_reset_ms)
            sampler.reset();
        sampler.record(now_ms);
        last_ms = now_ms;
    }

    bool Renderer::renderFrame(
        std::shared_ptr<campello_gpu::TextureView> target,
        float viewport_width,
        float viewport_height)
    {
        auto package = buildFrame(viewport_width, viewport_height);
        if (!package) return false;
        package->target = std::move(target);
        return rasterFrame(*package);
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
        backdrop_regions_.clear();

        // Reset per-frame dirty-region tracking before traversal.
        dirty_rects_.clear();
        dirty_region_overflowed_ = false;

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
        const Offset paint_origin{view_insets_.left, view_insets_.top};
        RenderObject::setActivePaintOriginOffset(paint_origin);
        root_->paint(ctx, paint_origin);
        // Note: We don't clear the backend here because it's needed for text
        // measurement during hit testing (e.g., TextField cursor positioning).
        // The backend pointer remains valid for the lifetime of the Renderer.

        // Debug overlays below position themselves using plain arithmetic
        // against the viewport size (e.g. "bottom-left corner"), so — unlike
        // the widget tree above, which only ever consumes paint_origin plus
        // its own already-logical layout results — they need genuinely
        // logical dimensions here, matching layoutPass()'s identical
        // conversion. viewport_width/height are physical pixels (the
        // caller passes drawableSize/physical_width straight through on
        // every platform); using them unconverted put the overlay several
        // times its own height below the visible logical area — invisible
        // once the dpr-scale transform pushed it further off-screen still.
        const float logical_viewport_width  = viewport_width  / device_pixel_ratio_;
        const float logical_viewport_height = viewport_height / device_pixel_ratio_;

        if (DebugFlags::showPerformanceOverlay)
            paintPerformanceOverlay(ctx, logical_viewport_width, logical_viewport_height);

        if (DebugFlags::showDebugBanner)
        {
            constexpr float kBannerH  = 24.0f;
            constexpr float kBannerW  = 96.0f;
            const float     bx        = logical_viewport_width - kBannerW;

            ctx.canvas().drawRect(
                Rect::fromLTWH(bx, 0.0f, kBannerW, kBannerH),
                Paint::filled(Color::fromRGBA(0.85f, 0.08f, 0.08f, 0.90f)));

            TextStyle debug_style{Color::white(), 11.0f, {}};
            debug_style.font_size *= device_pixel_ratio_;
            ctx.canvas().drawText(
                TextSpan{"DEBUG", debug_style},
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
        std::shared_ptr<campello_gpu::TextureView>         target_view,
        float viewport_width,
        float viewport_height,
        float dpr,
        bool  backdrop_pass)
    {
        if (!draw_backend_) return;

        // Notify the backend that a new render pass is starting so it can
        // reset any GPU state caches (e.g. scissor) that beginRenderPass()
        // invalidated. Called once per flushDrawList() invocation, which
        // corresponds to one beginRenderPass() call at the enclosing level.
        draw_backend_->onBeginFlush();

        // Seed the transform with the DPR scale so all logical draw-command
        // coordinates are converted to physical pixels before the Metal
        // shaders divide by the physical viewport to produce NDC. `dpr` is
        // read from the caller's FramePackage, not the device_pixel_ratio_
        // member — this may run on the raster thread while the UI thread
        // has already moved on to building the next frame.
        Matrix4 current_transform = Matrix4::identity();
        current_transform.data[0] = dpr;
        current_transform.data[5] = dpr;
        Rect                 current_clip      = Rect::fromLTWH(0, 0, viewport_width, viewport_height);
        std::vector<Matrix4> transform_stack;
        std::vector<Rect>    clip_stack;

        // ShaderMask accumulation state.
        bool                  in_shader_mask  = false;
        DrawList              shader_mask_cmds;
        DrawShaderMaskBeginCmd shader_mask_info{Rect{}, LinearGradient{}};

        // ClipRRect/ClipOval accumulation state. Unlike ShaderMask, clip
        // pushes share a single generic PopClipRectCmd with plain
        // PushClipRectCmd, so the matching pop is found by tracking the
        // nesting depth of every clip push/pop encountered while
        // accumulating (rather than a dedicated end command).
        bool     in_clip_shape    = false;
        int      clip_shape_depth = 0;
        DrawList clip_shape_cmds;
        Rect     clip_shape_bounds;
        float    clip_shape_corner_r = 0.0f;
        bool     clip_shape_is_oval  = false;

        // BackdropFilter child-skip counter (can nest theoretically).
        int backdrop_skip_depth = 0;

        // Stack of active CacheReplayBeginCmd/EndCmd regions — see
        // CacheReplayBeginCmd's doc comment and clip_shape_gpu_cache_'s.
        // .first = region_id, .second = next bracket index to assign.
        // A fresh stack local to this flushDrawList() call correctly
        // handles nested replay regions: the recursive flushDrawList()
        // calls inside applyClipShape()/applyShaderMask() (for the
        // offscreen child-content pass) each get their own local stack, so
        // a CacheReplayBeginCmd/EndCmd pair nested inside an accumulating
        // clip-shape/shader-mask bracket's child_cmds — preserved verbatim
        // by the accumulation branches below, which never special-case
        // these markers — is only ever interpreted by the recursive call
        // that actually re-walks that nested content.
        std::vector<std::pair<const void*, size_t>> replay_region_stack;

        // Per-draw-command-type timing breakdown (diagnostic only — see
        // DebugFlags::printRasterSubPhaseTimings doc comment).
        const bool print_breakdown = !backdrop_pass && DebugFlags::printRasterSubPhaseTimings;
        struct TypeStats { int count = 0; float ms = 0.0f; };
        TypeStats rect_stats, text_stats, image_stats, circle_stats, oval_stats, rrect_stats, line_stats;
        TypeStats backdrop_stats; // BackdropFilter's main-pass composite draw — previously untimed.
        // applyClipShape/applyShaderMask/applyBoxShadow run their own
        // offscreen render passes and were previously entirely outside
        // timeDraw()'s accounting, leaving a large unexplained gap between
        // the itemized categories above and the reported main-flush total.
        TypeStats clip_shape_stats, shader_mask_stats, shadow_stats;
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
                        const void* rid  = nullptr;
                        size_t      ridx = 0;
                        if (!replay_region_stack.empty())
                        {
                            rid  = replay_region_stack.back().first;
                            ridx = replay_region_stack.back().second++;
                        }
                        timeDraw(shader_mask_stats, [&] {
                            applyShaderMask(shader_mask_info, shader_mask_cmds,
                                            rpe, target_view, viewport_width, viewport_height, dpr,
                                            current_transform, current_clip, rid, ridx);
                        });
                        shader_mask_cmds.clear();
                    }
                    else
                    {
                        // Accumulate all commands (including nested begin/end).
                        shader_mask_cmds.push_back(c);
                    }
                    return;
                }

                // ── ClipRRect/ClipOval child accumulation ────────────────
                if (in_clip_shape)
                {
                    if constexpr (std::is_same_v<T, PushClipRectCmd>   ||
                                  std::is_same_v<T, PushClipRRectCmd>  ||
                                  std::is_same_v<T, PushClipOvalCmd>   ||
                                  std::is_same_v<T, PushClipPathCmd>)
                    {
                        ++clip_shape_depth;
                        clip_shape_cmds.push_back(c);
                    }
                    else if constexpr (std::is_same_v<T, PopClipRectCmd>)
                    {
                        --clip_shape_depth;
                        if (clip_shape_depth == 0)
                        {
                            in_clip_shape = false;
                            const void* rid  = nullptr;
                            size_t      ridx = 0;
                            if (!replay_region_stack.empty())
                            {
                                rid  = replay_region_stack.back().first;
                                ridx = replay_region_stack.back().second++;
                            }
                            timeDraw(clip_shape_stats, [&] {
                                applyClipShape(clip_shape_bounds, clip_shape_corner_r,
                                               clip_shape_is_oval, clip_shape_cmds,
                                               rpe, target_view, viewport_width, viewport_height, dpr,
                                               current_transform, current_clip, rid, ridx);
                            });
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
                    timeDraw(text_stats, [&] { drawCachedText(c, current_transform, current_clip, rpe, dpr); });
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
                else if constexpr (std::is_same_v<T, PushClipPathCmd>)
                {
                    // No general path-shaped clip yet — fall back to the
                    // path's bounding box (better than no clip at all).
                    clip_stack.push_back(current_clip);
                    current_clip = current_clip.intersection(c.path.getBounds());
                }
                else if constexpr (std::is_same_v<T, PushClipRRectCmd> ||
                                    std::is_same_v<T, PushClipOvalCmd>)
                {
                    // Begin accumulating this clip scope's commands so they
                    // can be rendered offscreen and composited through an
                    // SDF rounded-rect/ellipse mask — see applyClipShape().
                    in_clip_shape    = true;
                    clip_shape_depth = 1;
                    clip_shape_cmds.clear();
                    if constexpr (std::is_same_v<T, PushClipRRectCmd>)
                    {
                        clip_shape_bounds   = c.rrect.rect;
                        clip_shape_corner_r = c.rrect.radius_x;
                        clip_shape_is_oval  = false;
                    }
                    else
                    {
                        clip_shape_bounds   = c.rect;
                        clip_shape_corner_r = 0.0f;
                        clip_shape_is_oval  = true;
                    }
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
                        timeDraw(backdrop_stats, [&] {
                            draw_backend_->drawBackdropFilter(
                                c, blurred_backdrop_tex_,
                                current_transform, current_clip, *rpe);
                        });
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
                else if constexpr (std::is_same_v<T, CacheReplayBeginCmd>)
                {
                    replay_region_stack.push_back({c.region_id, size_t{0}});
                }
                else if constexpr (std::is_same_v<T, CacheReplayEndCmd>)
                {
                    if (!replay_region_stack.empty())
                        replay_region_stack.pop_back();
                }
                else if constexpr (std::is_same_v<T, DrawShadowCmd>)
                {
                    const void* rid  = nullptr;
                    size_t      ridx = 0;
                    if (!replay_region_stack.empty())
                    {
                        rid  = replay_region_stack.back().first;
                        ridx = replay_region_stack.back().second++;
                    }
                    timeDraw(shadow_stats, [&] {
                        applyBoxShadow(c, rpe, target_view, viewport_width, viewport_height,
                                       dpr, current_transform, current_clip, rid, ridx);
                    });
                }

                // SaveLayerCmd, DrawPathCmd, etc. are planned for a later
                // phase and silently fall through for now.

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
            printType("backdrop", backdrop_stats);
            printType("clipshape", clip_shape_stats);
            printType("shadermask", shader_mask_stats);
            printType("shadow", shadow_stats);
        }
    }

    void Renderer::applyShaderMask(
        const DrawShaderMaskBeginCmd&                      cmd,
        const DrawList&                                    child_cmds,
        std::shared_ptr<campello_gpu::RenderPassEncoder>& rpe,
        std::shared_ptr<campello_gpu::TextureView>         target_view,
        float viewport_width,
        float viewport_height,
        float dpr,
        const Matrix4& transform,
        const Rect&    clip,
        const void*    replay_region_id,
        size_t         replay_bracket_index)
    {
        if (!draw_backend_ || !frame_encoder_ || child_cmds.empty()) return;

        // Cache-hit path: this exact bracket was already composited on a
        // previous frame this same replay region was seen, and the region
        // being replayed at all guarantees its content is byte-for-byte
        // unchanged — see shader_mask_gpu_cache_'s doc comment. Reuse the
        // cached texture directly, skipping the capture-and-composite work
        // below entirely.
        const std::pair<const void*, size_t> cache_key{replay_region_id, replay_bracket_index};
        if (replay_region_id != nullptr)
        {
            auto it = shader_mask_gpu_cache_.find(cache_key);
            if (it != shader_mask_gpu_cache_.end())
            {
                it->second.last_used_frame = frame_counter_;
                draw_backend_->drawShaderMaskComposite(
                    it->second.texture, it->second.cmd, transform, clip, *rpe);
                return;
            }
        }

        // Offscreen texture must be in physical pixels so that the DPR-scaled
        // draw commands (from flushDrawList's initial DPR transform) fill it correctly.
        const uint32_t tw = static_cast<uint32_t>(std::ceil(cmd.bounds.width  * dpr));
        const uint32_t th = static_cast<uint32_t>(std::ceil(cmd.bounds.height * dpr));
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
            // transform scales it to physical pixels automatically. Matrix4's
            // translation components live at data[3]/data[7]/data[11] (last
            // column of rows 0-2) — data[12]/data[13] are part of row 3 (the
            // W-component coefficients), not X/Y translation.
            Matrix4 offset_mat = Matrix4::translate(
                vector_math::Vector3<float>(-cmd.bounds.x, -cmd.bounds.y, 0.0f));

            DrawList translated;
            translated.push_back(PushTransformCmd{offset_mat});
            for (const auto& cc : child_cmds) translated.push_back(cc);
            translated.push_back(PopTransformCmd{});

            // See applyClipShape() for why the backend viewport must track
            // the offscreen texture's own resolution during this sub-pass —
            // and why this must be setViewportSize(), not setViewport(): the
            // latter also cycles the uniform-buffer pools' per-frame ring
            // generation, which must only advance once per real frame. Doing
            // it here (mid-frame, possibly several times for several
            // composites) wraps the ring around while earlier draws in this
            // same not-yet-submitted command buffer still reference pooled
            // buffers by index, so a later draw's upload() can silently
            // overwrite an earlier draw's uniforms before the GPU ever reads
            // them — corrupting unrelated draws recorded earlier in the pass.
            draw_backend_->setViewportSize(static_cast<float>(tw), static_cast<float>(th));

            // A view of child_tex itself, in case a nested ShaderMask/
            // ClipRRect/ClipOval inside this one needs to restart against
            // it (see flushDrawList()'s target_view contract).
            auto child_view = child_tex->createView(draw_backend_->offscreenPixelFormat(), 1);
            flushDrawList(translated, child_rpe, child_view,
                          static_cast<float>(tw), static_cast<float>(th), dpr,
                          /*backdrop_pass=*/false);
            child_rpe->end();

            draw_backend_->setViewportSize(viewport_width, viewport_height);
        }

        // Restart whatever render pass was active before this composite.
        rpe = restartRenderPass(target_view);

        // Composite child_tex × shader mask → main pass.
        // Pass the DPR transform so the compositor places the result in physical pixels.
        if (child_rpe)
        {
            draw_backend_->drawShaderMaskComposite(
                child_tex, cmd, transform, clip, *rpe);

            // Populate the cache so a *future* identity-replay of this same
            // region can reuse this composite instead of recapturing it —
            // see shader_mask_gpu_cache_'s doc comment. Only meaningful
            // when we're actually inside a replay region: outside one,
            // region_id is nullptr and there is no stable key to store
            // under (a fresh, non-replayed record has no guarantee its
            // bracket sequence will repeat next frame the way a replay's
            // does).
            if (replay_region_id != nullptr)
            {
                ShaderMaskGpuCacheEntry entry;
                entry.texture         = child_tex;
                entry.cmd             = cmd;
                entry.last_used_frame = frame_counter_;
                shader_mask_gpu_cache_[cache_key] = std::move(entry);
            }
        }
    }

    void Renderer::applyClipShape(
        const Rect&                                        bounds,
        float                                               corner_radius,
        bool                                                is_oval,
        const DrawList&                                    child_cmds,
        std::shared_ptr<campello_gpu::RenderPassEncoder>& rpe,
        std::shared_ptr<campello_gpu::TextureView>         target_view,
        float viewport_width,
        float viewport_height,
        float dpr,
        const Matrix4& transform,
        const Rect&    clip,
        const void*    replay_region_id,
        size_t         replay_bracket_index)
    {
        if (!draw_backend_ || !frame_encoder_ || child_cmds.empty()) return;

        // Cache-hit path — see applyShaderMask()'s identical comment and
        // clip_shape_gpu_cache_'s doc comment.
        const std::pair<const void*, size_t> cache_key{replay_region_id, replay_bracket_index};
        if (replay_region_id != nullptr)
        {
            auto it = clip_shape_gpu_cache_.find(cache_key);
            if (it != clip_shape_gpu_cache_.end())
            {
                it->second.last_used_frame = frame_counter_;
                draw_backend_->drawClipShapeComposite(
                    it->second.texture, it->second.bounds, it->second.corner_radius,
                    it->second.is_oval, transform, clip, *rpe);
                return;
            }
        }

        // A NaN/Infinite bounds (seen in practice from a degenerate layout
        // upstream — e.g. a GridView row computed against a transient
        // zero/negative viewport extent) must not reach createOffscreenTexture():
        // casting an infinite or NaN float to uint32_t is undefined behavior,
        // and the resulting garbage width/height corrupts the D3D12 debug
        // layer's descriptor tracking (CreateRenderTargetView raising
        // D3D12SDKLayers!ReportCorruption) rather than failing gracefully.
        if (!std::isfinite(bounds.width) || !std::isfinite(bounds.height) ||
            bounds.width <= 0.0f || bounds.height <= 0.0f)
            return;

        const uint32_t tw = static_cast<uint32_t>(std::ceil(bounds.width  * dpr));
        const uint32_t th = static_cast<uint32_t>(std::ceil(bounds.height * dpr));
        if (tw == 0 || th == 0) return;

        auto child_tex = draw_backend_->createOffscreenTexture(tw, th);
        if (!child_tex) return;

        rpe->end();

        auto child_rpe = draw_backend_->beginOffscreenPass(child_tex, *frame_encoder_);
        if (child_rpe)
        {
            Matrix4 offset_mat = Matrix4::translate(
                vector_math::Vector3<float>(-bounds.x, -bounds.y, 0.0f));

            DrawList translated;
            translated.push_back(PushTransformCmd{offset_mat});
            for (const auto& cc : child_cmds) translated.push_back(cc);
            translated.push_back(PopTransformCmd{});

            draw_backend_->setViewportSize(static_cast<float>(tw), static_cast<float>(th));

            auto child_view = child_tex->createView(draw_backend_->offscreenPixelFormat(), 1);
            flushDrawList(translated, child_rpe, child_view,
                          static_cast<float>(tw), static_cast<float>(th), dpr,
                          /*backdrop_pass=*/false);
            child_rpe->end();

            draw_backend_->setViewportSize(viewport_width, viewport_height);
        }

        rpe = restartRenderPass(target_view);

        if (child_rpe)
        {
            draw_backend_->drawClipShapeComposite(
                child_tex, bounds, corner_radius, is_oval, transform, clip, *rpe);

            // Populate the cache for a future replay — see
            // applyShaderMask()'s identical comment.
            if (replay_region_id != nullptr)
            {
                ClipShapeGpuCacheEntry entry;
                entry.texture         = child_tex;
                entry.bounds          = bounds;
                entry.corner_radius   = corner_radius;
                entry.is_oval         = is_oval;
                entry.last_used_frame = frame_counter_;
                clip_shape_gpu_cache_[cache_key] = std::move(entry);
            }
        }
    }

    void Renderer::applyBoxShadow(
        const DrawShadowCmd&                                cmd,
        std::shared_ptr<campello_gpu::RenderPassEncoder>& rpe,
        std::shared_ptr<campello_gpu::TextureView>         target_view,
        float viewport_width,
        float viewport_height,
        float dpr,
        const Matrix4& transform,
        const Rect&    clip,
        const void*    replay_region_id,
        size_t         replay_bracket_index)
    {
        if (!draw_backend_ || !frame_encoder_) return;

        // Cache-hit path — see applyClipShape()'s identical comment and
        // shadow_gpu_cache_'s doc comment.
        const std::pair<const void*, size_t> cache_key{replay_region_id, replay_bracket_index};
        if (replay_region_id != nullptr)
        {
            auto it = shadow_gpu_cache_.find(cache_key);
            if (it != shadow_gpu_cache_.end())
            {
                it->second.last_used_frame = frame_counter_;
                const Rect& cb = it->second.bounds;
                const float cm = it->second.margin;
                DrawImageCmd img_cmd{
                    it->second.texture,
                    Rect::fromLTWH(0.0f, 0.0f, 1.0f, 1.0f),
                    Rect::fromLTWH(cb.x - cm, cb.y - cm,
                                   cb.width + 2.0f * cm, cb.height + 2.0f * cm),
                    1.0f };
                draw_backend_->drawImage(img_cmd, transform, clip, *rpe);
                return;
            }
        }

        // Path doesn't trace rounded corners itself yet (see
        // Path::addRRect()'s doc comment) — simpleRRectShape() is the only
        // reliable source of the corner radius for the common case (this is
        // the sole DrawShadowCmd producer). Falling back to the path's
        // bounding box with a square corner still draws a reasonable shadow
        // rather than none for any future non-simple-shape caller.
        RRect rr;
        if (auto simple = cmd.path.simpleRRectShape())
            rr = *simple;
        else
            rr = RRect{cmd.path.getBounds(), 0.0f};

        const Rect& bounds = rr.rect;
        if (!std::isfinite(bounds.width) || !std::isfinite(bounds.height) ||
            bounds.width <= 0.0f || bounds.height <= 0.0f)
            return;

        // cmd.elevation carries the shadow's blur_radius (see
        // RenderDecoratedBox::paintDecoration) — convert to a Gaussian sigma
        // via the same radius→sigma formula Flutter's BoxShadow.blurSigma
        // uses, so a given blur_radius produces comparably soft results.
        const float sigma  = cmd.elevation * 0.57735f + 0.5f;
        const float margin = std::min(sigma * 3.0f, 48.0f); // ~3-sigma cutoff, capped

        const float padded_w = bounds.width  + 2.0f * margin;
        const float padded_h = bounds.height + 2.0f * margin;

        const uint32_t tw = static_cast<uint32_t>(std::ceil(padded_w * dpr));
        const uint32_t th = static_cast<uint32_t>(std::ceil(padded_h * dpr));
        if (tw == 0 || th == 0) return;

        auto shadow_tex = draw_backend_->createOffscreenTexture(tw, th);
        if (!shadow_tex) return;

        rpe->end();

        std::shared_ptr<campello_gpu::Texture> blurred;
        auto shadow_rpe = draw_backend_->beginOffscreenPass(shadow_tex, *frame_encoder_);
        if (shadow_rpe)
        {
            draw_backend_->setViewportSize(static_cast<float>(tw), static_cast<float>(th));

            // Fill the shape — padded by `margin` on every side so the blur
            // has room to spread without clipping against the texture edge
            // — with the shadow's own color/alpha; blurring then diffuses
            // that alpha into a soft-edged shadow.
            //
            // The offscreen texture (and this pass's viewport, set above)
            // is sized in *physical* pixels (padded_w/h * dpr), but
            // local_bounds below is logical — so the transform must carry
            // the same dpr scale flushDrawList's own current_transform
            // seeds every other draw call with, or the shape is rasterized
            // at 1/dpr its intended size in the texture's top-left corner
            // (invisible at dpr==1, but shrunk-and-shifted at dpr>1, e.g.
            // iOS's Retina displays).
            Matrix4 dpr_scale  = Matrix4::identity();
            dpr_scale.data[0]  = dpr;
            dpr_scale.data[5]  = dpr;
            const Rect     texture_clip = Rect::fromLTWH(0.0f, 0.0f, padded_w, padded_h);
            const Rect     local_bounds = Rect::fromLTWH(margin, margin, bounds.width, bounds.height);
            if (rr.radius_x > 0.0f || rr.radius_y > 0.0f)
            {
                DrawRRectCmd rrect_cmd{
                    RRect{local_bounds, rr.radius_x, rr.radius_y},
                    Paint::filled(cmd.color) };
                draw_backend_->drawRRect(rrect_cmd, dpr_scale, texture_clip, *shadow_rpe);
            }
            else
            {
                DrawRectCmd rect_cmd{ local_bounds, Paint::filled(cmd.color) };
                draw_backend_->drawRect(rect_cmd, dpr_scale, texture_clip, *shadow_rpe);
            }

            shadow_rpe->end();

            blurred = draw_backend_->blurTexture(shadow_tex, sigma, sigma, *frame_encoder_);

            draw_backend_->setViewportSize(viewport_width, viewport_height);
        }

        rpe = restartRenderPass(target_view);

        if (blurred)
        {
            DrawImageCmd img_cmd{
                blurred,
                Rect::fromLTWH(0.0f, 0.0f, 1.0f, 1.0f),
                Rect::fromLTWH(bounds.x - margin, bounds.y - margin, padded_w, padded_h),
                1.0f };
            draw_backend_->drawImage(img_cmd, transform, clip, *rpe);

            // Populate the cache for a future replay — see
            // applyClipShape()'s identical comment.
            if (replay_region_id != nullptr)
            {
                ShadowGpuCacheEntry entry;
                entry.texture         = blurred;
                entry.bounds          = bounds;
                entry.margin          = margin;
                entry.last_used_frame = frame_counter_;
                shadow_gpu_cache_[cache_key] = std::move(entry);
            }
        }
    }

    void Renderer::evictStaleGpuCaches()
    {
        auto sweep = [&](auto& cache, uint64_t max_age)
        {
            for (auto it = cache.begin(); it != cache.end(); )
            {
                if (frame_counter_ - it->second.last_used_frame > max_age)
                    it = cache.erase(it);
                else
                    ++it;
            }
        };
        sweep(clip_shape_gpu_cache_, kClipShapeCacheMaxAgeFrames);
        sweep(shader_mask_gpu_cache_, kClipShapeCacheMaxAgeFrames);
        sweep(shadow_gpu_cache_, kClipShapeCacheMaxAgeFrames);
        sweep(text_texture_cache_, kTextTextureCacheMaxAgeFrames);
    }

    void Renderer::drawCachedText(
        const DrawTextCmd&                                 cmd,
        const Matrix4&                                     transform,
        const Rect&                                        clip,
        std::shared_ptr<campello_gpu::RenderPassEncoder>& rpe,
        float                                               dpr)
    {
        if (!draw_backend_) return;

        auto it = text_texture_cache_.find(cmd.span);
        if (it != text_texture_cache_.end())
        {
            auto& entry = it->second;
            entry.last_used_frame = frame_counter_;
            entry.bind_group = draw_backend_->drawTextTexture(
                entry.texture, entry.bind_group, entry.width, entry.height,
                cmd.origin, transform, clip, *rpe);
            return;
        }

        uint32_t w = 0, h = 0;
        auto texture = draw_backend_->rasterizeText(cmd.span, dpr, w, h);
        if (!texture) return;

        auto bind_group = draw_backend_->drawTextTexture(
            texture, nullptr, w, h, cmd.origin, transform, clip, *rpe);

        TextTextureCacheEntry entry;
        entry.texture         = std::move(texture);
        entry.bind_group      = std::move(bind_group);
        entry.width           = w;
        entry.height          = h;
        entry.last_used_frame = frame_counter_;
        text_texture_cache_[cmd.span] = std::move(entry);
    }

    std::shared_ptr<campello_gpu::RenderPassEncoder> Renderer::restartRenderPass(
        std::shared_ptr<campello_gpu::TextureView> target_view)
    {
        if (!frame_encoder_) return nullptr;

        GPU::ColorAttachment ca{};
        // target_view may be nullptr on Vulkan (swapchain), which is valid.
        ca.view    = target_view;
        ca.loadOp  = GPU::LoadOp::load;    // preserve what was drawn before
        ca.storeOp = GPU::StoreOp::store;

        GPU::BeginRenderPassDescriptor desc{};
        desc.colorAttachments = {ca};

        return frame_encoder_->beginRenderPass(desc);
    }

    // ------------------------------------------------------------------
    // Performance overlay — unified frame chart, matching Flutter
    // DevTools' "Flutter frames chart": one pair of adjacent bars per
    // frame (UI + raster), sharing a single chart area and a single
    // budget reference line at the panel's vertical midpoint (full panel
    // height == 2x the current display's refresh-rate budget, see
    // setDisplayRefreshHz()), with over-budget frames flagged in red.
    //
    // Each bar shows the *measured duration* of one frame phase, not how
    // often a frame was requested — see the build_sampler_/raster_sampler_
    // recordDuration() calls in buildFrame()/rasterFrame() for where these
    // are bracketed.
    // ------------------------------------------------------------------

    void Renderer::paintUnifiedFrameChart(
        PaintContext&                         ctx,
        const campello_gpu::FrameTimeSampler& build_sampler,
        const campello_gpu::FrameTimeSampler& raster_sampler,
        const campello_gpu::FrameTimeSampler& present_fps_sampler,
        float                                 chart_top,
        float                                 chart_w,
        float                                 panel_h,
        float                                 label_h,
        float                                 target_ms,
        float                                 max_ms,
        float                                 bar_w,
        int                                   max_frames)
    {
        const int   ui_total    = build_sampler.count();
        const int   raster_total = raster_sampler.count();
        const float ui_avg    = (ui_total > 0) ? build_sampler.averageMs() : 0.0f;
        const float raster_avg = (raster_total > 0) ? raster_sampler.averageMs() : 0.0f;

        // present_fps_sampler's average is an inter-frame *cadence* (ms
        // between successive presents), not a phase cost — invert it to
        // get an actual displayed frames-per-second. count() == 0 right
        // after a fresh start or an idle-gap reset() (see its doc comment
        // in renderer.hpp), so there's deliberately no fps to show yet.
        const int   fps_total = present_fps_sampler.count();
        const float fps_avg   = (fps_total > 0) ? 1000.0f / present_fps_sampler.averageMs() : 0.0f;

        // Only plot the most recent max_frames samples — the samplers
        // retain more history than that (kCapacity), so this lets the
        // chart show fewer, wider bars without discarding the rest of the
        // retained history (still reflected in the averages above).
        const int ui_n      = std::min(ui_total, max_frames);
        const int raster_n  = std::min(raster_total, max_frames);
        const int ui_skip     = ui_total - ui_n;
        const int raster_skip = raster_total - raster_n;

        // UI and raster colors mirror Flutter DevTools' frame chart (a
        // light blue/cyan UI bar, a purple raster bar); over-budget bars
        // are flagged red regardless of phase, same as DevTools' jank
        // overlay, so the color split (which phase dominates) is only
        // meaningful for frames within budget.
        const Color kUiColor     = Color::fromRGBA(0.30f, 0.70f, 0.90f, 1.0f);
        const Color kRasterColor = Color::fromRGBA(0.65f, 0.40f, 0.85f, 1.0f);
        const Color kJankColor   = Color::fromRGBA(0.90f, 0.20f, 0.20f, 1.0f);

        if (DebugFlags::performanceOverlayTextEnabled)
        {
            char fps_buf[8];
            if (fps_total > 0)
                std::snprintf(fps_buf, sizeof(fps_buf), "%.0f", fps_avg);
            else
                std::snprintf(fps_buf, sizeof(fps_buf), "--");

            char buf[112];
            std::snprintf(buf, sizeof(buf), "UI: %.1f ms   RASTER: %.1f ms   FPS: %s",
                          ui_avg, raster_avg, fps_buf);
            // font_size is in physical pixels, like every other drawText()
            // call site (RenderText/RenderParagraph/RenderTextField all
            // multiply by dpr before drawing) — without this the glyph
            // rasterizes at 1/dpr the intended size on any Retina/high-DPI
            // display.
            TextStyle style{Color::white(), 11.0f, {}};
            style.font_size *= device_pixel_ratio_;
            ctx.canvas().drawText(
                TextSpan{buf, style},
                Offset{6.0f, chart_top + 4.0f});
        }

        const float chart_bottom = chart_top + label_h + panel_h;

        const int   n      = std::max(ui_n, raster_n);
        // Each frame group is three equal segments: UI bar, raster bar,
        // then a blank segment of the same width before the next group —
        // so a frame's UI+RASTER pair reads as one visually adjacent
        // block, clearly separated from the next frame's pair.
        const float seg_w  = bar_w / 3.0f;

        for (int i = 0; i < n; ++i)
        {
            const float group_x = static_cast<float>(i) * bar_w;

            if (i < ui_n)
            {
                const float ms    = build_sampler.at(ui_skip + i);
                const float frac  = std::min(ms / max_ms, 1.0f);
                const float bar_h = frac * panel_h;
                const Color color = (ms > target_ms) ? kJankColor : kUiColor;
                ctx.canvas().drawRect(
                    Rect::fromLTWH(group_x, chart_bottom - bar_h, seg_w, bar_h),
                    Paint::filled(color));
            }

            if (i < raster_n)
            {
                const float ms    = raster_sampler.at(raster_skip + i);
                const float frac  = std::min(ms / max_ms, 1.0f);
                const float bar_h = frac * panel_h;
                const Color color = (ms > target_ms) ? kJankColor : kRasterColor;
                ctx.canvas().drawRect(
                    Rect::fromLTWH(group_x + seg_w, chart_bottom - bar_h, seg_w, bar_h),
                    Paint::filled(color));
            }

            // group_x + 2*seg_w .. group_x + 3*seg_w is the blank segment —
            // intentionally left undrawn so the background shows through.
        }

        // Single budget reference line at target_ms — with max_ms == 2 ×
        // target_ms (see paintPerformanceOverlay), this sits exactly at
        // the panel's vertical midpoint, so a bar filling the full panel
        // height represents 2 × target_ms (i.e. 30fps-equivalent cost).
        {
            const float line_y = chart_bottom - (target_ms / max_ms) * panel_h;
            ctx.canvas().drawRect(
                Rect::fromLTWH(0.0f, line_y, chart_w, 1.0f),
                Paint::filled(Color::fromRGBA(0.90f, 0.90f, 0.90f, 0.70f)));
        }
    }

    void Renderer::paintPerformanceOverlay(
        PaintContext& ctx,
        float         /*viewport_width*/,
        float         viewport_height)
    {
        constexpr float kPanelH = 90.0f;
        constexpr float kLabelH = 16.0f;
        constexpr float kTotalH = kLabelH + kPanelH;
        // Budget line tracks the *actual* display refresh rate (see
        // setDisplayRefreshHz()'s doc) rather than assuming 60Hz — a
        // window dragged onto a 144Hz monitor has a real ~6.9ms budget,
        // not 16.67ms, and the overlay should say so.
        const float kTargetMs = 1000.0f / display_refresh_hz_;
        // Full panel height represents 2x the target-rate budget, so the
        // single budget line sits at the panel's vertical midpoint — a bar
        // filling the whole panel is half-the-target-rate-equivalent cost.
        const float kMaxMs = 2.0f * kTargetMs;

        // Each frame group is three 4px segments — UI bar, raster bar,
        // blank gap — so a frame's UI+RASTER pair reads as one adjacent
        // block, clearly separated from the next frame's pair (see
        // paintUnifiedFrameChart). Show fewer, wider bars than the full
        // retained sample history (campello_gpu::FrameTimeSampler::
        // kCapacity, currently 64) — the samplers still retain the full
        // history for the averages shown in the label.
        constexpr float kSegmentW        = 4.0f;
        constexpr float kBarW            = 3.0f * kSegmentW;
        constexpr int   kDisplayedFrames = 20;
        constexpr float kOverlayW        = kBarW * static_cast<float>(kDisplayedFrames);

        const float oy = viewport_height - kTotalH;

        // Not a widget — this is painted directly by the Renderer outside
        // the widget tree, so it can't be wrapped in a SafeArea widget the
        // normal way. Applying the same insets manually here is the
        // equivalent: without it the overlay's bottom edge sits flush with
        // the physical screen edge, under the iOS home-indicator (or any
        // other platform's bottom system chrome).
        ctx.canvas().save();
        ctx.canvas().translate(view_insets_.left, -view_insets_.bottom);

        ctx.canvas().drawRect(
            Rect::fromLTWH(0.0f, oy, kOverlayW, kTotalH),
            Paint::filled(Color::fromRGBA(0.10f, 0.10f, 0.10f, 0.80f)));

        paintUnifiedFrameChart(ctx, build_sampler_, raster_sampler_, present_fps_sampler_, oy,
                               kOverlayW, kPanelH, kLabelH, kTargetMs, kMaxMs, kBarW,
                               kDisplayedFrames);

        ctx.canvas().restore();
    }

} // namespace systems::leal::campello_widgets
