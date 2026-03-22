#pragma once

#include <campello_widgets/widgets/widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief macOS entry point — mounts a widget tree and runs the event loop.
     *
     * Creates an NSWindow with an MTKView backed by campello_gpu, initializes the
     * widget renderer with a Metal draw backend, mounts `root_widget` into the
     * element tree, and calls `[NSApp run]`.
     *
     * Usage in `main.mm`:
     * @code
     * #include <campello_widgets/campello_widgets.hpp>
     * #include <campello_widgets/macos/run_app.hpp>
     *
     * class MyApp : public StatelessWidget {
     *     WidgetRef build(BuildContext&) const override {
     *         return Center::create(Text::create("Hello!"));
     *     }
     * };
     *
     * int main() {
     *     return runApp(std::make_shared<MyApp>());
     * }
     * @endcode
     *
     * @param root_widget  Root widget of the application.
     * @param title        Window title string (default: "campello_widgets").
     * @param width        Initial window width in points (default: 800).
     * @param height       Initial window height in points (default: 600).
     * @return NSApp exit code (typically 0).
     */
    int runApp(WidgetRef    root_widget,
               const char*  title  = "campello_widgets",
               float        width  = 800.0f,
               float        height = 600.0f);

} // namespace systems::leal::campello_widgets
