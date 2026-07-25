#include "gallery_app.hpp"
#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/android/run_app.hpp>
#include <campello_widgets/ui/debug_flags.hpp>
#include <android_native_app_glue.h>
#include <sys/system_properties.h>

namespace cw = systems::leal::campello_widgets;

// A shell env var set before `am start` is NOT inherited by the forked app
// process (the standard CW_TRACE_* convention on macOS/Windows/Linux, via
// std::getenv() in each platform's main), since `am start` goes through
// system_server/Zygote rather than a plain shell exec. System properties
// are the standard Android substitute — global state any process can read,
// set via `adb shell setprop debug.cw_trace_raster 1` before relaunching
// the app (properties are read once here at startup, not polled live).
static bool debugPropEnabled(const char* name)
{
    char value[PROP_VALUE_MAX] = {0};
    __system_property_get(name, value);
    return value[0] == '1';
}

void android_main(struct android_app* app)
{
    cw::DebugFlags::showPerformanceOverlay = true;
    if (debugPropEnabled("debug.cw_trace_raster"))
        cw::DebugFlags::printRasterSubPhaseTimings = true;
    if (debugPropEnabled("debug.cw_trace_dirty"))
        cw::DebugFlags::printDirtyRegionTrace = true;
    cw::runApp(app, cw::buildGalleryApp());
}
