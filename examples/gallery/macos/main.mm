#include "../gallery_app.hpp"
#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/macos/run_app.hpp>

namespace cw = systems::leal::campello_widgets;

int main()
{
    cw::DebugFlags::showPerformanceOverlay = true;
    return cw::runApp(cw::buildGalleryApp(), "campello_widgets — Gallery", 1024.0f, 720.0f);
}
