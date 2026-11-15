#pragma once

// One X11 top-level window (Phase 17): an X Window with its own View,
// campello_gpu Surface, Renderer and raster thread (see linux_host.hpp).
// Used by run_app.cpp (the main window) and x11_platform_windowing.cpp
// (windows opened through WindowController). Not public API.

#include "linux_host.hpp"

#include <campello_widgets/ui/platform_windowing.hpp>
#include <campello_widgets/ui/system_mouse_cursor.hpp>
#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/size.hpp>

#include <X11/Xlib.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace systems::leal::campello_widgets
{
    class IbusIme;

    namespace x11
    {
        /// The process's X connection, scale and shared resources. Set up once
        /// by the X11 runner before any window exists.
        void init(Display* display, float display_scale, std::string app_id);
        void shutdown();
        Display* display();
        /// Physical pixels per logical pixel (Xft.dpi / 96), the same for
        /// every window: X11 has one scale per screen.
        float    displayScale();
        /// The screen's refresh rate (XRandR, loaded at run time), 60 if unknown.
        float    refreshRate();
        /// The one IBus input context (may be null or inactive), shared by
        /// every window: the window with keyboard focus feeds it.
        void     setIme(IbusIme* ime);
        IbusIme* ime();

        /// The PlatformWindowing for WindowController (x11_platform_windowing.cpp).
        std::unique_ptr<PlatformWindowing> makePlatformWindowing();

        struct WindowConfig
        {
            std::string           title;
            Size                  size{640.0f, 480.0f};  ///< content size, logical pixels
            /// Top-left in logical pixels: on screen, or relative to the
            /// owner's content for popups. Unset: centred on the owner
            /// (dialogs, popups) or placed by the window manager.
            std::optional<Offset> position;
            WindowKind            kind      = WindowKind::regular;
            ::Window              owner     = 0;   ///< owner window (dialogs, modal windows, popups)
            bool                  modal     = false;
            bool                  resizable = true;
        };

        class X11Window final : public linux_host::LinuxWindow
        {
        public:
            /// Creates the X window (unmapped) and its Surface. `view` must
            /// outlive it. Check xid() and hasSurface() for success.
            X11Window(View& view, const WindowConfig& config);
            ~X11Window() override;

            ::Window   xid() const noexcept { return window_; }
            WindowKind kind() const noexcept { return kind_; }
            ::Window   owner() const noexcept { return owner_; }

            /// Maps the window and, with `activate`, gives it keyboard focus.
            /// The first time, places it (see WindowConfig::position).
            void show(bool activate = true);
            /// Unmaps the window (it keeps its state; show() brings it back).
            void hide();
            /// Raises the window and gives it keyboard focus.
            void activate();
            void setTitle(const std::string& title);
            /// A request: the window manager may adjust it.
            void setLogicalSize(Size size);

            /// Handles one event addressed to this window, inside its View's scope.
            void handleEvent(const XEvent& ev);

            void setCursor(SystemMouseCursor cursor);
            /// Tells IBus where this window's text cursor is (candidate window).
            void updateImeCursorPosition();

            uint32_t physicalWidth() const override  { return width_; }
            uint32_t physicalHeight() const override { return height_; }
            float    devicePixelRatio() const override { return displayScale(); }
            bool     isShown() const override { return mapped_; }
            bool     hasFocus() const noexcept { return focused_; }

            /// Lost keyboard focus to another window (popups close on this).
            std::function<void()> on_focus_lost;

            static X11Window* forXid(::Window window);
            static X11Window* forView(const View* view);
            /// Any mapped window other than `except` (popups excluded)?
            static bool       anyOtherShown(const X11Window* except);

        private:
            void handleKey(const XKeyEvent& key, bool press);
            void place();
            void applySizeHints();
            /// A press or key in a window blocked by a modal one: bring the
            /// modal window forward instead (the core drops the input).
            void redirectToBlocker();
            /// Raises the windows this one owns above it (see show()).
            void raiseOwnedWindows();

            WindowKind kind_      = WindowKind::regular;
            ::Window   window_    = 0;
            ::Window   owner_     = 0;
            uint32_t   width_     = 0;  ///< physical pixels
            uint32_t   height_    = 0;
            bool       resizable_ = true;
            bool       mapped_    = false;
            bool       placed_    = false;
            bool       show_called_ = false;
            bool       pending_show_     = false;  ///< show() waits for the owner's MapNotify
            bool       pending_activate_ = false;
            bool       focused_   = false;
            bool       focus_on_map_ = false;
            bool       had_focus_ = false;  ///< popups: lost focus only counts after having it
            bool       mouse_pressed_ = false;
            std::optional<Offset> position_;
        };
    } // namespace x11
} // namespace systems::leal::campello_widgets
