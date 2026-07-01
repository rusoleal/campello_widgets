#include "../gallery_app.hpp"
#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/ios/run_app.hpp>

namespace cw = systems::leal::campello_widgets;

int main(int argc, char* argv[])
{
    cw::DebugFlags::showPerformanceOverlay = true;
    return cw::runApp(argc, argv, cw::buildGalleryApp());
}
