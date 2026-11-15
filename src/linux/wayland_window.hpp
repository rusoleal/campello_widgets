#pragma once

// Wayland windows (Phase 17, multi-window): the process-level Wayland state
// (globals, seat, keyboard map, cursor, clipboard, libdecor) and one
// WaylandWindow per window: a wl_surface with its own View, campello_gpu
// Surface, Renderer and raster thread (see linux_host.hpp). Top-level
// windows and dialogs are libdecor frames; popups are native xdg_popups.
// Used by wayland_runner.cpp and wayland_platform_windowing.cpp. Not public API.

#include "linux_host.hpp"

#include <campello_widgets/ui/platform_windowing.hpp>
#include <campello_widgets/ui/system_mouse_cursor.hpp>
#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/size.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>

struct wl_display;
struct wl_surface;
struct wl_callback;
struct libdecor_frame;
struct libdecor_configuration;
struct xdg_surface;
struct xdg_popup;
struct xdg_dialog_v1;

namespace systems::leal::campello_widgets
{
    class IbusIme;

    namespace wayland
    {
        /// Binds the globals, the seat (pointer, keyboard) and the clipboard,
        /// and creates the libdecor context. False if the compositor lacks
        /// what a window needs (wl_compositor, libdecor).
        bool init(wl_display* display, std::string app_id);
        /// Releases everything init() created. Every window must be gone.
        void shutdown();
        wl_display* display();
        /// Dispatches queued Wayland and libdecor events (non-blocking).
        void dispatch();
        /// Waits up to `timeout_ms` for events and dispatches them. False on
        /// a connection error.
        bool dispatchWait(int timeout_ms);
        /// The scale of the first output (before a window knows its own).
        int  defaultScale();
        /// The one IBus input context (may be null or inactive), shared by
        /// every window: the window with keyboard focus feeds it.
        void     setIme(IbusIme* ime);
        IbusIme* ime();
        /// Loads the cursor theme at `scale` and installs the cursor handler.
        void initCursor(int scale);

        /// The PlatformWindowing for WindowController (wayland_platform_windowing.cpp).
        std::unique_ptr<PlatformWindowing> makePlatformWindowing();

        class WaylandWindow;

        struct WindowConfig
        {
            std::string           title;
            Size                  size{640.0f, 480.0f};  ///< content size, logical pixels
            /// Popups: top-left relative to the owner's content (logical
            /// pixels). Other windows: ignored (Wayland clients can't place
            /// their windows; the compositor centres dialogs on their parent).
            std::optional<Offset> position;
            WindowKind            kind      = WindowKind::regular;
            WaylandWindow*        owner     = nullptr;  ///< dialogs, modal windows, popups
            bool                  modal     = false;
            bool                  resizable = true;
            /// Buffer scale; 0: the owner's, else defaultScale().
            int                   scale     = 0;
        };

        class WaylandWindow final : public linux_host::LinuxWindow
        {
        public:
            /// Creates the wl_surface and its Surface (the window isn't mapped
            /// until show()). `view` must outlive it. Check valid().
            WaylandWindow(View& view, const WindowConfig& config);
            ~WaylandWindow() override;

            bool        valid() const noexcept { return surface_ && hasSurface(); }
            wl_surface* surface() const noexcept { return surface_; }
            WindowKind  kind() const noexcept { return kind_; }
            WaylandWindow* owner() const;

            /// Maps the window: a libdecor frame (top-level, dialog) or an
            /// xdg_popup, drawn once the compositor configures it.
            void show(bool activate = true);
            /// Unmaps the window (show() maps it again).
            void hide();
            void setTitle(const std::string& title);
            /// Top-level windows and dialogs; a popup keeps its size.
            void setLogicalSize(Size size);
            /// Changes the buffer scale before the first frame (see runAppWayland()).
            void setScale(int scale);

            void setCursor(SystemMouseCursor cursor);
            /// Tells IBus where this window's text cursor is (candidate window).
            void updateImeCursorPosition();

            uint32_t physicalWidth() const override;
            uint32_t physicalHeight() const override;
            float    devicePixelRatio() const override { return static_cast<float>(scale_); }
            bool     isShown() const override { return mapped_ && configured_; }
            bool     hasKeyboardFocus() const noexcept { return keyboard_focus_; }
            bool     everConfigured() const noexcept { return ever_configured_; }

            static WaylandWindow* forSurface(const wl_surface* surface);
            static WaylandWindow* forView(const View* view);
            /// Any shown window other than `except` (popups excluded)?
            static bool           anyOtherShown(const WaylandWindow* except);

            // Seat events, routed here by the process-level listeners
            // (wayland_window.cpp), inside this window's View scope.
            void pointerEnter(float x, float y);
            void pointerLeave();
            void pointerMotion(float x, float y);
            void pointerButton(uint32_t button, bool pressed);
            void pointerAxis(bool vertical, float delta);
            void keyboardEnter();
            void keyboardLeave();
            void key(uint32_t keycode, bool pressed);

            // libdecor / xdg callbacks (wayland_window.cpp).
            void onConfigure(libdecor_frame* frame, libdecor_configuration* configuration);
            void onFrameCommit();
            void onPopupConfigure(int width, int height);
            void onXdgSurfaceConfigure(uint32_t serial);
            void onFrameDone();
            void onSurfaceEnter(int output_scale);
            void requestCloseAsync();

        protected:
            bool frameThrottled() const override;
            void willSubmitFrame() override;

        private:
            void createFrame();
            void createPopup();
            void destroyRole();
            void applySize(int logical_w, int logical_h);
            /// Shows owned windows that waited for this one's first configure.
            void showPendingOwnedWindows();
            /// Unmaps the popups this window owns (before its own role goes).
            void hideOwnedPopups();

            WindowKind      kind_      = WindowKind::regular;
            View*           owner_view_ = nullptr;  ///< identifies the owner while it lives
            std::string     title_;
            bool            modal_     = false;
            bool            resizable_ = true;
            std::optional<Offset> position_;
            int             width_     = 0;  ///< logical pixels
            int             height_    = 0;
            int             scale_     = 1;

            wl_surface*     surface_   = nullptr;
            libdecor_frame* frame_     = nullptr;
            xdg_surface*    xdg_surface_ = nullptr;   ///< popups only (libdecor owns the frame's)
            xdg_popup*      popup_     = nullptr;
            xdg_dialog_v1*  dialog_    = nullptr;
            wl_callback*    frame_callback_ = nullptr;
            std::chrono::steady_clock::time_point frame_requested_at_{};

            bool            mapped_     = false;
            bool            configured_ = false;
            bool            ever_configured_ = false;
            bool            paused_     = false;   ///< rendering paused by hide()
            bool            pending_show_ = false; ///< show() waits for the owner's first configure
            bool            keyboard_focus_ = false;
            bool            mouse_pressed_  = false;
            float           pointer_x_  = 0.0f;
            float           pointer_y_  = 0.0f;
        };
    } // namespace wayland
} // namespace systems::leal::campello_widgets
