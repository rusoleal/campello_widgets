// PlatformWindowing for X11 (Phase 17, multi-window): windows opened through
// WindowController are X11Windows (see x11_window.hpp), each with its own
// View, Surface, Renderer and raster thread on the shared device.

#include "x11_window.hpp"

#include <campello_widgets/ui/view.hpp>
#include <campello_widgets/ui/render_box.hpp>

namespace systems::leal::campello_widgets::x11
{
    namespace
    {
        class X11PlatformWindow final : public PlatformWindow
        {
        public:
            X11PlatformWindow(const WindowOptions& options, ::Window owner)
                : view_(std::make_unique<View>())
                , kind_(options.kind)
            {
                WindowConfig config;
                config.title     = options.title;
                config.size      = options.size;
                config.position  = options.position;
                config.kind      = options.kind;
                config.owner     = owner;
                config.modal     = options.modal;
                config.resizable = options.resizable;
                window_ = std::make_unique<X11Window>(*view_, config);

                window_->on_close = [this] {
                    auto callback = on_close_requested;  // may destroy `this`
                    if (callback) callback();
                };
                window_->on_metrics_changed = [this] {
                    auto callback = on_metrics_changed;
                    if (callback) callback();
                };
                if (kind_ == WindowKind::popup)
                {
                    // A menu closes when the user goes elsewhere.
                    window_->on_focus_lost = [this] {
                        auto callback = on_close_requested;
                        if (callback) callback();
                    };
                }
            }

            ~X11PlatformWindow() override
            {
                window_.reset();  // before the View it points at
                view_.reset();
            }

            bool valid() const { return window_ && window_->xid() && window_->hasSurface(); }
            ::Window xid() const { return window_ ? window_->xid() : 0; }

            View&      view() override { return *view_; }
            WindowKind kind() const override { return kind_; }

            void setRootRenderBox(std::shared_ptr<RenderBox> root) override
            {
                window_->setRoot(std::move(root));
            }

            void setTitle(const std::string& title) override { window_->setTitle(title); }
            void setSize(Size logical_size) override { window_->setLogicalSize(logical_size); }

            WindowMetrics metrics() const override
            {
                WindowMetrics m;
                m.logical_size       = window_->logicalSize();
                m.device_pixel_ratio = window_->devicePixelRatio();
                return m;
            }

            void show() override { window_->show(/*activate=*/true); }

            void requestClose() override
            {
                // Same path as the close button: asynchronous.
                X11Window* window = window_.get();
                linux_host::postTask([window] {
                    if (!linux_host::LinuxWindow::isLive(window)) return;
                    auto callback = window->on_close;
                    if (callback) callback();
                });
            }

        private:
            std::unique_ptr<View>      view_;
            WindowKind                 kind_;
            std::unique_ptr<X11Window> window_;
        };

        ::Window ownerOf(PlatformWindow* parent, const WindowOptions& options)
        {
            if (auto* window = dynamic_cast<X11PlatformWindow*>(parent)) return window->xid();
            // Dialogs, popups and modal windows without a parent belong to the
            // main (runApp) window.
            const bool needs_owner = options.kind == WindowKind::dialog ||
                                     options.kind == WindowKind::popup || options.modal;
            if (!needs_owner) return 0;
            if (X11Window* main = X11Window::forView(View::primary())) return main->xid();
            return 0;
        }

        class X11PlatformWindowing final : public PlatformWindowing
        {
        public:
            std::unique_ptr<PlatformWindow> createWindow(const WindowOptions& options) override
            {
                if (!display() || !linux_host::device() || options.kind == WindowKind::external_display)
                    return nullptr;
                auto window = std::make_unique<X11PlatformWindow>(options, ownerOf(options.parent, options));
                if (!window->valid()) return nullptr;
                return window;
            }

            void postTask(std::function<void()> task) override
            {
                linux_host::postTask(std::move(task));
            }
        };
    } // namespace

    std::unique_ptr<PlatformWindowing> makePlatformWindowing()
    {
        return std::make_unique<X11PlatformWindowing>();
    }

} // namespace systems::leal::campello_widgets::x11
