#pragma once

// One <canvas> drawn by campello_widgets (Phase 17 M4): its campello_gpu
// Surface on the shared WebGPU device, its Renderer, its size tracking and
// its input. The runApp() canvas and every canvas opened through
// WindowController (platform_windowing.cpp) are WebCanvases; one
// requestAnimationFrame loop draws them all (WebCanvas::frameAll()) and one
// set of window-level DOM listeners routes moves, releases and keys to the
// right one.

#include <campello_widgets/ui/color.hpp>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace systems::leal::campello_gpu { class Device; class Surface; }

namespace systems::leal::campello_widgets
{
    class View;
    class Renderer;
    class RenderBox;

    class WebCanvas
    {
    public:
        /// Binds to the canvas matching `selector` and registers its input
        /// listeners. ok() is false if there is no such canvas or no surface.
        /// `host_window`: for a canvas in another browser window (a popup),
        /// the emscripten target of that window, whose moves, releases and
        /// keys then go to this canvas; empty for canvases on the page.
        WebCanvas(std::shared_ptr<campello_gpu::Device> device, std::string selector, View& view,
                  std::string host_window = {});
        ~WebCanvas();

        WebCanvas(const WebCanvas&)            = delete;
        WebCanvas& operator=(const WebCanvas&) = delete;

        bool               ok() const noexcept { return surface_ != nullptr; }
        View&              view() const noexcept { return view_; }
        const std::string& selector() const noexcept { return selector_; }

        /// Sets (replaces, clears) the render tree drawn into this canvas.
        void setRoot(std::shared_ptr<RenderBox> root, Color background = Color::white());
        Renderer* renderer() const noexcept { return renderer_.get(); }

        /// Follows the canvas's CSS box and devicePixelRatio; fires
        /// on_metrics_changed if either changed.
        void sync();

        int   width() const noexcept { return width_; }   ///< Physical pixels.
        int   height() const noexcept { return height_; }
        float devicePixelRatio() const noexcept { return dpr_; }

        std::function<void()> on_metrics_changed;
        /// Runs at the start of each of this canvas's frame ticks; returning
        /// false skips the tick (e.g. the canvas's popup has been closed).
        std::function<bool()> on_frame;

        /// Takes keyboard input from now on (the last canvas pressed or shown).
        void makeKey() noexcept;

        // ------------------------------------------------------------------
        // Process-wide
        // ------------------------------------------------------------------

        /// One requestAnimationFrame tick: sync and (if requested) draw every canvas.
        static void frameAll();
        /// The canvas receiving key events; the first one created by default.
        static WebCanvas* keyCanvas() noexcept;
        /// The canvas whose View owns `view`, or nullptr.
        static WebCanvas* forView(const View* view) noexcept;
        /// Installs the window-level listeners (moves, releases, keys, paste)
        /// and the frame loop. Called once by runApp().
        static void installGlobalHandlers();

        // For the DOM callbacks in web_canvas.cpp.
        struct Input;

    private:
        void frame();

        std::shared_ptr<campello_gpu::Device>  device_;
        std::string                            selector_;
        std::string                            host_window_;
        View&                                  view_;
        std::shared_ptr<campello_gpu::Surface> surface_;
        std::shared_ptr<Renderer>              renderer_;
        std::shared_ptr<RenderBox>             root_;

        int   width_  = 1;
        int   height_ = 1;
        float dpr_    = 1.0f;
        float origin_x_ = 0.0f;  ///< Canvas position in the viewport, CSS pixels.
        float origin_y_ = 0.0f;
        bool  needs_redraw_ = true;
        bool  listening_    = false;
    };

} // namespace systems::leal::campello_widgets
