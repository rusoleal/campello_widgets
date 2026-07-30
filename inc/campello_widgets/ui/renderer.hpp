#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <campello_gpu/frame_time_sampler.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/draw_command.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/draw_backend.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/edge_insets.hpp>
#include <campello_widgets/ui/image_filter.hpp>
#include <campello_widgets/ui/frame_package.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <campello_widgets/ui/dirty_region.hpp>
#include <campello_widgets/ui/frame_scheduler.hpp>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

// campello_gpu forward declarations
namespace systems::leal::campello_gpu
{
    class Device;
    class TextureView;
    class CommandEncoder;
    class RenderPassEncoder;
    class Texture;
    class BindGroup;
}

namespace systems::leal::campello_widgets
{

    // Forward declaration for static accessor
    class Renderer;

    namespace detail {
        // Internal accessor for widgets that need to query view insets.
        // This is a temporary solution until MediaQuery is implemented.
        inline std::atomic<Renderer*>& currentRenderer() noexcept {
            static std::atomic<Renderer*> instance{nullptr};
            return instance;
        }
    }

    /**
     * @brief Drives the per-frame render loop: layout → paint → GPU submit.
     *
     * Renderer bridges the campello_gpu device and the widget framework's
     * RenderObject tree. Each call to `renderFrame()` runs:
     *
     *  1. **Layout pass** — calls `root_render_object->layout(screen_constraints)`,
     *     which propagates constraints top-down and resolves sizes bottom-up.
     *
     *  2. **Paint pass** — opens a render pass on `target`, creates a `PaintContext`,
     *     and calls `root_render_object->paint(context, Offset::zero())`.
     *     Each RenderObject appends DrawCommands to the context.
     *
     *  3. **Flush** — iterates the DrawList and dispatches each command to the
     *     GPU via the RenderPassEncoder.  Actual GPU draw calls require compiled
     *     shader pipelines; these are set up by registering an `IDrawBackend`
     *     in Phase 5. Until then, commands are accumulated and logged.
     *
     *  4. **Submit** — ends the render pass, calls `encoder->finish()`, and
     *     submits the CommandBuffer to the device.
     *
     * The widget/element tree is managed by the application. The Renderer only
     * requires a root `RenderBox` — how that render object is populated (manually
     * or via the element tree) is left to the caller.
     *
     * **Typical setup:**
     * @code
     * auto renderer = std::make_shared<Renderer>(device, root_render_box);
     *
     * // In your platform display callback:
     * auto target = device->getSwapchainTextureView();
     * renderer->renderFrame(target, viewport_width, viewport_height);
     * @endcode
     */
    class Renderer
    {
    public:
        /**
         * @param device              The campello_gpu device to render with.
         * @param root_render_object  Root of the RenderBox tree to render.
         * @param clear_color         Background clear color for each frame.
         */
        Renderer(
            std::shared_ptr<campello_gpu::Device> device,
            std::shared_ptr<RenderBox>            root_render_object,
            Color                                 clear_color = Color::black());

        ~Renderer();

        // ------------------------------------------------------------------
        // Per-frame entry point
        // ------------------------------------------------------------------

        /**
         * @brief UI-thread phase: ticks input/animation, rebuilds the widget
         *        tree, lays out and paints it into a value-type `DrawList`,
         *        and packages everything the raster phase needs into a
         *        `FramePackage`.
         *
         * Must be called from the UI thread (the same thread that owns the
         * widget tree and the platform event loop). Returns `std::nullopt`
         * if nothing changed this frame (mirrors the old `renderFrame()`'s
         * `return false` path) — the caller should skip rastering entirely
         * in that case.
         *
         * The returned `FramePackage` does not yet have `target` set;
         * callers must assign it before passing the package to
         * `rasterFrame()` (kept as a separate step so the swapchain
         * texture/drawable can be acquired by the platform layer either
         * before or after `buildFrame()`, as convenient).
         *
         * @param viewport_width   Viewport width in logical pixels.
         * @param viewport_height  Viewport height in logical pixels.
         */
        std::optional<FramePackage> buildFrame(float viewport_width, float viewport_height);

        /**
         * @brief Raster phase: encodes and submits GPU commands for an
         *        already-built `FramePackage`.
         *
         * Reads only the immutable `package` parameter and `Renderer`
         * members that are exclusively raster-phase-owned (`frame_encoder_`,
         * `frame_target_`, `backdrop_tex_`, `blurred_backdrop_tex_`,
         * `draw_backend_`) — never a `Renderer` member that `buildFrame()`
         * might mutate for a later frame. Safe to call from a dedicated
         * raster thread as long as `rasterFrame()` is never called
         * concurrently with itself (e.g. via `RasterThread`'s single-worker
         * depth-1 handoff).
         *
         * @param package  A `FramePackage` produced by `buildFrame()`, with
         *                 `target` assigned.
         */
        bool rasterFrame(const FramePackage& package);

        /**
         * @brief Renders one frame to `target` — `buildFrame()` followed by
         *        `rasterFrame()` on the calling thread.
         *
         * Kept for platforms that have not adopted a separate raster
         * thread; behaviourally identical to calling `buildFrame()` then
         * `rasterFrame()` directly.
         *
         * @warning This method is **not** thread-safe. Concurrent calls from
         *          multiple threads, or calls while the widget tree is being
         *          mutated on another thread, will cause data races and
         *          undefined behaviour.
         *
         * @param target           Swapchain (or offscreen) texture view to render into.
         * @param viewport_width   Viewport width in logical pixels.
         * @param viewport_height  Viewport height in logical pixels.
         */
        bool renderFrame(
            std::shared_ptr<campello_gpu::TextureView> target,
            float viewport_width,
            float viewport_height);

        /**
         * @brief Schedules a native drawable for presentation on the NEXT submit().
         *
         * Call this before renderFrame() when rendering to a swapchain.  The
         * drawable is only attached to the command buffer that renderFrame()
         * submits, so nested submit() calls (e.g. from offscreen renderers
         * triggered during layout) do not consume it prematurely.
         */
        void setPendingDrawable(void* nativeDrawable) noexcept
        {
            pending_drawable_ = nativeDrawable;
        }

        /**
         * @brief Reports that some node in the tree became paint-dirty.
         *
         * Called from `RenderObject::markNeedsPaint()` on every dirty
         * transition, regardless of how far it bubbles up the tree — since
         * `markNeedsPaint()` now stops at the nearest repaint boundary
         * (see `RenderObject::isRepaintBoundary()`) rather than always
         * reaching the root, `root_->needsPaint()` can no longer be used as
         * "does anything need painting" — a dirty leaf under a boundary
         * would leave the root's own flag false. This latch is the
         * decoupled replacement: `buildFrame()` consumes (checks and
         * clears) it instead. Mirrors Flutter's `PipelineOwner.
         * requestVisualUpdate()`, which is likewise decoupled from any
         * single RenderObject's own dirty flag. Idempotent — safe to call
         * every time, whether or not a frame is already pending.
         */
        void notePaintRequested() noexcept { paint_requested_ = true; }

        // ------------------------------------------------------------------
        // Configuration
        // ------------------------------------------------------------------

        void setClearColor(Color color) noexcept { clear_color_ = color; }
        Color clearColor() const noexcept { return clear_color_; }

        /**
         * @brief Sets the device pixel ratio (DPR) for logical-to-physical pixel conversion.
         *
         * The DPR is the ratio of physical pixels to logical pixels. For example,
         * on a Retina display with 2x scaling, the DPR would be 2.0.
         *
         * The Renderer uses this to convert physical viewport dimensions to logical
         * pixels for layout, keeping all widget layout in device-independent units.
         *
         * @param dpr The device pixel ratio (must be > 0, typically 1.0, 2.0, 3.0, etc.)
         */
        void setDevicePixelRatio(float dpr) noexcept;

        /** @brief Returns the current device pixel ratio. */
        float devicePixelRatio() const noexcept { return device_pixel_ratio_; }

        /**
         * @brief Sets the refresh rate of the display this content is
         * currently presented on, in Hz — used only by the performance
         * overlay's budget reference line (`paintUnifiedFrameChart()`), not
         * by frame scheduling itself (this is an on-demand renderer; actual
         * present cadence is paced by the OS compositor/vsync of whichever
         * display the window is on, and already adapts automatically —
         * this setter just lets the overlay's "budget" line reflect that
         * same reality instead of always assuming 60Hz).
         *
         * Platforms with multiple displays at different refresh rates
         * (e.g. macOS with an external 144Hz monitor) should call this
         * whenever the window moves to a different display — there's no
         * portable way for the Renderer to detect that itself. Defaults to
         * 60.0 so platforms that never call this see unchanged behavior.
         *
         * @param hz Refresh rate in Hz (clamped to a sane [1, 1000] range).
         */
        void setDisplayRefreshHz(float hz) noexcept;

        /** @brief Returns the display refresh rate last set via `setDisplayRefreshHz()`. */
        float displayRefreshHz() const noexcept { return display_refresh_hz_; }

        /**
         * @brief Registers the platform-specific draw backend.
         *
         * Must be set before the first `renderFrame()` call for GPU drawing
         * to actually execute. Without a backend, the DrawList is iterated
         * but no GPU draw calls are issued.
         */
        void setDrawBackend(std::unique_ptr<IDrawBackend> backend) noexcept
        {
            draw_backend_ = std::move(backend);
        }

        IDrawBackend* drawBackend() const noexcept { return draw_backend_.get(); }

        campello_gpu::Device& device() noexcept { return *device_; }

        /** @brief The GPU device used by this renderer (shared ownership). */
        std::shared_ptr<campello_gpu::Device> sharedDevice() const noexcept { return device_; }

        // ------------------------------------------------------------------
        // BackdropFilter support
        // ------------------------------------------------------------------

        /**
         * @brief Called by RenderBackdropFilter during paint.
         *
         * Tells the Renderer that at least one BackdropFilter exists in the
         * scene this frame, records the maximum blur sigma so the
         * pre-computed backdrop texture uses the correct blur level, and
         * records `bounds` (this filter's own on-screen rect) so
         * `buildFrame()` can later decide, once the whole frame's paint
         * walk is done, whether the capture pass can actually be skipped —
         * see `noteDirtyRegion()`.
         */
        void noteBackdropFilter(const Rect& bounds, const ImageFilter& filter) noexcept
        {
            has_backdrop_filter_ = true;
            backdrop_regions_.push_back(bounds);
            if (filter.sigma_x > max_sigma_x_) max_sigma_x_ = filter.sigma_x;
            if (filter.sigma_y > max_sigma_y_) max_sigma_y_ = filter.sigma_y;
        }

        // ------------------------------------------------------------------
        // Dirty-region tracking (per-frame; reset in layoutPass())
        // ------------------------------------------------------------------

        /**
         * @brief Reports that `bounds` (a node's on-screen rect) actually
         *        changed or moved this frame.
         *
         * Called from `RenderObject::paint()` (for nodes whose
         * `needs_paint_` was set) and from `OffsetLayer::maybeReplay()`'s
         * cheap-reposition path (for content that moved without
         * repainting). Used by `buildFrame()` to decide whether any
         * `BackdropFilter` region actually needs a fresh capture this
         * frame — not a general damage-tracking system, only enough to
         * make that one decision.
         */
        void noteDirtyRegion(const Rect& bounds) noexcept
        {
            if (dirty_region_overflowed_) return;
            if (dirty_rects_.size() >= kMaxDirtyRects) {
                dirty_region_overflowed_ = true;
                dirty_rects_.clear();
                return;
            }
            dirty_rects_.push_back(bounds);
        }

        /**
         * @brief Drops any clip-shape/shader-mask/shadow GPU cache entries
         * keyed by `region_id`.
         *
         * Call this from an `OffsetLayer`'s destructor, passing its own
         * address (the same value it uses as `region_id` in
         * `CacheReplayBeginCmd`). Those caches are keyed by the
         * `OffsetLayer`'s raw address purely as an opaque, never-
         * dereferenced map key (see `clip_shape_gpu_cache_`'s doc comment)
         * — safe against address reuse only because a *fresh* `OffsetLayer`
         * never reads the cache on its first paint (always a full record,
         * never a replay). But its first *replay* (second paint, once
         * offset-unchanged) does consult the cache, and without this call
         * that lookup can spuriously hit a *stale* entry a different,
         * already-destroyed `OffsetLayer` left behind at the same
         * (reused) address — the periodic idle-sweep
         * (`kClipShapeCacheMaxAgeFrames`) only reclaims entries unused for
         * ~2 seconds, far slower than a virtualized list/grid recycling
         * same-sized cells can reuse a freed address. Without this,
         * scrolling a `GridView`/`ListView` whose cells clip their content
         * can make a newly-mounted cell briefly render a previous,
         * unrelated cell's cached appearance.
         */
        void evictReplayCacheEntries(const void* region_id) noexcept;

        // ------------------------------------------------------------------
        // View insets (safe area, keyboard, etc.)
        // ------------------------------------------------------------------

        /**
         * @brief Sets the view insets to apply around the root render object.
         *
         * View insets represent areas of the screen that are partially or fully
         * obscured by system UI (status bar, notch, home indicator, keyboard).
         * The renderer subtracts these insets from the viewport constraints
         * passed to the root render object, effectively creating a "safe area"
         * where content won't be obscured.
         *
         * Call this whenever the safe area changes (e.g., on orientation change,
         * keyboard show/hide, or when entering/leaving multi-window mode).
         *
         * @param insets The insets to apply (left, top, right, bottom in logical pixels).
         */
        void setViewInsets(const EdgeInsets& insets) noexcept
        {
            if (!(view_insets_ == insets)) {
                view_insets_ = insets;
                if (root_) root_->markNeedsLayout();
            }
        }

        /** @brief Returns the current view insets. */
        EdgeInsets viewInsets() const noexcept { return view_insets_; }

        /**
         * @brief Forces a full refresh of the widget tree.
         *
         * Marks the root render object as needing layout, and *every* node
         * in the tree (not just root) as needing paint. Call this when
         * global rendering flags change (e.g., debug overlays).
         *
         * Marking only the root dirty is enough to guarantee a frame
         * actually renders (paint() unconditionally cascades into
         * children, and a clean repaint boundary would otherwise
         * clean-replay its subtree instead of re-visiting it) — sufficient
         * for something like the performance overlay, which is an
         * unconditional post-process draw step with no per-node dirty
         * check. It is *not* sufficient for a flag like
         * `repaintRainbowEnabled`, whose effect is gated on each
         * individual node's own `needs_paint_` (`was_dirty` at
         * `RenderObject::paint()`'s entry) — with only root marked, every
         * descendant's own flag reads false and nothing visibly flashes,
         * even though they did technically repaint.
         *
         * Explicitly requests a frame and latches paint_requested_ itself,
         * rather than relying entirely on markNeedsPaint()'s internal calls
         * to do so: markNeedsPaint() only calls FrameScheduler::
         * scheduleFrame()/notePaintRequested() when a node's own
         * needs_paint_ transitions false -> true, and returns immediately
         * (skipping both) if that node was already dirty for an unrelated
         * reason (e.g. a widget mid-animation) — which would otherwise make
         * a debug-flag toggle's visibility depend on incidental tree state
         * instead of being guaranteed on the very next frame.
         */
        void forceRefresh()
        {
            if (!root_) return;
            root_->markNeedsLayout();
            std::function<void(RenderBox*)> markAll = [&markAll](RenderBox* node) {
                if (!node) return;
                node->markNeedsPaint();
                node->visitRenderChildren(markAll);
            };
            markAll(root_.get());
            notePaintRequested();
            FrameScheduler::scheduleFrame();
        }

        /**
         * @brief Records a new presentation timestamp for the real-FPS
         * sampler, discarding (not storing) the delta if it spans an
         * idle gap rather than an ordinary frame.
         *
         * `sampler` normally just accumulates call-to-call deltas via
         * `record()`. But this is fed from an on-demand renderer — no
         * frame happens at all while nothing requests one — so the first
         * call after being idle for a while would otherwise store one
         * wildly small "instant fps" sample (e.g. one frame after 3s
         * idle ≈ 0.3fps) and drag the rolling average down for up to
         * `FrameTimeSampler::kCapacity` frames right when a fresh
         * animation starts. When the gap since `last_ms` exceeds
         * `idle_gap_reset_ms`, `sampler` is reset() first, so this call
         * only re-establishes the baseline (per `record()`'s own "first
         * call stores nothing" contract) instead of recording the gap.
         *
         * A static method with no `this` (takes the sampler and baseline
         * explicitly) purely so it can be unit-tested with synthetic
         * timestamps — `rasterFrame()` can't otherwise be exercised
         * without a real `Device`/GPU (see test_renderer.cpp).
         *
         * @param idle_gap_reset_ms  Comfortably above one vsync period
         *   even at a slow 30Hz display, so this only fires on a genuine
         *   "nothing was requested for a while" gap, not an ordinary
         *   slow/janky frame.
         */
        static void recordPresentSample(
            campello_gpu::FrameTimeSampler& sampler,
            uint64_t&                       last_ms,
            uint64_t                        now_ms,
            uint64_t                        idle_gap_reset_ms = 200);

    private:
        void layoutPass(float viewport_width, float viewport_height);

        // Generates the draw list headlessly (no GPU encoder) so it can be
        // replayed to multiple render passes in one frame.
        DrawList generateDrawList(float viewport_width, float viewport_height);

        // Flushes draw commands to `rpe`, which currently targets `target_view`.
        // backdrop_pass = true  → skips backdrop-filter child commands (capture mode).
        // backdrop_pass = false → handles backdrop-filter commands normally.
        // `rpe` may be replaced (and re-target `target_view`) when a
        // ShaderMask/ClipRRect/ClipOval region is encountered — target_view
        // must be the actual color attachment `rpe` was opened against
        // (the swapchain during the main pass, backdrop_tex_ during backdrop
        // capture, or a child texture during a nested offscreen composite),
        // NOT always the swapchain, or content drawn after the first such
        // region within this call gets silently redirected to the wrong
        // render target.
        void flushDrawList(
            const DrawList&                                    commands,
            std::shared_ptr<campello_gpu::RenderPassEncoder>& rpe,
            std::shared_ptr<campello_gpu::TextureView>         target_view,
            float viewport_width,
            float viewport_height,
            float dpr,
            bool  backdrop_pass = false);

        // Applies a ShaderMask region: renders child commands to an offscreen
        // texture, then composites with the gradient mask into the main pass.
        // `target_view` is re-bound via restartRenderPass() after the
        // offscreen composite — see flushDrawList() for why it must match
        // whatever `rpe` currently targets.
        //
        // `replay_region_id`/`replay_bracket_index` identify this bracket's
        // slot in `shader_mask_gpu_cache_` — see the cache members' doc
        // comment below. Default (`nullptr`, 0) means "not inside a
        // CacheReplayBeginCmd/EndCmd region", which disables the cache
        // entirely for this call (no lookup, no store): only brackets
        // encountered while replaying a previously-recorded, byte-for-byte-
        // unchanged region are safe to key by (region_id, bracket index)
        // alone, per CacheReplayBeginCmd's doc comment.
        void applyShaderMask(
            const DrawShaderMaskBeginCmd&                      cmd,
            const DrawList&                                    child_cmds,
            std::shared_ptr<campello_gpu::RenderPassEncoder>& rpe,
            std::shared_ptr<campello_gpu::TextureView>         target_view,
            float viewport_width,
            float viewport_height,
            float dpr,
            const Matrix4& transform,
            const Rect&    clip,
            const void*    replay_region_id     = nullptr,
            size_t         replay_bracket_index  = 0);

        // Applies a ClipRRect/ClipOval region: renders child commands to an
        // offscreen texture, then composites through a rounded-rect/ellipse
        // SDF mask into the main pass. `corner_radius` is ignored when
        // `is_oval` is true. See flushDrawList() for `target_view`, and
        // applyShaderMask() above for `replay_region_id`/`replay_bracket_index`.
        void applyClipShape(
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
            const void*    replay_region_id     = nullptr,
            size_t         replay_bracket_index  = 0);

        // Applies a box shadow (DrawShadowCmd): fills the shadow's shape
        // (recovered from the path via Path::simpleRRectShape(), falling
        // back to its bounding box) into a padded offscreen texture, runs
        // it through the existing two-pass Gaussian blur, and composites
        // the result into the main pass — mirrors applyClipShape()'s
        // offscreen-then-composite structure, but with a plain shape fill
        // + blur instead of a mask. `cmd.elevation` carries the shadow's
        // blur_radius (see RenderDecoratedBox::paintDecoration's call
        // site); converted to a Gaussian sigma via the same formula
        // Flutter's BoxShadow.blurSigma uses. See flushDrawList() for
        // `target_view`, and applyShaderMask() above for
        // `replay_region_id`/`replay_bracket_index` — same cache-gating
        // mechanism, backed by shadow_gpu_cache_ below.
        void applyBoxShadow(
            const DrawShadowCmd&                               cmd,
            std::shared_ptr<campello_gpu::RenderPassEncoder>& rpe,
            std::shared_ptr<campello_gpu::TextureView>         target_view,
            float viewport_width,
            float viewport_height,
            float dpr,
            const Matrix4& transform,
            const Rect&    clip,
            const void*    replay_region_id     = nullptr,
            size_t         replay_bracket_index  = 0);

        // Renders `child_cmds` (a `RenderDrawSurface`'s newly-added stroke
        // segments) directly into `cmd.target` — preserving its existing
        // content unless `cmd.clear_first` is set — instead of into an
        // offscreen texture that then gets composited into the main pass.
        // There is no composite step: the caller (RenderDrawSurface) emits
        // its own DrawImageCmd for `cmd.target` right after this bracket,
        // so the persistent texture is simply displayed like any other
        // image once this call returns. See flushDrawList() for
        // `target_view`.
        void applyDrawSurfaceUpdate(
            const DrawSurfaceUpdateBeginCmd&                   cmd,
            const DrawList&                                    child_cmds,
            std::shared_ptr<campello_gpu::RenderPassEncoder>& rpe,
            std::shared_ptr<campello_gpu::TextureView>         target_view,
            float viewport_width,
            float viewport_height,
            float dpr);

        // Restarts a render pass on `target_view` with LoadOp::load (preserves
        // existing content).  Used after an offscreen composite operation.
        std::shared_ptr<campello_gpu::RenderPassEncoder> restartRenderPass(
            std::shared_ptr<campello_gpu::TextureView> target_view);

        /**
         * @brief Draws the performance overlay: a single unified frame
         * chart, matching Flutter DevTools' "Flutter frames chart" — one
         * pair of adjacent bars per frame (UI + raster), sharing one chart
         * area and one budget reference line (at the panel's vertical
         * midpoint — full panel height is 2x the current display's
         * refresh-rate budget, see `setDisplayRefreshHz()`), with frames
         * over budget highlighted in red. See buildFrame()/rasterFrame()
         * for where each phase is bracketed with start/end timestamps.
         */
        void paintPerformanceOverlay(
            PaintContext& ctx,
            float         viewport_width,
            float         viewport_height);

        // Draws the unified frame chart: each frame occupies a group of
        // three equal segments — UI bar, raster bar, then a blank segment
        // — so a frame's UI+RASTER pair reads as one adjacent block,
        // clearly separated from the next frame's pair. `bar_w` is the
        // full group width (3 segments); the per-bar width is bar_w / 3.
        // Drawn against a shared background and shared budget reference
        // lines. Only the most recent `max_frames` samples are drawn (the
        // samplers retain more — see campello_gpu::FrameTimeSampler::
        // kCapacity — so the chart can show fewer, wider bars than the
        // full retained history).
        void paintUnifiedFrameChart(
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
            int                                   max_frames);

        std::shared_ptr<campello_gpu::Device> device_;
        std::shared_ptr<RenderBox>            root_;
        Color                                 clear_color_;
        std::unique_ptr<IDrawBackend>         draw_backend_;

        // --- performance overlay state ---
        // Actual measured phase durations per frame (not call-to-call
        // cadence) — see buildFrame()/rasterFrame() for the start/end
        // brackets. build_sampler_ is written by buildFrame() (UI thread)
        // and read by paintPerformanceOverlay() (also UI thread, inside
        // buildFrame()'s generateDrawList() call) — no cross-thread access.
        // raster_sampler_ is written by rasterFrame(), which may run on a
        // separate raster thread, while it's read by the *next* frame's
        // buildFrame() on the UI thread — sampler_mutex_ guards both
        // samplers against that cross-thread access. FrameTimeSampler
        // itself documents "not thread-safe, call only from the render
        // thread" — this mutex is what makes that contract hold once
        // build and raster run on different threads.
        mutable std::mutex             sampler_mutex_;
        campello_gpu::FrameTimeSampler build_sampler_;  // build + layout + paint recording
        campello_gpu::FrameTimeSampler raster_sampler_; // GPU encode + submit

        // Actual presentation cadence (real FPS), as distinct from the cost
        // samplers above: build_sampler_/raster_sampler_ each answer "how
        // expensive was this phase", not "how often did a frame actually
        // reach the screen" — a frame can be cheap on both counts and still
        // not be presented promptly (thread scheduling, a stalled backend,
        // etc). present_fps_sampler_ uses FrameTimeSampler::record()'s
        // call-to-call-cadence mode (not recordDuration()) to measure the
        // wall-clock delta between successive rasterFrame() completions —
        // see the bottom of rasterFrame() for where it's fed.
        //
        // This is an on-demand renderer (frames only happen when something
        // requests one — see FrameScheduler): after being idle for a while,
        // the next rasterFrame() call is naturally far apart from the last
        // one, which would otherwise register as a single nonsensical
        // "instant fps" sample (e.g. one frame after 3s idle => ~0.3fps)
        // and drag the rolling average down for up to kCapacity frames
        // right when a fresh animation starts. last_present_ms_ lets
        // rasterFrame() detect that gap and reset() the sampler instead of
        // recording it, so resuming from idle starts a clean new window.
        campello_gpu::FrameTimeSampler present_fps_sampler_;
        uint64_t                       last_present_ms_ = 0;

        // --- paint-requested latch (see notePaintRequested()'s doc) ---
        // Not per-frame state (unlike dirty_rects_/backdrop_regions_ below,
        // which are reset every layoutPass()) — persists across buildFrame()
        // calls until consumed, since a markNeedsPaint() can happen at any
        // time, not just during an active frame build. Defaults true so the
        // first buildFrame() after construction renders even though nothing
        // has explicitly called markNeedsPaint() yet (mirrors every
        // RenderObject's own needs_paint_ defaulting true on construction).
        bool paint_requested_ = true;

        // --- view insets (safe area) ---
        EdgeInsets view_insets_;

        // --- device pixel ratio ---
        float device_pixel_ratio_ = 1.0f;

        // --- display refresh rate (see setDisplayRefreshHz()'s doc) ---
        float display_refresh_hz_ = 60.0f;

        // --- backdrop filter state (per-frame, reset in layoutPass) ---
        bool  has_backdrop_filter_ = false;
        float max_sigma_x_         = 0.0f;
        float max_sigma_y_         = 0.0f;
        std::vector<Rect> backdrop_regions_; // one entry per BackdropFilter painted this frame

        // --- dirty-region tracking (per-frame, reset in layoutPass) ---
        // See noteDirtyRegion()/dirtyRegionIntersects(). Capped so a very
        // "busy" frame degrades to the conservative "assume dirty"
        // fallback instead of growing unbounded.
        static constexpr size_t kMaxDirtyRects = 32;
        std::vector<Rect> dirty_rects_;
        bool              dirty_region_overflowed_ = false;

        // Offscreen textures for backdrop capture + blur (persistent, resized lazily).
        std::shared_ptr<campello_gpu::Texture> backdrop_tex_;
        std::shared_ptr<campello_gpu::Texture> blurred_backdrop_tex_;
        uint32_t backdrop_tex_w_ = 0;
        uint32_t backdrop_tex_h_ = 0;
        // The backdrop-capture pass's TextureView (bd_view in rasterFrame())
        // used to be a local, safe only because Device::submit() blocked
        // until the GPU was done before rasterFrame() returned and it went
        // out of scope. Now that submit() is async (see its doc comment in
        // campello_gpu's Vulkan device.cpp), it needs the same two-generation
        // defer as VulkanDrawBackend's frame_views_/frame_textures_ — see
        // rasterFrame()'s use of these for the rotation.
        std::shared_ptr<campello_gpu::TextureView> frame_bd_view_;
        std::shared_ptr<campello_gpu::TextureView> prev_frame_bd_view_;
        std::shared_ptr<campello_gpu::TextureView> prev2_frame_bd_view_;

        // --- clip-shape / shader-mask GPU composite cache ---
        //
        // Skips the offscreen capture-and-composite GPU work for a
        // ClipRRect/ClipOval/ShaderMask bracket that flushDrawList()
        // encounters inside a CacheReplayBeginCmd/EndCmd region — i.e. one
        // that OffsetLayer::maybeReplay() has already determined is a
        // byte-for-byte-unchanged replay of a previous frame's content (see
        // CacheReplayBeginCmd's doc comment). A RepaintBoundary correctly
        // skips re-walking the widget tree for such content, but without
        // this cache the *GPU* side of clip-shape/shader-mask compositing
        // still reran every frame regardless — this is what actually fixes
        // that gap.
        //
        // Keyed by (region_id, Nth clip-shape/shader-mask bracket since the
        // region began) — safe as a non-fragile key with no content
        // hashing needed, because "replay" only ever happens when nothing
        // in the region changed, so the same region_id always produces the
        // same sequence of brackets in the same order. `region_id` is an
        // OffsetLayer's own address, used purely as an opaque, never-
        // dereferenced map key: if the owning RenderObject unmounts, later
        // reuse of that address by an unrelated OffsetLayer is harmless,
        // because a fresh OffsetLayer's first paint is always a full
        // record (never identity-replay), and the eviction sweep below
        // drops any entry that goes unused for kClipShapeCacheMaxAgeFrames
        // frames regardless.
        struct ReplayKeyHash
        {
            size_t operator()(const std::pair<const void*, size_t>& k) const noexcept
            {
                return std::hash<const void*>{}(k.first) ^ (k.second * 0x9E3779B97F4A7C15ull);
            }
        };

        struct ClipShapeGpuCacheEntry
        {
            std::shared_ptr<campello_gpu::Texture> texture;
            Rect     bounds;
            float    corner_radius = 0.0f;
            bool     is_oval       = false;
            uint64_t last_used_frame = 0;
        };

        struct ShaderMaskGpuCacheEntry
        {
            std::shared_ptr<campello_gpu::Texture> texture;
            DrawShaderMaskBeginCmd cmd{Rect{}, LinearGradient{}};
            uint64_t last_used_frame = 0;
        };

        // Same rationale as ClipShapeGpuCacheEntry above, for
        // Renderer::applyBoxShadow() — without this, a RenderDecoratedBox
        // boundary correctly skipping its own re-walk on replay still left
        // the DrawShadowCmd's expensive offscreen blur composite (texture
        // alloc + two render-pass restarts) rerunning every frame
        // regardless, since that command is still present — just replayed,
        // not freshly recorded — in the frame's draw list either way.
        struct ShadowGpuCacheEntry
        {
            std::shared_ptr<campello_gpu::Texture> texture;
            Rect     bounds;         ///< Padded bounds the texture covers (unblurred shape's bounds + margin).
            float    margin = 0.0f;
            uint64_t last_used_frame = 0;
        };

        static constexpr uint64_t kClipShapeCacheMaxAgeFrames = 120;

        std::unordered_map<std::pair<const void*, size_t>, ClipShapeGpuCacheEntry, ReplayKeyHash>
            clip_shape_gpu_cache_;
        std::unordered_map<std::pair<const void*, size_t>, ShaderMaskGpuCacheEntry, ReplayKeyHash>
            shader_mask_gpu_cache_;
        std::unordered_map<std::pair<const void*, size_t>, ShadowGpuCacheEntry, ReplayKeyHash>
            shadow_gpu_cache_;

        // Platform-independent text-rasterization cache. GDI/CoreText/
        // FreeType+HarfBuzz rasterization plus GPU texture allocation/
        // upload is expensive; caching the result by (text, style) lets
        // unchanged text reuse the same GPU texture and bind group across
        // frames instead of redoing all of that work every frame — moved
        // here (out of each IDrawBackend implementation, which used to
        // carry this same cache three times over, once per platform) since
        // the caching *policy* is entirely platform-neutral: only
        // rasterizeText() itself differs per backend. Unlike
        // clip_shape_gpu_cache_/shader_mask_gpu_cache_ above, this cache is
        // keyed directly by content (TextSpan == text + style), so it's
        // inherently safe to check unconditionally for every DrawTextCmd —
        // no CacheReplayBeginCmd/EndCmd region gating needed, since equal
        // keys are always genuinely equal content, whether the command came
        // from a fresh paint or an OffsetLayer replay.
        //
        // The BindGroup is cached alongside the texture (same lifetime,
        // same eviction) so a cache hit also skips
        // IDrawBackend::drawTextTexture() having to rebuild it — dropping
        // it would not just cost CPU time back but, per
        // D3DDrawBackend::createBindGroup()'s doc comment, slowly exhaust
        // the shader-visible descriptor heap over a long session, since
        // that heap never reclaims slots. Safe to cache at this level
        // (unlike a clip-shape/shader-mask composite's bind group, which
        // deliberately is NOT cached here) because a text quad's bind group
        // is only ever {texture@0, sampler@1} with a fixed, never-varying
        // sampler — no other per-draw uniform is mixed in.
        //
        // DPR is intentionally not part of the key: neither this cache nor
        // the per-backend ones it replaces invalidate on a mid-session DPR
        // change (e.g. dragging the window to a different-DPI monitor) —
        // preserving that existing (rare-in-practice) gap rather than
        // introducing new key-matching complexity to close it.
        struct TextTextureCacheEntry
        {
            std::shared_ptr<campello_gpu::Texture>   texture;
            std::shared_ptr<campello_gpu::BindGroup> bind_group;
            uint32_t width  = 0;
            uint32_t height = 0;
            uint64_t last_used_frame = 0;
        };

        static constexpr uint64_t kTextTextureCacheMaxAgeFrames = 120;

        std::unordered_map<TextSpan, TextTextureCacheEntry, TextSpanHash> text_texture_cache_;

        // Looks up (or rasterizes via IDrawBackend::rasterizeText() and
        // inserts) cmd.span's texture, draws it via
        // IDrawBackend::drawTextTexture(), and marks the entry used on the
        // current frame.
        void drawCachedText(
            const DrawTextCmd&                                 cmd,
            const Matrix4&                                     transform,
            const Rect&                                        clip,
            std::shared_ptr<campello_gpu::RenderPassEncoder>& rpe,
            float                                              dpr);

        // Incremented once per rasterFrame() call; drives the eviction
        // sweeps above (mirrors D3DDrawBackend::OffscreenTexturePool's
        // last_used_frame_/kMaxAgeFrames pattern).
        uint64_t frame_counter_ = 0;

        void evictStaleGpuCaches();

        // --- frame-scoped pointers (valid only during renderFrame) ---
        campello_gpu::CommandEncoder*              frame_encoder_ = nullptr;
        std::shared_ptr<campello_gpu::TextureView> frame_target_;

        // Native drawable to present after this frame's command buffer (MTLDrawable / CAMetalDrawable).
        void* pending_drawable_ = nullptr;
    };

} // namespace systems::leal::campello_widgets
