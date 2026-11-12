// macOS PlatformWindowing (Phase 17, multi-window): native windows opened
// through WindowController / the Window widget. Each one is a CampelloWindow
// + CampelloMTKView with its own View, campello_gpu Surface, Renderer and
// raster thread, sharing the main window's GPU device.

#import "macos_window_internal.h"

#import <campello_widgets/ui/platform_windowing.hpp>
#import <campello_widgets/ui/view.hpp>
#import <campello_widgets/ui/renderer.hpp>
#import <campello_widgets/ui/render_box.hpp>
#import <campello_widgets/ui/pointer_dispatcher.hpp>
#import <campello_widgets/ui/color.hpp>

#import <campello_gpu/device.hpp>
#import <campello_gpu/surface.hpp>

#import "../gpu/metal/metal_draw_backend.hpp"

namespace GPU     = ::systems::leal::campello_gpu;
namespace Widgets = ::systems::leal::campello_widgets;

namespace { class MacPlatformWindow; }

/// NSWindowDelegate bridging AppKit window events to a MacPlatformWindow.
/// Holds a raw owner pointer, cleared by the owner's destructor.
@interface CampelloPlatformWindowBridge : NSObject <NSWindowDelegate>
@property (nonatomic, assign) MacPlatformWindow* owner;
@end

namespace
{
    class MacPlatformWindow final : public Widgets::PlatformWindow
    {
    public:
        MacPlatformWindow(std::shared_ptr<GPU::Device> device, const Widgets::WindowOptions& options)
            : device_(std::move(device))
            , view_(std::make_unique<Widgets::View>())
            , kind_(options.kind)
        {
            const bool popup = options.kind == Widgets::WindowKind::popup;
            NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                      NSWindowStyleMaskMiniaturizable;
            if (options.resizable) style |= NSWindowStyleMaskResizable;
            if (popup) style = NSWindowStyleMaskBorderless;

            const NSRect frame = NSMakeRect(0, 0, options.size.width, options.size.height);
            window_ = [[CampelloWindow alloc] initWithContentRect:frame
                                                        styleMask:style
                                                          backing:NSBackingStoreBuffered
                                                            defer:NO];
            window_.releasedWhenClosed = NO;  // we own its lifetime
            window_.title = [NSString stringWithUTF8String:options.title.c_str()];
            window_.widgetsView = view_.get();

            mtk_ = [[CampelloMTKView alloc] initWithFrame:frame device:MTLCreateSystemDefaultDevice()];
            mtk_.colorPixelFormat        = MTLPixelFormatBGRA8Unorm;
            mtk_.depthStencilPixelFormat = MTLPixelFormatInvalid;
            mtk_.autoresizingMask        = NSViewWidthSizable | NSViewHeightSizable;
            // On demand, like the main window: frames only when requested.
            mtk_.paused                = YES;
            mtk_.enableSetNeedsDisplay = YES;
            [mtk_ setWidgetsView:view_.get()];
            window_.contentView = mtk_;
            [window_ setAcceptsMouseMovedEvents:YES];

            surface_ = device_->createSurface((__bridge void*)mtk_.layer);

            __weak CampelloMTKView* weak_mtk = mtk_;
            view_->setFrameRequestCallback([weak_mtk] {
                if (CampelloMTKView* m = weak_mtk) [m setNeedsDisplay:YES];
            });

            bridge_ = [[CampelloPlatformWindowBridge alloc] init];
            bridge_.owner = this;
            window_.delegate = bridge_;

            if (options.kind == Widgets::WindowKind::dialog || popup || options.modal)
            {
                parent_ = parentWindow(options.parent);
                if (parent_) [parent_ addChildWindow:window_ ordered:NSWindowAbove];
            }

            if (popup)
            {
                // A menu-like window: no title bar, above normal windows,
                // takes keys (Escape), moves with its parent (child window).
                window_.acceptsKey = YES;
                window_.level      = NSPopUpMenuWindowLevel;
                window_.hasShadow  = YES;
                if (parent_ && options.position)
                {
                    // `position` is relative to the parent's content top-left.
                    const NSRect content = [parent_ contentRectForFrameRect:parent_.frame];
                    [window_ setFrameTopLeftPoint:NSMakePoint(NSMinX(content) + options.position->x,
                                                              NSMaxY(content) - options.position->y)];
                    positioned_ = true;
                }
            }
            else if (options.position)
            {
                // AppKit's origin is bottom-left of the main screen.
                const CGFloat screen_h = NSScreen.mainScreen.frame.size.height;
                [window_ setFrameTopLeftPoint:NSMakePoint(options.position->x,
                                                          screen_h - options.position->y)];
                positioned_ = true;
            }
        }

        ~MacPlatformWindow() override
        {
            bridge_.owner = nullptr;      // late AppKit callbacks become no-ops
            teardownRenderer();
            view_->setFrameRequestCallback(nullptr);
            [mtk_ setWidgetsView:nullptr];
            window_.widgetsView = nullptr;
            if (parent_) [parent_ removeChildWindow:window_];
            window_.delegate = nil;
            [window_ orderOut:nil];
            [window_ close];
        }

        Widgets::View&      view() override { return *view_; }
        Widgets::WindowKind kind() const override { return kind_; }

        void setRootRenderBox(std::shared_ptr<Widgets::RenderBox> root) override
        {
            teardownRenderer();
            root_ = std::move(root);
            view_->pointerDispatcher().setRoot(root_);
            if (!root_ || !surface_) return;

            const Widgets::Color background = Widgets::Color::white();
            renderer_ = std::make_shared<Widgets::Renderer>(device_, root_, background);
            renderer_->setDrawBackend(std::make_unique<Widgets::MetalDrawBackend>(
                device_, background, surface_->getPixelFormat()));
            renderer_->setDevicePixelRatio(static_cast<float>(window_.backingScaleFactor));
            updateRefreshRate();
            view_->setRenderer(renderer_.get());

            delegate_ = [[CampelloMTKDelegate alloc] initWithDevice:device_
                                                           renderer:renderer_
                                                            surface:surface_];
            mtk_.delegate = delegate_;
            view_->requestFrame();
        }

        void setTitle(const std::string& title) override
        {
            window_.title = [NSString stringWithUTF8String:title.c_str()];
        }

        void setSize(Widgets::Size logical_size) override
        {
            [window_ setContentSize:NSMakeSize(logical_size.width, logical_size.height)];
        }

        Widgets::WindowMetrics metrics() const override
        {
            Widgets::WindowMetrics m;
            const NSSize size = mtk_.bounds.size;
            m.logical_size       = Widgets::Size{(float)size.width, (float)size.height};
            m.device_pixel_ratio = (float)window_.backingScaleFactor;
            return m;
        }

        void show() override
        {
            if (!positioned_)
            {
                if (parent_)
                {
                    // Centre on the parent window.
                    const NSRect p = parent_.frame, w = window_.frame;
                    [window_ setFrameOrigin:NSMakePoint(NSMidX(p) - w.size.width / 2,
                                                        NSMidY(p) - w.size.height / 2)];
                }
                else
                {
                    [window_ center];
                }
                positioned_ = true;
            }
            [window_ makeKeyAndOrderFront:nil];
        }

        void requestClose() override
        {
            // Same path as the close button (asynchronous, see the bridge).
            [bridge_ windowShouldClose:window_];
        }

        // Called by the bridge.
        void fireCloseRequested()
        {
            // Copy first: the handler may destroy `this` (WindowController::destroy()).
            auto callback = on_close_requested;
            if (callback) callback();
        }

        void fireResignedKey()
        {
            // A popup closes when the user goes elsewhere.
            if (kind_ == Widgets::WindowKind::popup) fireCloseRequested();
        }

        void updateRefreshRate()
        {
            const float hz = campelloScreenRefreshHz(window_.screen);
            mtk_.preferredFramesPerSecond = static_cast<NSInteger>(hz);
            if (renderer_) renderer_->setDisplayRefreshHz(hz);
        }

        void fireMetricsChanged()
        {
            if (renderer_) renderer_->setDevicePixelRatio((float)window_.backingScaleFactor);
            auto callback = on_metrics_changed;
            if (callback) callback();
            view_->requestFrame();
        }

    private:
        static NSWindow* parentWindow(Widgets::PlatformWindow* parent)
        {
            if (auto* mac = dynamic_cast<MacPlatformWindow*>(parent)) return mac->window_;
            // e.g. a dialog over the runApp() window
            if (CampelloWindow* main = campelloWindowForView(Widgets::View::primary())) return main;
            return NSApp.mainWindow;
        }

        void teardownRenderer()
        {
            // The raster thread first: it may be rasterizing with the renderer.
            mtk_.delegate = nil;
            [delegate_ stopRasterThread];
            delegate_ = nil;
            view_->setRenderer(nullptr);
            renderer_.reset();
        }

        std::shared_ptr<GPU::Device>        device_;
        std::unique_ptr<Widgets::View>      view_;
        Widgets::WindowKind                 kind_;
        CampelloWindow*                     window_   = nil;
        CampelloMTKView*                    mtk_      = nil;
        CampelloMTKDelegate*                delegate_ = nil;
        CampelloPlatformWindowBridge*       bridge_   = nil;
        NSWindow*                           parent_   = nil;
        std::shared_ptr<GPU::Surface>       surface_;
        std::shared_ptr<Widgets::Renderer>  renderer_;
        std::shared_ptr<Widgets::RenderBox> root_;
        bool                                positioned_ = false;
    };

    class MacPlatformWindowing final : public Widgets::PlatformWindowing
    {
    public:
        explicit MacPlatformWindowing(std::shared_ptr<GPU::Device> device) : device_(std::move(device)) {}

        std::unique_ptr<Widgets::PlatformWindow> createWindow(const Widgets::WindowOptions& options) override
        {
            if (!device_) return nullptr;
            return std::make_unique<MacPlatformWindow>(device_, options);
        }

        void postTask(std::function<void()> task) override
        {
            // Next turn of the main run loop: after the current AppKit event
            // (and our dispatch of it) has fully unwound.
            auto heap_task = std::make_shared<std::function<void()>>(std::move(task));
            dispatch_async(dispatch_get_main_queue(), ^{ if (*heap_task) (*heap_task)(); });
        }

    private:
        std::shared_ptr<GPU::Device> device_;
    };
}

@implementation CampelloPlatformWindowBridge

- (BOOL)windowShouldClose:(NSWindow*)sender
{
    (void)sender;
    // Never close here: the app decides (WindowController::on_close_requested,
    // default destroy()). Deliver it on the next run-loop turn, so destroying
    // the window never happens inside AppKit's own callback for it.
    __weak CampelloPlatformWindowBridge* weak_self = self;
    dispatch_async(dispatch_get_main_queue(), ^{
        CampelloPlatformWindowBridge* strong_self = weak_self;
        if (strong_self && strong_self.owner) strong_self.owner->fireCloseRequested();
    });
    return NO;
}

- (void)windowDidResignKey:(NSNotification*)notification
{
    (void)notification;
    // Asynchronous, like a close request: the popup may be destroyed.
    __weak CampelloPlatformWindowBridge* weak_self = self;
    dispatch_async(dispatch_get_main_queue(), ^{
        CampelloPlatformWindowBridge* strong_self = weak_self;
        if (strong_self && strong_self.owner) strong_self.owner->fireResignedKey();
    });
}

- (void)windowDidChangeScreen:(NSNotification*)notification
{
    (void)notification;
    if (_owner) _owner->updateRefreshRate();
}

- (void)windowDidResize:(NSNotification*)notification
{
    (void)notification;
    if (_owner) _owner->fireMetricsChanged();
}

- (void)windowDidChangeBackingProperties:(NSNotification*)notification
{
    (void)notification;
    if (_owner) _owner->fireMetricsChanged();
}

@end

std::unique_ptr<Widgets::PlatformWindowing>
makeMacPlatformWindowing(std::shared_ptr<GPU::Device> device)
{
    return std::make_unique<MacPlatformWindowing>(std::move(device));
}
