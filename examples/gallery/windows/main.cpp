#include "../gallery_app.hpp"
#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/windows/run_app.hpp>

namespace cw = systems::leal::campello_widgets;

int main()
{
    // Hidden by default — toggle at runtime with Ctrl+D, wired in
    // buildGalleryApp().
    return cw::runApp("campello_widgets — Gallery", 1024, 720, cw::buildGalleryApp());
}
