#pragma once

// One X11 top-level window (Phase 17): an X Window with its own View,
// campello_gpu Surface, Renderer and raster thread (see linux_host.hpp).
// Used by run_app.cpp's X11 runner. Not public API.

#include "linux_host.hpp"

#include <campello_widgets/ui/system_mouse_cursor.hpp>
#include <campello_widgets/ui/size.hpp>

#include <X11/Xlib.h>

#include <string>

namespace systems::leal::campello_widgets
{
    class IbusIme;

    namespace x11
    {
        /// The process's X connection, scale and shared resources. Set up once
        /// by the X11 runner before any window exists.
        void init(Display* display, float display_scale);
        void shutdown();
        Display* display();
        /// Physical pixels per logical pixel (Xft.dpi / 96), the same for
        /// every window: X11 has one scale per screen.
        float    displayScale();
        /// The one IBus input context (may be null or inactive), shared by
        /// every window: only the window with keyboard focus feeds it keys.
        void     setIme(IbusIme* ime);
        IbusIme* ime();

        struct WindowConfig
        {
            std::string title;
            Size        size{640.0f, 480.0f};  ///< content size, logical pixels
            std::string app_id;                ///< WM_CLASS (matches a .desktop file)
        };

        class X11Window final : public linux_host::LinuxWindow
        {
        public:
            /// Creates the X window (unmapped) and its Surface. `view` must
            /// outlive it. Check hasSurface() for success.
            X11Window(View& view, const WindowConfig& config);
            ~X11Window() override;

            ::Window xid() const noexcept { return window_; }

            /// Maps (shows) the window.
            void show();

            /// Handles one event addressed to this window, inside its View's scope.
            void handleEvent(const XEvent& ev);

            void setCursor(SystemMouseCursor cursor);
            /// Tells IBus where this window's text cursor is (candidate window).
            void updateImeCursorPosition();

            uint32_t physicalWidth() const override  { return width_; }
            uint32_t physicalHeight() const override { return height_; }
            float    devicePixelRatio() const override { return displayScale(); }
            bool     isShown() const override { return mapped_; }

            static X11Window* forXid(::Window window);

        private:
            void handleKey(const XKeyEvent& key, bool press);

            ::Window window_  = 0;
            uint32_t width_   = 0;  ///< physical pixels
            uint32_t height_  = 0;
            bool     mapped_  = false;
            bool     mouse_pressed_ = false;
        };
    } // namespace x11
} // namespace systems::leal::campello_widgets
