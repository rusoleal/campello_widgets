#pragma once

#include <campello_widgets/widgets/widget.hpp>
#include <string>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Linux entry point — creates an X11 window and runs the event loop.
     *
     * Creates an X11 window backed by campello_gpu's Vulkan backend, initialises
     * the widget renderer, mounts `root_widget` into the element tree, and runs
     * the X11 event loop.
     *
     * Mouse and keyboard events are translated to PointerEvents and KeyEvents,
     * routed through PointerDispatcher and FocusManager respectively.
     *
     * IME (Input Method Editor) is supported via IBus D-Bus integration,
     * enabling composed characters (accents, CJK input, etc.).
     *
     * Usage in `main.cpp`:
     * @code
     * #include <campello_widgets/campello_widgets.hpp>
     * #include <campello_widgets/linux/run_app.hpp>
     *
     * class MyApp : public StatelessWidget {
     *     WidgetRef build(BuildContext&) const override {
     *         return Center::create(Text::create("Hello Linux!"));
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
     * @brief Extended Linux entry point with additional options.
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

    /**
     * @brief Full Linux entry point, including the window's application ID.
     *
     * @p app_id is the identity the window advertises to the compositor/window
     * manager — `xdg_toplevel.set_app_id()` on Wayland (via libdecor), `WM_CLASS`
     * on X11. Desktop shells (GNOME Shell, KDE Plasma, etc.) use it to match the
     * running window to an installed `.desktop` file for the taskbar/alt-tab
     * icon and name; without it, most compositors fall back to a generic
     * "unknown application" icon. It should match the basename of that
     * `.desktop` file, e.g. `"my-app"` for `my-app.desktop`.
     *
     * @param title        Window title bar text (UTF-8).
     * @param width        Initial window width.
     * @param height       Initial window height.
     * @param root_widget  Root widget of the application.
     * @param resizable    Whether the window can be resized.
     * @param app_id       Application ID for compositor/WM identification.
     * @return Exit code.
     */
    int runApp(
        const std::string& title,
        int                width,
        int                height,
        WidgetRef          root_widget,
        bool               resizable,
        const std::string& app_id);

} // namespace systems::leal::campello_widgets
