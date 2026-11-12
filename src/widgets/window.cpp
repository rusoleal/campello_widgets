#include <campello_widgets/widgets/window.hpp>
#include <campello_widgets/widgets/media_query.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/view.hpp>

namespace systems::leal::campello_widgets
{
    // ======================================================================
    // WindowController
    // ======================================================================

    std::shared_ptr<WindowController> WindowController::create(const WindowOptions& options)
    {
        auto* windowing = PlatformWindowing::instance();
        if (!windowing) return nullptr;
        auto window = windowing->createWindow(options);
        if (!window) return nullptr;
        auto controller = adopt(std::move(window));
        controller->show();
        return controller;
    }

    std::shared_ptr<WindowController> WindowController::adopt(std::unique_ptr<PlatformWindow> window)
    {
        if (!window) return nullptr;
        auto controller = std::shared_ptr<WindowController>(new WindowController(std::move(window)));
        std::weak_ptr<WindowController> weak = controller;
        controller->window_->on_close_requested = [weak] {
            if (auto self = weak.lock())
            {
                if (self->on_close_requested) self->on_close_requested(*self);
                else self->destroy();
            }
        };
        controller->window_->on_metrics_changed = [weak] {
            if (auto self = weak.lock()) self->notify(Event::metrics_changed);
        };
        return controller;
    }

    namespace
    {
        WindowController::PlatformWindowHandler& platformWindowHandler()
        {
            static WindowController::PlatformWindowHandler handler;
            return handler;
        }
    }

    void WindowController::setPlatformWindowHandler(PlatformWindowHandler handler)
    {
        platformWindowHandler() = std::move(handler);
    }

    bool WindowController::offerPlatformWindow(std::unique_ptr<PlatformWindow>& window)
    {
        auto handler = platformWindowHandler();  // a copy: it may replace itself
        if (!window || !handler) return false;
        handler(adopt(std::move(window)));
        return true;
    }

    WindowController::WindowController(std::unique_ptr<PlatformWindow> window)
        : window_(std::move(window))
    {
    }

    WindowController::~WindowController()
    {
        // No on_destroyed here: the owner is going away anyway.
        if (window_)
        {
            notify(Event::destroying);
            window_.reset();
        }
    }

    View* WindowController::view() const noexcept
    {
        return window_ ? &window_->view() : nullptr;
    }

    WindowMetrics WindowController::metrics() const
    {
        return window_ ? window_->metrics() : WindowMetrics{};
    }

    void WindowController::setTitle(const std::string& title)
    {
        if (window_) window_->setTitle(title);
    }

    void WindowController::setSize(Size logical_size)
    {
        if (window_) window_->setSize(logical_size);
    }

    void WindowController::show()
    {
        if (window_) window_->show();
    }

    void WindowController::requestClose()
    {
        if (window_) window_->requestClose();
    }

    void WindowController::destroy()
    {
        if (!window_ || destroy_pending_) return;
        destroy_pending_ = true;
        // Deferred (see PlatformWindowing::postTask()): we may be inside one
        // of this window's own event handlers right now.
        auto self = shared_from_this();  // alive until the task has run
        auto task = [self] { self->finishDestroy(); };
        if (auto* windowing = PlatformWindowing::instance()) windowing->postTask(std::move(task));
        else task();
    }

    void WindowController::finishDestroy()
    {
        destroy_pending_ = false;
        if (!window_) return;
        // Hosted subtree first (its render objects and focus nodes are
        // registered with the window's View), then the window itself.
        notify(Event::destroying);
        window_.reset();
        // Keep ourselves alive through the callback: it typically drops the
        // last reference held by the app's State.
        auto keep_alive = shared_from_this();
        if (on_destroyed) on_destroyed();
    }

    WindowController::ListenerId WindowController::addListener(std::function<void(Event)> listener)
    {
        const ListenerId id = next_listener_++;
        listeners_.emplace(id, std::move(listener));
        return id;
    }

    void WindowController::removeListener(ListenerId id)
    {
        listeners_.erase(id);
    }

    void WindowController::notify(Event event)
    {
        // Snapshot: a listener may remove itself (an element unmounting).
        auto snapshot = listeners_;
        for (auto& [id, listener] : snapshot)
            if (listeners_.count(id) && listener) listener(event);
    }

    // ======================================================================
    // Window
    // ======================================================================

    std::shared_ptr<RenderObject> Window::createRenderObject() const
    {
        // What the *parent* window sees: an empty, zero-size leaf.
        auto placeholder    = std::make_shared<RenderSizedBox>();
        placeholder->width  = 0.0f;
        placeholder->height = 0.0f;
        return placeholder;
    }

    std::shared_ptr<Element> Window::createElement() const
    {
        return std::make_shared<WindowElement>(
            std::static_pointer_cast<const Window>(shared_from_this()));
    }

    // ======================================================================
    // WindowElement
    // ======================================================================

    WindowElement::WindowElement(std::shared_ptr<const Window> widget)
        : RenderObjectElement(widget)
        , controller_(widget->controller)
    {
    }

    const Window& WindowElement::windowWidget() const noexcept
    {
        return static_cast<const Window&>(*widget_);
    }

    View* WindowElement::childView() const noexcept
    {
        // The hosted subtree belongs to the window's View, not ours. Without
        // an open window, fall back to ours (the subtree is dropped anyway).
        if (controller_ && controller_->isOpen()) return controller_->view();
        return view();
    }

    void WindowElement::mount(Element* parent)
    {
        subscribe();
        RenderObjectElement::mount(parent);  // builds (performBuild) the first time
    }

    void WindowElement::unmount()
    {
        dropChild();
        unsubscribe();
        RenderObjectElement::unmount();
    }

    void WindowElement::update(WidgetRef new_widget)
    {
        const auto& next = static_cast<const Window&>(*new_widget);
        if (next.controller != controller_)
        {
            dropChild();  // its render tree belongs to the old window
            unsubscribe();
            controller_ = next.controller;
            subscribe();
        }
        RenderObjectElement::update(std::move(new_widget));
    }

    void WindowElement::subscribe()
    {
        if (!controller_) return;
        listener_ = controller_->addListener([this](WindowController::Event event) {
            switch (event)
            {
            case WindowController::Event::metrics_changed:
                markNeedsBuild();  // new MediaQuery for the hosted subtree
                break;
            case WindowController::Event::destroying:
                dropChild();       // before the window's View goes away
                break;
            }
        });
    }

    void WindowElement::unsubscribe()
    {
        if (controller_ && listener_) controller_->removeListener(listener_);
        listener_ = 0;
    }

    void WindowElement::performBuild()
    {
        if (!controller_ || !controller_->isOpen() || !windowWidget().child)
        {
            dropChild();
            return;
        }

        // Describe *this* window to the hosted subtree; everything the window
        // doesn't define (brightness, ...) comes from the parent's MediaQuery.
        MediaQueryData data;
        if (const MediaQueryData* inherited = MediaQuery::of(*this)) data = *inherited;
        const WindowMetrics m   = controller_->metrics();
        data.logical_size       = m.logical_size;
        data.device_pixel_ratio = m.device_pixel_ratio;
        data.padding            = m.padding;
        data.view_insets        = EdgeInsets::zero();

        auto content = std::make_shared<MediaQuery>(data, windowWidget().child);
        child_ = updateChild(child_, content, this);  // mounted in childView()
        syncRoot();
    }

    void WindowElement::onDescendantRenderObjectChanged()
    {
        // Don't propagate: our own (placeholder) render object didn't change.
        syncRoot();
    }

    void WindowElement::syncRoot()
    {
        if (!controller_ || !controller_->isOpen()) { root_ = nullptr; return; }
        std::shared_ptr<RenderBox> box;
        if (child_)
            if (auto* roe = child_->findDescendantRenderObjectElement())
                box = std::dynamic_pointer_cast<RenderBox>(roe->sharedRenderObject());
        if (box.get() == root_) return;
        root_ = box.get();
        controller_->platformWindow()->setRootRenderBox(std::move(box));
    }

    void WindowElement::dropChild()
    {
        if (child_)
        {
            View::Scope scope(View::isAlive(childView()) ? childView() : View::current());
            child_->unmount();
            child_.reset();
        }
        if (root_ && controller_ && controller_->isOpen())
            controller_->platformWindow()->setRootRenderBox(nullptr);
        root_ = nullptr;
    }

} // namespace systems::leal::campello_widgets
