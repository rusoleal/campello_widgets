#pragma once

#include <campello_widgets/widgets/widget.hpp>
#include <string>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Runs a widget tree in a web page (Emscripten + WebGPU).
     *
     * Renders into the `<canvas>` matching @p canvas_selector, sized to the
     * canvas's CSS box times `devicePixelRatio` (logical pixels are CSS
     * pixels), and sets the page title to @p title.
     *
     * Unlike the desktop runApp(), this returns as soon as the first frame
     * is scheduled: the browser drives the frame loop
     * (requestAnimationFrame) and input (DOM events) afterwards, and the
     * runtime stays alive after main() returns. Returns non-zero if the
     * WebGPU device or the widget tree could not be created.
     *
     * @code
     * int main() {
     *     return runApp("My App", std::make_shared<MyApp>());
     * }
     * @endcode
     */
    int runApp(
        const std::string& title,
        WidgetRef          root_widget,
        const std::string& canvas_selector = "#canvas");

    /// Desktop-compatible overload: @p width and @p height are ignored (the
    /// canvas's CSS size decides).
    int runApp(
        const std::string& title,
        int                width,
        int                height,
        WidgetRef          root_widget);

} // namespace systems::leal::campello_widgets
