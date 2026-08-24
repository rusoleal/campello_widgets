#include "linux_display_backend.hpp"

#include <atomic>

namespace systems::leal::campello_widgets
{
    namespace
    {
        std::atomic<bool> g_running_under_wayland{false};
    }

    bool isRunningUnderWayland() noexcept
    {
        return g_running_under_wayland.load();
    }

    void setRunningUnderWayland(bool wayland) noexcept
    {
        g_running_under_wayland.store(wayland);
    }

} // namespace systems::leal::campello_widgets
