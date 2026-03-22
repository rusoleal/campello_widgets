#pragma once

#include <campello_widgets/widgets/widget.hpp>

// Forward declaration — avoids pulling in android_native_app_glue in public headers.
struct android_app;

namespace systems::leal::campello_widgets
{

    /**
     * @brief Android entry point — mounts a widget tree and runs the event loop.
     *
     * Drives the campello_gpu/campello_widgets render loop inside a native Android
     * activity (GameActivity + android_native_app_glue). Handles:
     *  - APP_CMD_INIT_WINDOW / APP_CMD_TERM_WINDOW lifecycle
     *  - Per-frame rendering via Renderer::renderFrame
     *  - Multitouch input via GameActivityMotionEvent → PointerDispatcher
     *
     * Touch events carry a per-finger `pointer_id` (from GameActivity's pointer
     * index) so GestureDetector correctly handles simultaneous fingers.
     *
     * Usage in your android_main:
     * @code
     * #include <campello_widgets/campello_widgets.hpp>
     * #include <campello_widgets/android/run_app.hpp>
     * #include <game-activity/native_app_glue/android_native_app_glue.h>
     *
     * class MyApp : public StatelessWidget {
     *     WidgetRef build(BuildContext&) const override {
     *         return Center::create(Text::create("Hello!"));
     *     }
     * };
     *
     * void android_main(struct android_app* app) {
     *     systems::leal::campello_widgets::runApp(app, std::make_shared<MyApp>());
     * }
     * @endcode
     *
     * @param app         The android_app struct from android_native_app_glue.
     * @param root_widget Root widget of the application.
     */
    void runApp(struct android_app* app, WidgetRef root_widget);

} // namespace systems::leal::campello_widgets
