#pragma once

#include <string>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Wayland-side implementation of Clipboard::setText()/getText(),
     *        called from src/linux/clipboard.cpp when
     *        isRunningUnderWayland() is true.
     *
     * Implemented in wayland_runner.cpp, not clipboard.cpp -- the
     * wl_data_device_manager/wl_data_device/wl_seat object graph and the
     * last input serial (Wayland requires one to claim a selection) only
     * exist inside WaylandWindowState there, which is a purely local
     * stack variable with no accessor exposed elsewhere in this codebase.
     */
    void setWaylandClipboardText(const std::string& text);
    std::string getWaylandClipboardText();

} // namespace systems::leal::campello_widgets
