#ifdef CAMPELLO_WIDGETS_HAS_WAYLAND

// PlatformWindowing for Wayland (Phase 17, multi-window): windows opened
// through WindowController are WaylandWindows (see wayland_window.hpp), each
// with its own View, Surface, Renderer and raster thread on the shared device.

#include "wayland_window.hpp"

#include <campello_widgets/ui/view.hpp>
#include <campello_widgets/ui/render_box.hpp>

namespace systems::leal::campello_widgets::wayland
{
    namespace
    {
        class WaylandPlatformWindow final : public PlatformWindow
        {
        public:
            WaylandPlatformWindow(const WindowOptions& options, WaylandWindow* owner)
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
                window_ = std::make_unique<WaylandWindow>(*view_, config);

                // Close button, and a popup dismissed by the compositor.
                window_->on_close = [this] {
                    auto callback = on_close_requested;  // may destroy `this`
                    if (callback) callback();
                };
                window_->on_metrics_changed = [this] {
                    auto callback = on_metrics_changed;
                    if (callback) callback();
                };
            }

            ~WaylandPlatformWindow() override
            {
                window_.reset();  // before the View it points at
                view_.reset();
            }

            bool           valid() const { return window_ && window_->valid(); }
            WaylandWindow* window() const { return window_.get(); }

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

            void requestClose() override { window_->requestCloseAsync(); }

        private:
            std::unique_ptr<View>          view_;
            WindowKind                     kind_;
            std::unique_ptr<WaylandWindow> window_;
        };

        WaylandWindow* ownerOf(PlatformWindow* parent, const WindowOptions& options)
        {
            if (auto* window = dynamic_cast<WaylandPlatformWindow*>(parent)) return window->window();
            // Dialogs, popups and modal windows without a parent belong to the
            // main (runApp) window.
            const bool needs_owner = options.kind == WindowKind::dialog ||
                                     options.kind == WindowKind::popup || options.modal;
            if (!needs_owner) return nullptr;
            return WaylandWindow::forView(View::primary());
        }

        class WaylandPlatformWindowing final : public PlatformWindowing
        {
        public:
            std::unique_ptr<PlatformWindow> createWindow(const WindowOptions& options) override
            {
                if (!display() || !linux_host::device() || options.kind == WindowKind::external_display)
                    return nullptr;
                WaylandWindow* owner = ownerOf(options.parent, options);
                if (options.kind == WindowKind::popup && !owner) return nullptr;
                auto window = std::make_unique<WaylandPlatformWindow>(options, owner);
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
        return std::make_unique<WaylandPlatformWindowing>();
    }

} // namespace systems::leal::campello_widgets::wayland

#endif // CAMPELLO_WIDGETS_HAS_WAYLAND
