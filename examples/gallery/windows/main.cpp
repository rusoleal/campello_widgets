#include "../gallery_app.hpp"
#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/windows/run_app.hpp>
#include <cstdlib>

namespace cw = systems::leal::campello_widgets;

int main()
{
    // Hidden by default — toggle at runtime with Ctrl+D, wired in
    // buildGalleryApp(). Matches gallery/macos/main.mm's env-var-gated
    // tracing convention.
    if (std::getenv("CW_TRACE_RASTER"))
        cw::DebugFlags::printRasterSubPhaseTimings = true;
    if (std::getenv("CW_SHOW_OVERLAY"))
        cw::DebugFlags::showPerformanceOverlay = true;
    if (std::getenv("CW_NO_OVERLAY_TEXT"))
        cw::DebugFlags::performanceOverlayTextEnabled = false;
    return cw::runApp("campello_widgets — Gallery", 1024, 720, cw::buildGalleryApp());
}
