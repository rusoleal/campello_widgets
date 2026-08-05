#pragma once

#include <campello_widgets/widgets/widget.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_gpu/device.hpp>
#include <campello_gpu/texture_view.hpp>
#include <campello_gpu/constants/pixel_format.hpp>

#include <cstdint>
#include <functional>
#include <memory>

namespace systems::leal::campello_widgets
{
    class Element;
    class Renderer;
    class PointerDispatcher;
    class FocusManager;
    class TickerScheduler;
    class TextInputManager;

    /**
     * @brief Runs a campello_widgets tree without owning a window, surface,
     * or event loop — for embedding inside a host application that already
     * has its own GPU device and its own render loop (e.g. a Wayland
     * compositor drawing a dashboard/overlay on top of other content).
     *
     * Unlike runApp()/runAppWayland(), EmbeddedApp never creates an
     * X11/Wayland window, never opens its own display connection, and never
     * runs an event loop. The host is responsible for:
     *   - calling renderFrame() whenever it wants the tree redrawn, targeting
     *     whatever TextureView it chooses — it does not have to be a
     *     swapchain image; any render-target-capable TextureView works,
     *     including one wrapping a buffer the host allocated and will
     *     composite into its own scene itself;
     *   - forwarding its own input events into pointerDispatcher()/
     *     focusManager();
     *   - calling tick() periodically (e.g. once per host frame) so
     *     animations/tickers keep advancing even on frames the host chooses
     *     not to redraw.
     *
     * The host must supply a real campello_gpu::Device — typically one it
     * already owns, so the embedded tree's textures live in the same Vulkan
     * device/queue as the host's own rendering (no cross-device import
     * needed). Device::createDefaultDevice(nullptr) creates a device with no
     * window/surface attached at all, exactly what a headless host needs.
     *
     * Like every other platform entry point in this library, EmbeddedApp
     * registers itself as the process's active PointerDispatcher/
     * FocusManager/TickerScheduler/TextInputManager/FrameScheduler callback
     * on construction and clears them on destruction — only one
     * campello_widgets tree (of any kind: desktop window or embedded) may be
     * active in a process at a time. This matches every other platform's
     * runApp(); nothing about embedding relaxes it further.
     */
    class EmbeddedApp
    {
    public:
        /**
         * @param device        GPU device to render with. Must not be null.
         * @param root_widget   Root widget of the tree.
         * @param width         Initial viewport width, in logical pixels.
         * @param height        Initial viewport height, in logical pixels.
         * @param background    Clear color for each frame. Defaults to fully
         *                      transparent — the common case for an overlay
         *                      composited on top of other content, unlike
         *                      every windowed entry point's opaque default.
         * @param pixel_format  Pixel format of the TextureView that will be
         *                      passed to renderFrame(). Must match whatever
         *                      the host's render targets actually use.
         * @param on_redraw_needed  Invoked (on the calling/UI thread) whenever
         *                      the tree becomes dirty and wants a fresh frame
         *                      — e.g. from a running animation or a
         *                      setState() call. Mirrors FrameScheduler's
         *                      contract on every other platform. May be
         *                      empty if the host always redraws every frame
         *                      regardless (e.g. because a game is already
         *                      redrawing every frame anyway).
         */
        EmbeddedApp(
            std::shared_ptr<campello_gpu::Device> device,
            WidgetRef                             root_widget,
            float                                 width,
            float                                 height,
            Color                                 background = Color::transparent(),
            campello_gpu::PixelFormat             pixel_format = campello_gpu::PixelFormat::bgra8unorm,
            std::function<void()>                 on_redraw_needed = {});
        ~EmbeddedApp();

        EmbeddedApp(const EmbeddedApp&) = delete;
        EmbeddedApp& operator=(const EmbeddedApp&) = delete;

        /**
         * @brief Builds and rasters one frame directly into `target`.
         *
         * @return false if nothing changed and no frame was actually drawn
         *         (mirrors Renderer::buildFrame()'s std::nullopt path) — the
         *         host should skip whatever compositing step would display
         *         `target` in that case, since its contents are unchanged
         *         from the previous call. Call forceRefresh() beforehand to
         *         guarantee a frame is drawn regardless (e.g. the first call
         *         after the overlay becomes visible).
         */
        bool renderFrame(
            std::shared_ptr<campello_gpu::TextureView> target,
            float viewport_width,
            float viewport_height);

        /** @brief Forces the next renderFrame() call to draw unconditionally. */
        void forceRefresh();

        /**
         * @brief Advances tickers/animations without drawing a frame.
         *
         * Call this on frames the host doesn't call renderFrame() on, so
         * time-based animations don't stall — mirrors the idle-tick path in
         * runAppWayland()'s event loop (ticked from the poll() timeout there
         * instead of a real frame callback).
         */
        void tick(uint64_t now_ms);

        /** @brief Forward the host's own pointer events here. */
        PointerDispatcher& pointerDispatcher() noexcept { return *dispatcher_; }

        /** @brief Forward the host's own key events here. */
        FocusManager& focusManager() noexcept { return *focus_manager_; }

        /** @brief The underlying Renderer, for advanced configuration (DPR, view insets, etc.). */
        std::shared_ptr<Renderer> renderer() const noexcept { return renderer_; }

    private:
        std::shared_ptr<campello_gpu::Device> device_;
        std::shared_ptr<Element>              root_element_;
        std::shared_ptr<Renderer>             renderer_;
        std::shared_ptr<PointerDispatcher>    dispatcher_;
        std::shared_ptr<FocusManager>         focus_manager_;
        std::unique_ptr<TickerScheduler>      ticker_scheduler_;
        std::unique_ptr<TextInputManager>     text_input_manager_;
    };

} // namespace systems::leal::campello_widgets
