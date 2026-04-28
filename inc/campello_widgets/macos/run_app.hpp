#pragma once

#include <campello_widgets/widgets/widget.hpp>
#include <campello_gpu/device.hpp>

#include <memory>

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

    /**
     * @brief macOS entry point with an externally-provided GPU device.
     *
     * The caller creates and owns the campello_gpu device; the widget framework
     * will use it for all rendering.  This allows the host application to share
     * the same device (and therefore the same command queue) with other GPU
     * consumers such as 3D viewers or game viewports.
     *
     * @param device       Pre-created campello_gpu device.
     * @param root_widget  Root widget of the application.
     * @param title        Window title string (default: "campello_widgets").
     * @param width        Initial window width in points (default: 800).
     * @param height       Initial window height in points (default: 600).
     * @return NSApp exit code (typically 0).
     */
    int runApp(std::shared_ptr<campello_gpu::Device> device,
               WidgetRef                             root_widget,
               const char*                           title  = "campello_widgets",
               float                                 width  = 800.0f,
               float                                 height = 600.0f);

    /**
     * @brief Request a full refresh and redraw of the widget tree.
     *
     * Forces the entire widget tree to be relaid out and repainted.
     * Call this when toggling debug overlays or other global rendering flags
     * that affect how widgets are displayed.
     */
    void requestRefresh();

} // namespace systems::leal::campello_widgets
