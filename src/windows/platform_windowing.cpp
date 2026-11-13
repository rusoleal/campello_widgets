// Windows PlatformWindowing (Phase 17, multi-window): native windows opened
// through WindowController / the Window widget. Each one is a Win32Window
// (an HWND with its own View, campello_gpu Surface and Renderer) sharing the
// main window's GPU device and UI thread.
//
//   regular  WS_OVERLAPPEDWINDOW, unowned unless a parent is given.
//   dialog   captioned, owned by its parent (stays above it, no taskbar
//            button), centred on it.
//   modal    additionally disables its owner (EnableWindow), the Win32 modal
//            idiom: clicks on the owner flash and activate the modal.
//   popup    borderless WS_POPUP tool window with a drop shadow, owned,
//            placed relative to the owner's client area; closes when it
//            loses activation.

#include "win32_host.hpp"

#include <campello_widgets/ui/view.hpp>
#include <campello_widgets/ui/render_box.hpp>

namespace systems::leal::campello_widgets::win32
{
    namespace
    {
        class WinPlatformWindow;

        HWND ownerHwnd(PlatformWindow* parent, const WindowOptions& options);

        class WinPlatformWindow final : public PlatformWindow
        {
        public:
            explicit WinPlatformWindow(const WindowOptions& options)
                : view_(std::make_unique<View>())
                , kind_(options.kind)
            {
                WindowConfig config;
                config.title     = options.title;
                config.size      = options.size;
                config.position  = options.position;
                config.kind      = options.kind;
                config.owner     = ownerHwnd(options.parent, options);
                config.resizable = options.resizable && options.kind != WindowKind::popup;
                window_ = std::make_unique<Win32Window>(*view_, config);

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
                    window_->on_deactivated = [this] {
                        auto callback = on_close_requested;
                        if (callback) callback();
                    };
                }

                if (options.modal && config.owner)
                {
                    disabled_owner_ = config.owner;
                    EnableWindow(disabled_owner_, FALSE);
                }
            }

            ~WinPlatformWindow() override
            {
                // Re-enable the owner first, so Windows hands activation back
                // to it when this window goes away.
                if (disabled_owner_ && IsWindow(disabled_owner_)) EnableWindow(disabled_owner_, TRUE);
                const bool was_active = window_ && GetActiveWindow() == window_->hwnd();
                const HWND owner      = disabled_owner_;
                window_.reset();  // before the View it points at
                if (was_active && owner && IsWindow(owner)) SetActiveWindow(owner);
                view_.reset();
            }

            bool valid() const { return window_ && window_->hwnd() && window_->hasSurface(); }
            HWND hwnd() const { return window_ ? window_->hwnd() : nullptr; }

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
                // Same path as the close button (asynchronous, see WM_CLOSE).
                if (HWND h = hwnd()) PostMessageW(h, WM_CLOSE, 0, 0);
            }

        private:
            std::unique_ptr<View>        view_;
            WindowKind                   kind_;
            std::unique_ptr<Win32Window> window_;
            HWND                         disabled_owner_ = nullptr;
        };

        HWND ownerHwnd(PlatformWindow* parent, const WindowOptions& options)
        {
            if (auto* win = dynamic_cast<WinPlatformWindow*>(parent)) return win->hwnd();
            // Dialogs, popups and modal windows without a parent belong to the
            // main (runApp) window.
            const bool needs_owner = options.kind == WindowKind::dialog ||
                                     options.kind == WindowKind::popup || options.modal;
            if (!needs_owner) return nullptr;
            if (Win32Window* main = Win32Window::forView(View::primary())) return main->hwnd();
            return nullptr;
        }

        class WinPlatformWindowing final : public PlatformWindowing
        {
        public:
            std::unique_ptr<PlatformWindow> createWindow(const WindowOptions& options) override
            {
                if (!device() || options.kind == WindowKind::external_display) return nullptr;
                auto window = std::make_unique<WinPlatformWindow>(options);
                if (!window->valid()) return nullptr;
                return window;
            }

            void postTask(std::function<void()> task) override
            {
                win32::postTask(std::move(task));
            }
        };
    } // namespace

    std::unique_ptr<PlatformWindowing> makePlatformWindowing()
    {
        return std::make_unique<WinPlatformWindowing>();
    }

} // namespace systems::leal::campello_widgets::win32
