#pragma once

#include <campello_widgets/widgets/widget.hpp>
#include <string>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Windows entry point — creates a window and runs the message loop.
     *
     * Creates an HWND window with a Direct3D 11/12 swap chain backed by campello_gpu,
     * initialises the widget renderer, mounts `root_widget` into the element tree,
     * and runs the Win32 message loop.
     *
     * Mouse and keyboard events are translated to PointerEvents and KeyEvents,
     * routed through PointerDispatcher and FocusManager respectively.
     *
     * Usage in `main.cpp`:
     * @code
     * #include <campello_widgets/campello_widgets.hpp>
     * #include <campello_widgets/windows/run_app.hpp>
     *
     * class MyApp : public StatelessWidget {
     *     WidgetRef build(BuildContext&) const override {
     *         return Center::create(Text::create("Hello Windows!"));
     *     }
     * };
     *
     * int main() {
     *     return runApp("My App", 1280, 720, std::make_shared<MyApp>());
     * }
     * @endcode
     *
     * @param title       Window title bar text (UTF-8).
     * @param width       Initial window width in logical pixels.
     * @param height      Initial window height in logical pixels.
     * @param root_widget Root widget of the application.
     * @return Exit code (0 on success, non-zero on error).
     */
    int runApp(
        const std::string& title,
        int                width,
        int                height,
        WidgetRef          root_widget);

    /**
     * @brief Extended Windows entry point with additional options.
     *
     * @param title        Window title bar text (UTF-8).
     * @param width        Initial window width.
     * @param height       Initial window height.
     * @param root_widget  Root widget of the application.
     * @param resizable    Whether the window can be resized (default: true).
     * @return Exit code.
     */
    int runApp(
        const std::string& title,
        int                width,
        int                height,
        WidgetRef          root_widget,
        bool               resizable);

} // namespace systems::leal::campello_widgets
