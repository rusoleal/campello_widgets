// Web PlatformWindowing (Phase 17 M4): a "window" is a <canvas> on the same
// page, drawn with the runApp() canvas's WebGPU device through its own
// campello_gpu Surface (GPUCanvasContext). Either an existing canvas named
// by WindowOptions::host_element, or a new one this creates (and removes).

#include "web_canvas.hpp"

#include <campello_widgets/ui/platform_windowing.hpp>
#include <campello_widgets/ui/view.hpp>
#include <campello_widgets/ui/render_box.hpp>

#include <campello_gpu/device.hpp>

#include <emscripten/emscripten.h>

#include <cstdio>
#include <functional>
#include <memory>
#include <string>

extern "C" {
void cw_web_create_canvas(const char* id, const char* title, int w, int h,
                          int has_position, int x, int y, int dialog);
void cw_web_remove_canvas(const char* selector);
void cw_web_show_canvas(const char* selector, const char* parent_selector, int centre);
void cw_web_set_canvas_size(const char* selector, int w, int h);
void cw_web_set_canvas_title(const char* selector, const char* title);
int  cw_web_open_popup(const char* selector, const char* title, int w, int h,
                       int has_position, int x, int y);
int  cw_web_popup_closed(const char* selector);
void cw_web_close_popup(const char* selector);
void cw_web_popup_command(const char* selector, int command, const char* str, int w, int h);
}

namespace GPU = ::systems::leal::campello_gpu;

namespace systems::leal::campello_widgets
{
    void postWebTask(std::function<void()> task);

    namespace
    {
        class WebPlatformWindow final : public PlatformWindow
        {
        public:
            WebPlatformWindow(std::shared_ptr<GPU::Device> device, const WindowOptions& options,
                              std::string parent_selector, Offset parent_origin = {})
                : view_(std::make_unique<View>())
                , parent_selector_(std::move(parent_selector))
                , kind_(options.kind)
            {
                static int next_id = 1;
                if (options.web_popup) {
                    // Another browser window; nothing is created if it's blocked.
                    selector_ = "#cw-popup-" + std::to_string(next_id++);
                    popup_ = true;
                    if (!cw_web_open_popup(selector_.c_str(), options.title.c_str(),
                                           (int)options.size.width, (int)options.size.height,
                                           options.position ? 1 : 0,
                                           options.position ? (int)options.position->x : 0,
                                           options.position ? (int)options.position->y : 0)) {
                        std::fprintf(stderr, "[Web] The browser blocked the popup window "
                                             "(open it from a user gesture)\n");
                        return;
                    }
                    owns_element_ = true;
                } else if (!options.host_element.empty()) {
                    selector_ = options.host_element;
                } else {
                    const std::string id = "cw-window-" + std::to_string(next_id++);
                    // A popup's position is relative to its parent canvas.
                    const bool popup = options.kind == WindowKind::popup;
                    Offset at = options.position ? *options.position : Offset{};
                    if (popup) at = Offset{ parent_origin.x + at.x, parent_origin.y + at.y };
                    cw_web_create_canvas(id.c_str(), options.title.c_str(),
                                         (int)options.size.width, (int)options.size.height,
                                         options.position ? 1 : 0, (int)at.x, (int)at.y,
                                         popup ? 2 : (options.kind == WindowKind::dialog ? 1 : 0));
                    selector_     = "#" + id;
                    owns_element_ = true;
                    centre_       = options.kind == WindowKind::dialog && !options.position;
                }
                canvas_ = std::make_unique<WebCanvas>(std::move(device), selector_, *view_,
                                                      popup_ ? selector_ + "-window" : std::string{});
                canvas_->on_metrics_changed = [this] {
                    auto callback = on_metrics_changed;
                    if (callback) callback();
                };
                if (kind_ == WindowKind::popup) {
                    canvas_->is_popup = true;
                    canvas_->on_outside_press = [this] { requestClose(); };
                }
                if (popup_) {
                    // The user closed the popup: tell the app (it can't veto
                    // it any more; the default handler destroys the window).
                    canvas_->on_frame = [this] {
                        if (close_reported_) return false;
                        if (!cw_web_popup_closed(selector_.c_str())) return true;
                        close_reported_ = true;  // no more frames: its canvas is gone
                        requestClose();
                        return false;
                    };
                }
            }

            ~WebPlatformWindow() override
            {
                // The canvas's listeners are removed while the element still exists.
                canvas_.reset();
                if (!owns_element_) return;
                if (popup_) cw_web_close_popup(selector_.c_str());
                else        cw_web_remove_canvas(selector_.c_str());
            }

            bool ok() const noexcept { return canvas_ && canvas_->ok(); }
            const std::string& selector() const noexcept { return selector_; }

            View&      view() override { return *view_; }
            WindowKind kind() const override { return kind_; }
            WebCanvas* canvas() const noexcept { return canvas_.get(); }

            void setRootRenderBox(std::shared_ptr<RenderBox> root) override
            {
                canvas_->setRoot(std::move(root));
            }

            void setTitle(const std::string& title) override
            {
                if (popup_) return cw_web_popup_command(selector_.c_str(), 1, title.c_str(), 0, 0);
                cw_web_set_canvas_title(selector_.c_str(), title.c_str());
            }

            void setSize(Size logical_size) override
            {
                if (popup_) return cw_web_popup_command(selector_.c_str(), 2, nullptr,
                                                        (int)logical_size.width, (int)logical_size.height);
                cw_web_set_canvas_size(selector_.c_str(), (int)logical_size.width, (int)logical_size.height);
                canvas_->sync();
            }

            WindowMetrics metrics() const override
            {
                WindowMetrics m;
                const float dpr = canvas_->devicePixelRatio();
                m.device_pixel_ratio = dpr;
                m.logical_size = Size{ canvas_->width() / dpr, canvas_->height() / dpr };
                return m;
            }

            void show() override
            {
                if (popup_) {
                    cw_web_popup_command(selector_.c_str(), 0, nullptr, 0, 0);  // focus
                    canvas_->makeKey();
                    return;
                }
                cw_web_show_canvas(selector_.c_str(),
                                   parent_selector_.empty() ? nullptr : parent_selector_.c_str(),
                                   centre_ ? 1 : 0);
                centre_ = false;
                canvas_->makeKey();
                canvas_->sync();  // now laid out: its real size
            }

            void requestClose() override
            {
                // Asynchronous, as a close button's would be: the handler
                // usually destroys this window (WindowController::destroy()).
                std::weak_ptr<int> alive = alive_;
                postWebTask([this, alive] {
                    if (alive.expired()) return;
                    auto callback = on_close_requested;  // the handler may destroy `this`
                    if (callback) callback();
                });
            }

        private:
            std::unique_ptr<View>      view_;  // first: outlives the canvas
            std::string                parent_selector_;
            WindowKind                 kind_ = WindowKind::regular;
            std::string                selector_;
            std::unique_ptr<WebCanvas> canvas_;
            bool                       owns_element_ = false;
            bool                       centre_       = false;
            bool                       popup_        = false;
            bool                       close_reported_ = false;
            std::shared_ptr<int>       alive_ = std::make_shared<int>(0);  // for posted tasks
        };

        class WebPlatformWindowing final : public PlatformWindowing
        {
        public:
            explicit WebPlatformWindowing(std::shared_ptr<GPU::Device> device) : device_(std::move(device)) {}

            std::unique_ptr<PlatformWindow> createWindow(const WindowOptions& options) override
            {
                if (!device_) return nullptr;
                std::string parent;
                Offset      parent_origin;
                if (auto* p = dynamic_cast<WebPlatformWindow*>(options.parent)) {
                    parent        = p->selector();
                    parent_origin = Offset{ p->canvas()->originX(), p->canvas()->originY() };
                } else if (options.kind != WindowKind::regular || options.modal) {
                    // Over the runApp() canvas (the primary View's).
                    WebCanvas* main = WebCanvas::forView(View::primary());
                    if (!main) main = WebCanvas::keyCanvas();
                    if (main) {
                        parent        = main->selector();
                        parent_origin = Offset{ main->originX(), main->originY() };
                    }
                }
                auto window = std::make_unique<WebPlatformWindow>(device_, options, std::move(parent),
                                                                  parent_origin);
                if (!window->ok()) {
                    std::fprintf(stderr, "[Web] Could not open a window on canvas %s\n",
                                 window->selector().c_str());
                    return nullptr;
                }
                return window;
            }

            void postTask(std::function<void()> task) override { postWebTask(std::move(task)); }

        private:
            std::shared_ptr<GPU::Device> device_;
        };
    } // namespace

    void postWebTask(std::function<void()> task)
    {
        if (!task) return;
        // setTimeout(0): after the current DOM event (or frame) has unwound.
        auto* heap = new std::function<void()>(std::move(task));
        emscripten_async_call([](void* arg) {
            std::unique_ptr<std::function<void()>> fn(static_cast<std::function<void()>*>(arg));
            (*fn)();
        }, heap, 0);
    }

    std::unique_ptr<PlatformWindowing> makeWebPlatformWindowing(std::shared_ptr<GPU::Device> device)
    {
        return std::make_unique<WebPlatformWindowing>(std::move(device));
    }
} // namespace systems::leal::campello_widgets
