#pragma once

#include <campello_widgets/ui/platform_windowing.hpp>
#include <campello_widgets/widgets/render_object_widget.hpp>
#include <campello_widgets/widgets/render_object_element.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace systems::leal::campello_widgets
{
    class RenderBox;

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

        bool            isOpen() const noexcept { return window_ != nullptr; }
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
        bool destroy_pending_ = false;

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
    };

} // namespace systems::leal::campello_widgets
