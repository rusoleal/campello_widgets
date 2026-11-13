#pragma once

#include <campello_widgets/widgets/widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief iOS entry point — mounts a widget tree and runs the UIKit event loop.
     *
     * Creates a UIWindow containing an MTKView backed by campello_gpu, initialises
     * the widget renderer with a Metal draw backend, mounts `root_widget` into the
     * element tree, and calls `UIApplicationMain()`.
     *
     * Touch events are translated to PointerEvents and routed through
     * PointerDispatcher with full multitouch support (each UITouch maps to a
     * unique pointer_id).
     *
     * Usage in `main.mm`:
     * @code
     * #include <campello_widgets/campello_widgets.hpp>
     * #include <campello_widgets/ios/run_app.hpp>
     *
     * class MyApp : public StatelessWidget {
     *     WidgetRef build(BuildContext&) const override {
     *         return Center::create(Text::create("Hello!"));
     *     }
     * };
     *
     * int main(int argc, char* argv[]) {
     *     return runApp(argc, argv, std::make_shared<MyApp>());
     * }
     * @endcode
     *
     * Windows (Phase 17): the app runs on the UIScene lifecycle. Declare
     * `UIApplicationSceneManifest` in Info.plist (the scene delegate is
     * configured in code). With `UIApplicationSupportsMultipleScenes` on
     * iPadOS, WindowController / the Window widget open more windows (each
     * a UIWindowScene), and WindowController::setPlatformWindowHandler()
     * receives windows the user opens from the dock and external displays.
     * See examples/ios_multi_window.
     *
     * @param argc         Forwarded to UIApplicationMain.
     * @param argv         Forwarded to UIApplicationMain.
     * @param root_widget  Root widget of the application.
     * @return UIApplicationMain exit code (typically 0).
     */
    int runApp(int       argc,
               char**    argv,
               WidgetRef root_widget);

} // namespace systems::leal::campello_widgets
