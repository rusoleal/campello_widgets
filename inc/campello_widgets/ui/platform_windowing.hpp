#pragma once

#include <campello_widgets/ui/edge_insets.hpp>
#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/size.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace systems::leal::campello_widgets
{
    class View;
    class RenderBox;
    class PlatformWindow;

    /// What kind of native window to create (Phase 17, multi-window).
    enum class WindowKind
    {
        /// An ordinary top-level window.
        regular,
        /// A secondary window that stays above `WindowOptions::parent` and is
        /// centred on it when first shown. Not modal yet: the parent keeps
        /// receiving input (see TODO.md, Phase 17 M2). Where windows can't
        /// be parented (iPadOS scenes) it is an ordinary window.
        dialog,
        /// A window on an external display (a monitor, AirPlay), opened by
        /// the platform when one is connected (iOS/iPadOS). Not created
        /// through createWindow(); non-interactive.
        external_display,
    };

    /// Creation parameters for PlatformWindowing::createWindow().
    struct WindowOptions
    {
        std::string             title = "campello_widgets";
        Size                    size{640.0f, 480.0f};  ///< Content size, logical pixels.
        std::optional<Offset>   position;              ///< Top-left on screen; platform default if unset.
        WindowKind              kind      = WindowKind::regular;
        PlatformWindow*         parent    = nullptr;   ///< Required for WindowKind::dialog.
        bool                    resizable = true;
        /// Web only: CSS selector of an existing `<canvas>` to host the
        /// window (e.g. a sidebar laid out by the page). Empty: a new canvas
        /// is created, see web/run_app.hpp. Ignored on other platforms.
        std::string             host_element;
        /// Web only (experimental): open a separate browser window
        /// (`window.open()`, same origin) drawn with the app's GPU device,
        /// instead of a canvas on the page. Must be created from a user
        /// gesture (a tap handler) or popup blockers refuse it, and then
        /// WindowController::create() returns nullptr. The user closing it
        /// can't be vetoed: on_close_requested runs after the fact.
        bool                    web_popup = false;
    };

    /// A window's current geometry, as the widget tree sees it (MediaQuery).
    struct WindowMetrics
    {
        Size       logical_size{0.0f, 0.0f};
        float      device_pixel_ratio = 1.0f;
        EdgeInsets padding;  ///< Safe-area insets, logical pixels.
    };

    /**
     * @brief One native window (or, on the web, canvas), implemented per
     *        platform.
     *
     * Owns the window's View and everything that draws it (renderer, raster
     * thread, GPU surface). Destroying the object closes the native window.
     * Geometry setters are *requests* the platform may adjust or ignore
     * (e.g. Wayland doesn't let clients position windows).
     *
     * Created through PlatformWindowing::createWindow(); application code
     * normally uses WindowController and the Window widget instead.
     */
    class PlatformWindow
    {
    public:
        virtual ~PlatformWindow() = default;

        /// The window's View (its pointer dispatcher, focus, text input, frame hook).
        virtual View& view() = 0;

        virtual WindowKind kind() const { return WindowKind::regular; }

        /// Sets (or replaces, or clears with nullptr) the root of the
        /// window's render tree. The window lays it out at its size, draws
        /// it, and hit-tests pointer events against it.
        virtual void setRootRenderBox(std::shared_ptr<RenderBox> root) = 0;

        virtual void          setTitle(const std::string& title) = 0;
        virtual void          setSize(Size logical_size) = 0;
        virtual WindowMetrics metrics() const = 0;

        /// Makes the window visible and brings it to the front with key focus.
        virtual void show() = 0;

        /// Asks to close, as if the user had clicked the close button:
        /// fires on_close_requested (it does not destroy the window itself).
        virtual void requestClose() = 0;

        /// The user (or requestClose()) asked to close the window.
        std::function<void()> on_close_requested;
        /// Size, device pixel ratio or safe-area insets changed.
        std::function<void()> on_metrics_changed;
    };

    /**
     * @brief Creates native windows. One implementation per platform,
     *        installed by that platform's runApp().
     *
     * instance() is nullptr on platforms without multi-window support yet;
     * WindowController::create() then returns nullptr.
     */
    class PlatformWindowing
    {
    public:
        virtual ~PlatformWindowing() = default;

        /// Creates (but doesn't show) a window. nullptr on failure.
        virtual std::unique_ptr<PlatformWindow> createWindow(const WindowOptions& options) = 0;

        /**
         * Runs `task` on the UI thread once the current event (or frame) has
         * been handled, e.g. dispatch_async on the main queue.
         * WindowController::destroy() uses it: a window is often closed from
         * one of its own event handlers (a "Close" button), and tearing it
         * down synchronously would destroy the dispatcher and render objects
         * that are still delivering that event. The default runs `task`
         * immediately, for platforms (and tests) without a run loop to post to.
         */
        virtual void postTask(std::function<void()> task) { if (task) task(); }

        static PlatformWindowing* instance() noexcept;
        /// Installs the platform implementation (not owned). Pass nullptr to remove it.
        static void setInstance(PlatformWindowing* windowing) noexcept;
    };

} // namespace systems::leal::campello_widgets
