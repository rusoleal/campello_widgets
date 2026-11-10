#include "../gallery_app.hpp"
#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/web/run_app.hpp>

namespace cw = systems::leal::campello_widgets;

int main()
{
    cw::DebugFlags::paintSizeEnabled           = false;
    cw::DebugFlags::showDebugBanner            = false;
    cw::DebugFlags::printRasterSubPhaseTimings = false;
    // Returns once the first frame is scheduled; the browser runs the app.
    return cw::runApp("campello_widgets — Gallery", cw::buildGalleryApp());
}
