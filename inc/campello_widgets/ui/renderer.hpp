#pragma once

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
#include <vector>

// campello_gpu forward declarations
namespace systems::leal::campello_gpu
{
    class Device;
    class TextureView;
    class CommandEncoder;
    class RenderPassEncoder;
    class Texture;
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
         * Marks the root render object as needing both layout and paint.
         * Call this when global rendering flags change (e.g., debug overlays).
         */
        void forceRefresh()
        {
            if (root_) {
                root_->markNeedsLayout();
                root_->markNeedsPaint();
            }
        }

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
        void applyShaderMask(
            const DrawShaderMaskBeginCmd&                      cmd,
            const DrawList&                                    child_cmds,
            std::shared_ptr<campello_gpu::RenderPassEncoder>& rpe,
            std::shared_ptr<campello_gpu::TextureView>         target_view,
            float viewport_width,
            float viewport_height,
            float dpr,
            const Matrix4& transform,
            const Rect&    clip);

        // Applies a ClipRRect/ClipOval region: renders child commands to an
        // offscreen texture, then composites through a rounded-rect/ellipse
        // SDF mask into the main pass. `corner_radius` is ignored when
        // `is_oval` is true. See flushDrawList() for `target_view`.
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
            const Rect&    clip);

        // Restarts a render pass on `target_view` with LoadOp::load (preserves
        // existing content).  Used after an offscreen composite operation.
        std::shared_ptr<campello_gpu::RenderPassEncoder> restartRenderPass(
            std::shared_ptr<campello_gpu::TextureView> target_view);

        /**
         * @brief Draws the performance overlay: a single unified frame
         * chart, matching Flutter DevTools' "Flutter frames chart" — one
         * pair of adjacent bars per frame (UI + raster), sharing one chart
         * area and one budget reference line (at the panel's vertical
         * midpoint — full panel height is 2x the 60fps budget), with
         * frames over budget highlighted in red. See buildFrame()/
         * rasterFrame() for where each phase is bracketed with start/end
         * timestamps.
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

        // --- frame-scoped pointers (valid only during renderFrame) ---
        campello_gpu::CommandEncoder*              frame_encoder_ = nullptr;
        std::shared_ptr<campello_gpu::TextureView> frame_target_;

        // Native drawable to present after this frame's command buffer (MTLDrawable / CAMetalDrawable).
        void* pending_drawable_ = nullptr;
    };

} // namespace systems::leal::campello_widgets
