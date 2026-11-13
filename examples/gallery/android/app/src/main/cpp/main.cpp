#include "gallery_app.hpp"
#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/android/run_app.hpp>
#include <campello_widgets/ui/debug_flags.hpp>
#include <android/asset_manager.h>
#include <android/native_activity.h>
#include <jni.h>
#include <sys/system_properties.h>
#include <cstdio>
#include <string>

namespace cw = systems::leal::campello_widgets;

// gallery_app.hpp's setSampleVideoPath() takes a plain filesystem path on
// every platform — the app is sandboxed here (no host filesystem access,
// and sample_video.mp4 isn't one on-device to begin with: it's bundled as
// an APK asset, only reachable via AAssetManager), so copy it once into
// internalDataPath (always readable/writable, no permissions needed) and
// hand that real path to setSampleVideoPath() instead. Mirrors how iOS's
// main.mm resolves the same bundled asset via NSBundle to a real path
// rather than teaching VideoPlayerController's Android backend anything
// about AAssetManager.
static std::string copySampleVideoToInternalStorage(ANativeActivity* activity)
{
    const std::string dest_path = std::string(activity->internalDataPath) + "/sample_video.mp4";

    AAsset* asset = AAssetManager_open(activity->assetManager, "sample_video.mp4", AASSET_MODE_STREAMING);
    if (!asset) return {};

    FILE* out = std::fopen(dest_path.c_str(), "wb");
    if (!out)
    {
        AAsset_close(asset);
        return {};
    }

    char buf[64 * 1024];
    int read_bytes = 0;
    while ((read_bytes = AAsset_read(asset, buf, sizeof(buf))) > 0)
        std::fwrite(buf, 1, static_cast<size_t>(read_bytes), out);

    std::fclose(out);
    AAsset_close(asset);
    return dest_path;
}

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

cw::WidgetRef buildMultiWindowDemo();  // multi_window_demo.cpp

// NativeActivity's entry point, once per activity instance: the launcher
// activity and every extra window (see campello_widgets/android/run_app.hpp).
extern "C" JNIEXPORT void ANativeActivity_onCreate(ANativeActivity* activity,
                                                   void* saved_state, size_t saved_state_size)
{
    cw::runApp(activity, saved_state, saved_state_size, [activity]() -> cw::WidgetRef {
        cw::DebugFlags::showPerformanceOverlay = true;
        if (debugPropEnabled("debug.cw_trace_raster"))
            cw::DebugFlags::printRasterSubPhaseTimings = true;
        if (debugPropEnabled("debug.cw_trace_dirty"))
            cw::DebugFlags::printDirtyRegionTrace = true;

        // `adb shell setprop debug.cw_multiwindow 1` (then relaunch): the
        // Phase 17 multi-window demo instead of the gallery.
        if (debugPropEnabled("debug.cw_multiwindow"))
        {
            cw::DebugFlags::showPerformanceOverlay = false;
            return buildMultiWindowDemo();
        }

        const std::string video_path = copySampleVideoToInternalStorage(activity);
        if (!video_path.empty())
            cw::setSampleVideoPath(video_path);
        return cw::buildGalleryApp();
    });
}
