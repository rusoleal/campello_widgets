#pragma once

#include <campello_widgets/widgets/widget.hpp>
#include <string>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Microsoft GDK (Gaming.Desktop.x64) entry point — creates a
     * window and runs a present-paced game loop.
     *
     * See TODO.md's Phase 4.5 for the full Xbox/GDK roadmap this is part
     * of. Same widget-tree/rendering plumbing as
     * `campello_widgets/windows/run_app.hpp`'s desktop `runApp()` (HWND +
     * D3D12 swap chain via campello_gpu, PointerDispatcher/FocusManager for
     * input), but differs in the ways Gaming.Desktop.x64 requires:
     *
     *  - Bootstraps/tears down the Gaming Runtime via
     *    `XGameRuntimeInitialize()`/`XGameRuntimeUninitialize()`.
     *  - Registers for suspend/resume via `RegisterAppStateChangeNotification`
     *    instead of relying on desktop `WM_SIZE`/`WM_CLOSE` semantics.
     *  - Runs a continuous, present-paced loop (`PeekMessage` + render every
     *    iteration) instead of the desktop backend's idle `GetMessage`-blocked,
     *    `DwmFlush()`-paced-vsync-thread model — DWM composition doesn't apply
     *    the same way here, and a continuous loop is the normal shape for a
     *    game rather than a windowed desktop app. Pacing comes from
     *    campello_gpu's own `Present(1, 0)` call (already synced to vsync),
     *    not from a separate wait primitive.
     *  - No registry-based OS theme read (`getSystemBrightness()`) or IME
     *    (`imm.h`) support — neither concept applies on this target.
     *
     * Mouse/keyboard input is still translated from Win32 window messages
     * (`WM_MOUSEMOVE`, `WM_KEYDOWN`, ...) exactly as the desktop backend
     * does, **not** yet from campello_input's GDK GameInput layer
     * (`campello_input/src/gdk/`) — see TODO.md 4.5.3's last bullet, left
     * open: that's a separate rework (polling `KeyboardDevice`/`MouseDevice`
     * state each frame and diffing it into synthetic press/release events)
     * that needs its own dedicated pass.
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
     * @brief Extended GDK entry point with additional options.
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
