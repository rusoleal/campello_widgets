#include "gallery_app.hpp"
#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/android/run_app.hpp>
#include <campello_widgets/ui/debug_flags.hpp>
#include <android_native_app_glue.h>

namespace cw = systems::leal::campello_widgets;

void android_main(struct android_app* app)
{
    cw::DebugFlags::showPerformanceOverlay = true;
    cw::runApp(app, cw::buildGalleryApp());
}
