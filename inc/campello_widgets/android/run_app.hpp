#pragma once

#include <campello_widgets/widgets/widget.hpp>

#include <cstddef>
#include <functional>

struct ANativeActivity;

namespace systems::leal::campello_widgets
{

    /**
     * @brief Android entry point: hosts the app in a NativeActivity.
     *
     * Call it from your library's `ANativeActivity_onCreate` (the function
     * NativeActivity looks up; no android_native_app_glue). Android calls
     * that once per activity instance: the launcher activity, and one more
     * per extra window opened through WindowController / the Window widget
     * (Phase 17), each in its own task. All of them share one widget tree,
     * run on the Java main thread (callbacks, input, vsync), and draw with
     * one Vulkan device, each through its own surface and raster thread.
     *
     * `make_root` runs once, on the first call, to build the root widget.
     * The tree then lives as long as the process: backgrounding the app or
     * destroying the main activity keeps its state.
     *
     * @code
     * #include <campello_widgets/android/run_app.hpp>
     * #include <android/native_activity.h>
     *
     * extern "C" JNIEXPORT void ANativeActivity_onCreate(
     *     ANativeActivity* activity, void* saved_state, size_t saved_state_size)
     * {
     *     cw::runApp(activity, saved_state, saved_state_size,
     *                [] { return std::make_shared<MyApp>(); });
     * }
     * @endcode
     *
     * Extra windows reuse the activity's own class (android.app.NativeActivity),
     * launched with FLAG_ACTIVITY_NEW_DOCUMENT | MULTIPLE_TASK (plus
     * LAUNCH_ADJACENT when the app is already in multi-window mode): next to
     * the app in split screen, a freeform window in desktop windowing
     * (WindowOptions::position/size become launch bounds), otherwise a
     * full-screen task of its own on top. Closing it in the app
     * finishes the activity and removes its task; the user swiping the task
     * away in Recents closes the window (on_close_requested). Back moves it to
     * the background (Android 12+ behaviour for a task's root activity).
     *
     * Launching the app again while a window's task exists brings that task
     * back (same activity class); the main window reappears once the app is
     * launched with no window tasks left, with the tree as it was.
     */
    void runApp(ANativeActivity* activity, void* saved_state, size_t saved_state_size,
                std::function<WidgetRef()> make_root);

} // namespace systems::leal::campello_widgets
