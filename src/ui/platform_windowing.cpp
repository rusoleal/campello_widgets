#include <campello_widgets/ui/platform_windowing.hpp>

namespace systems::leal::campello_widgets
{
    namespace
    {
        PlatformWindowing* s_windowing = nullptr;  // UI thread only
    }

    PlatformWindowing* PlatformWindowing::instance() noexcept
    {
        return s_windowing;
    }

    void PlatformWindowing::setInstance(PlatformWindowing* windowing) noexcept
    {
        s_windowing = windowing;
    }

} // namespace systems::leal::campello_widgets
