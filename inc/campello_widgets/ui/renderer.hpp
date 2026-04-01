#pragma once

#include <memory>
#include <campello_gpu/frame_time_sampler.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/draw_command.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/draw_backend.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/edge_insets.hpp>
#include <campello_widgets/ui/image_filter.hpp>

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
        inline Renderer*& currentRenderer() noexcept {
            static Renderer* instance = nullptr;
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
         * @brief Renders one frame to `target`.
         *
         * Must be called from the render thread. Safe to call every vsync.
         *
         * @param target           Swapchain (or offscreen) texture view to render into.
         * @param viewport_width   Viewport width in logical pixels.
         * @param viewport_height  Viewport height in logical pixels.
         */
        bool renderFrame(
            std::shared_ptr<campello_gpu::TextureView> target,
            float viewport_width,
            float viewport_height);

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

        // ------------------------------------------------------------------
        // BackdropFilter support
        // ------------------------------------------------------------------

        /**
         * @brief Called by RenderBackdropFilter during the layout pass.
         *
         * Tells the Renderer that at least one BackdropFilter exists in the
         * scene this frame, and records the maximum blur sigma so the
         * pre-computed backdrop texture uses the correct blur level.
         */
        void noteBackdropFilter(const ImageFilter& filter) noexcept
        {
            has_backdrop_filter_ = true;
            if (filter.sigma_x > max_sigma_x_) max_sigma_x_ = filter.sigma_x;
            if (filter.sigma_y > max_sigma_y_) max_sigma_y_ = filter.sigma_y;
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

    private:
        void layoutPass(float viewport_width, float viewport_height);

        // Generates the draw list headlessly (no GPU encoder) so it can be
        // replayed to multiple render passes in one frame.
        DrawList generateDrawList(float viewport_width, float viewport_height);

        // Flushes draw commands to `rpe`.
        // backdrop_pass = true  → skips backdrop-filter child commands (capture mode).
        // backdrop_pass = false → handles backdrop-filter commands normally.
        // `rpe` may be replaced when a ShaderMask region is encountered.
        void flushDrawList(
            const DrawList&                                    commands,
            std::shared_ptr<campello_gpu::RenderPassEncoder>& rpe,
            float viewport_width,
            float viewport_height,
            bool  backdrop_pass = false);

        // Applies a ShaderMask region: renders child commands to an offscreen
        // texture, then composites with the gradient mask into the main pass.
        void applyShaderMask(
            const DrawShaderMaskBeginCmd&                      cmd,
            const DrawList&                                    child_cmds,
            std::shared_ptr<campello_gpu::RenderPassEncoder>& rpe,
            float viewport_width,
            float viewport_height,
            const Matrix4& transform,
            const Rect&    clip);

        // Restarts a render pass on frame_target_ with LoadOp::load (preserves
        // existing content).  Used after an offscreen composite operation.
        std::shared_ptr<campello_gpu::RenderPassEncoder> restartMainRenderPass();

        /**
         * @brief Draws the Flutter-style performance overlay.
         */
        void paintPerformanceOverlay(
            PaintContext& ctx,
            float         viewport_width,
            float         viewport_height);

        std::shared_ptr<campello_gpu::Device> device_;
        std::shared_ptr<RenderBox>            root_;
        Color                                 clear_color_;
        std::unique_ptr<IDrawBackend>         draw_backend_;

        // --- performance overlay state ---
        campello_gpu::FrameTimeSampler perf_sampler_;

        // --- view insets (safe area) ---
        EdgeInsets view_insets_;

        // --- device pixel ratio ---
        float device_pixel_ratio_ = 1.0f;

        // --- backdrop filter state (per-frame, reset in layoutPass) ---
        bool  has_backdrop_filter_ = false;
        float max_sigma_x_         = 0.0f;
        float max_sigma_y_         = 0.0f;

        // Offscreen textures for backdrop capture + blur (persistent, resized lazily).
        std::shared_ptr<campello_gpu::Texture> backdrop_tex_;
        std::shared_ptr<campello_gpu::Texture> blurred_backdrop_tex_;
        uint32_t backdrop_tex_w_ = 0;
        uint32_t backdrop_tex_h_ = 0;

        // --- frame-scoped pointers (valid only during renderFrame) ---
        campello_gpu::CommandEncoder*              frame_encoder_ = nullptr;
        std::shared_ptr<campello_gpu::TextureView> frame_target_;
    };

} // namespace systems::leal::campello_widgets
