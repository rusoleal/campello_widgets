#pragma once

namespace systems::leal::campello_widgets
{

    /**
     * @brief True if the Wayland backend (runAppWayland()) is the one
     *        actually driving the app right now; false if X11 is.
     *
     * WAYLAND_DISPLAY being set doesn't guarantee this -- Wayland support
     * might not be compiled in, or runAppWayland()'s own GPU init might
     * have failed and fallen back to X11/XWayland (see run_app.cpp's
     * runApp()). Meaningless (returns false) before runApp() has chosen a
     * backend.
     */
    bool isRunningUnderWayland() noexcept;

    /** @brief Set by runApp() once the backend is actually chosen. Not meant to be called from application code. */
    void setRunningUnderWayland(bool wayland) noexcept;

} // namespace systems::leal::campello_widgets
