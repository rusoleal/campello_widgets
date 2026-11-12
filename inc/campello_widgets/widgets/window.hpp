#pragma once

#include <campello_widgets/ui/platform_windowing.hpp>
#include <campello_widgets/widgets/render_object_widget.hpp>
#include <campello_widgets/widgets/render_object_element.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <vector>
#include <memory>
#include <string>

namespace systems::leal::campello_widgets
{
    class RenderBox;
    class OverlayEntry;

    /**
     * @brief Owns one native window (Phase 17, multi-window).
     *
     * Creating a controller creates (and shows) the native window; its
     * content comes from a `Window` widget placed anywhere in the app's
     * widget tree with this controller. Because every window shares the one
     * element tree and UI thread, state held by a common ancestor (a
     * ValueNotifier, a State) is visible to all windows with no messaging.
     *
     * @code
     * // in some State:
     * inspector_ = WindowController::create({.title = "Inspector", .size = {360, 480}});
     * inspector_->on_destroyed = [this] { setState([&] { inspector_.reset(); }); };
     *
     * // in build():
     * children.push_back(std::make_shared<Window>(inspector_, buildInspector()));
     * @endcode
     *
     * Closing: when the user clicks the close button, on_close_requested runs
     * (default: destroy()). After destroy() the native window is gone; the
     * Window widget, if still in the tree, renders nothing until removed.
     */
    class WindowController : public std::enable_shared_from_this<WindowController>
    {
    public:
        /// Creates and shows a native window. nullptr if this platform has no
        /// PlatformWindowing implementation (yet) or creation failed.
        static std::shared_ptr<WindowController> create(const WindowOptions& options = {});

        /// Wraps an already created platform window (tests, custom embedders).
        static std::shared_ptr<WindowController> adopt(std::unique_ptr<PlatformWindow> window);

        ~WindowController();

        WindowController(const WindowController&)            = delete;
        WindowController& operator=(const WindowController&) = delete;

        /**
         * Windows the platform opens by itself rather than through create():
         * on iPadOS the user can ask for another window of the app (the dock,
         * the app switcher's "+"), and an external display can connect
         * (WindowKind::external_display). The handler adopts the controller:
         * keeps it and hosts a Window widget for it. Without a handler, such
         * windows are closed again (an external display keeps mirroring).
         */
        using PlatformWindowHandler = std::function<void(std::shared_ptr<WindowController>)>;
        static void setPlatformWindowHandler(PlatformWindowHandler handler);

        /// For platform layers: hands a platform-opened window to the
        /// handler. Returns false, leaving `window` untouched, if there is no
        /// handler (the platform then closes it).
        static bool offerPlatformWindow(std::unique_ptr<PlatformWindow>& window);

        bool            isOpen() const noexcept { return window_ != nullptr; }
        /// The kind it was created with (create()), else the platform's.
        WindowKind      kind() const { return kind_ ? *kind_ : (window_ ? window_->kind() : WindowKind::regular); }

        /// The controller whose window owns `view`, or nullptr (e.g. the
        /// main window, which has no controller).
        static WindowController* forView(const View* view) noexcept;

        /// What closing the main (runApp) window does on desktop platforms.
        enum class MainWindowClose
        {
            quit_app,      ///< Closes every window and quits (default).
            keep_running,  ///< Other windows stay; the app quits after the last one.
        };
        static void            setMainWindowClosePolicy(MainWindowClose policy) noexcept;
        static MainWindowClose mainWindowClosePolicy() noexcept;
        PlatformWindow* platformWindow() const noexcept { return window_.get(); }
        View*           view() const noexcept;
        WindowMetrics   metrics() const;

        // Requests; the platform may adjust them. No-ops once destroyed.
        void setTitle(const std::string& title);
        void setSize(Size logical_size);
        void show();
        void requestClose();

        /// Closes the native window: the hosted widget subtree is unmounted
        /// first, then the window (renderer, surface, View) goes, then
        /// on_destroyed runs. Deferred to the next UI-thread turn
        /// (PlatformWindowing::postTask()), so it is safe to call from one of
        /// the window's own event handlers; isOpen() stays true until then.
        void destroy();

        /// The user asked to close the window. Default (unset): destroy().
        std::function<void(WindowController&)> on_close_requested;
        /// After destroy(): e.g. drop the controller and setState() so the
        /// Window widget leaves the tree.
        std::function<void()> on_destroyed;

        // ------------------------------------------------------------------
        // For WindowElement
        // ------------------------------------------------------------------

        enum class Event { metrics_changed, destroying };
        using ListenerId = uint64_t;
        ListenerId addListener(std::function<void(Event)> listener);
        void       removeListener(ListenerId id);

    private:
        explicit WindowController(std::unique_ptr<PlatformWindow> window);
        void notify(Event event);
        void finishDestroy();
        void releaseOwnerLinks();
        bool destroy_pending_ = false;

        std::optional<WindowKind>                          kind_;
        View*                                              blocked_view_ = nullptr;  ///< modal: the parent it blocks
        std::weak_ptr<WindowController>                    owner_;
        std::vector<std::weak_ptr<WindowController>>       owned_;

        std::unique_ptr<PlatformWindow>                    window_;
        std::map<ListenerId, std::function<void(Event)>>   listeners_;
        ListenerId                                         next_listener_ = 1;
    };

    /**
     * @brief Hosts `child` as the content of `controller`'s window.
     *
     * In its parent window it is an empty, zero-size leaf; `child` becomes
     * the root of the other window's render tree, laid out at that window's
     * size, with a MediaQuery describing that window (size, pixel ratio,
     * safe area; other fields inherited from the parent's MediaQuery).
     * Everything under it belongs to that window's View: pointer and key
     * events, focus, text input, frames.
     */
    class Window : public RenderObjectWidget
    {
    public:
        Window() = default;
        Window(std::shared_ptr<WindowController> controller, WidgetRef child)
            : controller(std::move(controller)), child(std::move(child)) {}

        std::shared_ptr<WindowController> controller;
        WidgetRef                         child;

        std::shared_ptr<RenderObject> createRenderObject() const override;
        std::shared_ptr<Element>      createElement() const override;
    };

    /// Element for Window: a leaf in the parent's render tree whose child
    /// subtree lives in another window (see Element::childView()).
    class WindowElement : public RenderObjectElement
    {
    public:
        explicit WindowElement(std::shared_ptr<const Window> widget);

        void mount(Element* parent) override;
        void unmount() override;
        void update(WidgetRef new_widget) override;

        View*    childView() const noexcept override;
        Element* firstChildElement() const noexcept override { return child_.get(); }

        /// A descendant's render object was replaced: re-root the window.
        void onDescendantRenderObjectChanged() override;

    protected:
        void performBuild() override;

    private:
        const Window& windowWidget() const noexcept;
        void subscribe();
        void unsubscribe();
        void syncRoot();
        void dropChild();

        std::shared_ptr<WindowController> controller_;
        WindowController::ListenerId      listener_ = 0;
        std::shared_ptr<Element>          child_;
        RenderBox*                        root_ = nullptr;
        std::shared_ptr<OverlayEntry>     overlay_entry_;  ///< hosts `child` in the window's Overlay
        WidgetRef                         overlay_;
    };

} // namespace systems::leal::campello_widgets
