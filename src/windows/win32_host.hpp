#pragma once

// Windows host internals (Phase 17, multi-window): one Win32Window per HWND,
// each with its own View, campello_gpu Surface and Renderer, all driven by
// one UI thread and one shared GPU device. Used by run_app.cpp (the main
// window) and platform_windowing.cpp (windows opened through
// WindowController). Not public API.

#include <campello_widgets/ui/platform_windowing.hpp>
#include <campello_widgets/ui/system_mouse_cursor.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/size.hpp>

#include <windows.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace systems::leal::campello_gpu
{
    class Device;
    class Surface;
}

namespace systems::leal::campello_widgets
{
    class View;
    class Renderer;
    class RenderBox;

    namespace win32
    {
        // ------------------------------------------------------------------
        // App-level host: one per process, created by runApp().
        // ------------------------------------------------------------------

        /// Registers the window class, starts the vsync thread and the task
        /// window. `device` is the one GPU device every window shares.
        bool initHost(std::shared_ptr<campello_gpu::Device> device);
        /// Stops the vsync thread and runs any tasks still queued.
        void shutdownHost();
        std::shared_ptr<campello_gpu::Device> device();

        /// Runs `task` on the UI thread on a later turn of the message loop
        /// (also inside modal loops: window drags, resizes).
        void postTask(std::function<void()> task);
        /// Runs every queued task now (teardown).
        void drainTasks();

        /// Logical pixel ratio of the primary monitor (96 DPI = 1.0).
        float systemScale();

        /// Runs (on a later turn) after any Win32Window is destroyed; the
        /// main window uses it to quit once the last window is gone.
        void setWindowDestroyedHook(std::function<void()> hook);

        /// The PlatformWindowing for WindowController (platform_windowing.cpp).
        std::unique_ptr<PlatformWindowing> makePlatformWindowing();

        // ------------------------------------------------------------------
        // One native window
        // ------------------------------------------------------------------

        struct WindowConfig
        {
            std::string           title;
            Size                  size{640.0f, 480.0f};  ///< client size, logical pixels
            std::optional<Offset> position;              ///< see WindowKind (logical pixels)
            WindowKind            kind      = WindowKind::regular;
            HWND                  owner     = nullptr;   ///< owner window (dialogs, popups)
            bool                  resizable = true;
        };

        class Win32Window
        {
        public:
            /// Creates the HWND (hidden) and its Surface. `view` must outlive it.
            Win32Window(View& view, const WindowConfig& config);
            ~Win32Window();

            Win32Window(const Win32Window&)            = delete;
            Win32Window& operator=(const Win32Window&) = delete;

            HWND       hwnd() const noexcept { return hwnd_; }
            View&      view() noexcept { return view_; }
            WindowKind kind() const noexcept { return kind_; }
            bool       hasSurface() const noexcept { return surface_ != nullptr; }

            /// (Re)creates the renderer for `root` (nullptr: no content).
            void      setRoot(std::shared_ptr<RenderBox> root, Color background = Color::white());
            Renderer* renderer() const noexcept { return renderer_.get(); }

            /// Shows the window; the first time, centres it on its owner
            /// (or the screen) unless it was given a position.
            void show(bool activate = true);
            void setTitle(const std::string& title);
            void setLogicalSize(Size size);

            float devicePixelRatio() const;
            Size  logicalSize() const;   ///< client area, logical pixels

            /// Asks for a frame at the next vsync.
            void requestFrame();
            /// Builds and presents a frame now, if one was requested (or `force`).
            void drawFrame(bool force = false);
            bool needsFrame() const noexcept { return needs_frame_; }

            void setCursor(SystemMouseCursor cursor);

            /// WM_CLOSE (the user's close button, Alt+F4, requestClose()).
            std::function<void()> on_close;
            /// Size or DPI changed.
            std::function<void()> on_metrics_changed;
            /// Lost activation (popups close on this).
            std::function<void()> on_deactivated;
            /// WM_SETTINGCHANGE (e.g. the light/dark app theme).
            std::function<void()> on_settings_changed;

            LRESULT handleMessage(UINT msg, WPARAM wparam, LPARAM lparam);
            /// The window procedure delivers messages while CreateWindowEx is
            /// still running, before the constructor has stored the handle.
            void attachHwnd(HWND hwnd) noexcept { if (!hwnd_) hwnd_ = hwnd; }

            /// Live windows, in creation order.
            static const std::vector<Win32Window*>& all();
            static bool          isLive(const Win32Window* window);
            static Win32Window*  forView(const View* view);
            static Win32Window*  forHwnd(HWND hwnd);

        private:
            bool  scopedInputAllowed(UINT msg);
            void  dispatchPointer(UINT msg, WPARAM wparam, LPARAM lparam);
            void  dispatchKey(UINT msg, WPARAM wparam, LPARAM lparam);
            float refreshHz() const;
            void  updateRefreshRate();

            View&                                 view_;
            WindowKind                            kind_;
            HWND                                  hwnd_  = nullptr;
            HWND                                  owner_ = nullptr;
            std::shared_ptr<campello_gpu::Surface> surface_;
            std::shared_ptr<Renderer>             renderer_;
            std::shared_ptr<RenderBox>            root_;
            bool                                  needs_frame_  = true;
            bool                                  positioned_   = false;
            bool                                  mouse_inside_ = false;
            bool                                  in_frame_     = false;
            HCURSOR                               cursor_       = nullptr;
        };

    } // namespace win32
} // namespace systems::leal::campello_widgets
