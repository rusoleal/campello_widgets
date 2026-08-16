#include "../gallery_app.hpp"
#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/ios/run_app.hpp>
#import <Foundation/Foundation.h>

namespace cw = systems::leal::campello_widgets;

int main(int argc, char* argv[])
{
    cw::DebugFlags::showPerformanceOverlay = true;

    // Unlike macOS's main.mm (a compile-time absolute host path — this
    // example always runs from the build tree there), the app is
    // sandboxed here: sample_video.mp4 is bundled as an app resource (see
    // CMakeLists.txt's MACOSX_PACKAGE_LOCATION) and must be looked up via
    // NSBundle at runtime instead. See gallery_app.hpp's
    // setSampleVideoPath() doc comment.
    @autoreleasepool {
        NSString* path = [[NSBundle mainBundle] pathForResource:@"sample_video" ofType:@"mp4"];
        if (path)
            cw::setSampleVideoPath(std::string([path UTF8String]));
    }

    return cw::runApp(argc, argv, cw::buildGalleryApp());
}
