#include "../gallery_app.hpp"
#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/macos/run_app.hpp>
#include <cstdlib>

namespace cw = systems::leal::campello_widgets;

int main()
{
    cw::DebugFlags::showPerformanceOverlay = true;

    // Hidden by default — toggle at runtime with Cmd+D (Ctrl+D on
    // Windows/Linux), wired in buildGalleryApp().
    if (std::getenv("CW_TRACE_DIRTY"))
        cw::DebugFlags::printDirtyRegionTrace = true;
    if (std::getenv("CW_TRACE_RASTER"))
        cw::DebugFlags::printRasterSubPhaseTimings = true;
    if (std::getenv("CW_TRACE_SCROLL"))
        cw::DebugFlags::printScrollTrace = true;
    return cw::runApp(cw::buildGalleryApp(), "campello_widgets — Gallery", 1024.0f, 720.0f);
}
