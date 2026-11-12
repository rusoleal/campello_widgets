// iOS / iPadOS PlatformWindowing (Phase 17 M5): every window is a
// UIWindowScene with a CampelloUIWindow + CampelloViewController hosting
// that window's View, drawn with the app's one GPU device.
//
// Unlike desktop platforms, the app can't create a window synchronously: it
// asks UIKit for a scene session (activateSceneSessionForRequest:) and UIKit
// connects a scene later, to CampelloSceneDelegate. So createWindow()
// returns a *pending* IOSPlatformWindow at once (its View exists, the widget
// subtree can mount); its content appears once the scene connects, matched
// by an id carried in an NSUserActivity (and kept in the session's
// userInfo). Only iPadOS supports several scenes
// (UIApplication.supportsMultipleScenes); on iPhone createWindow() returns
// nullptr, and the only extra windows are external displays.

#import "ios_window_internal.h"

#import <campello_widgets/ui/platform_windowing.hpp>
#import <campello_widgets/ui/view.hpp>
#import <campello_widgets/ui/render_box.hpp>
#import <campello_widgets/widgets/window.hpp>

#import <campello_gpu/device.hpp>

#include <map>
#include <set>
#include <memory>
#include <string>

namespace GPU     = ::systems::leal::campello_gpu;
namespace Widgets = ::systems::leal::campello_widgets;

namespace { class IOSPlatformWindow; }

@interface CampelloSceneDelegate ()
@property (nonatomic, assign) IOSPlatformWindow* owner;  // cleared by the owner
@property (nonatomic, assign) BOOL hostsMain;
@end

namespace
{
    NSString* const kActivityType = @"systems.leal.campello_widgets.window";
    NSString* const kWindowIdKey  = @"cw_window_id";

    std::set<IOSPlatformWindow*>& liveWindows()
    {
        static std::set<IOSPlatformWindow*> windows;
        return windows;
    }

    std::map<std::string, IOSPlatformWindow*>& pendingWindows()
    {
        static std::map<std::string, IOSPlatformWindow*> windows;
        return windows;
    }

    void postMain(std::function<void()> task)
    {
        auto heap = std::make_shared<std::function<void()>>(std::move(task));
        dispatch_async(dispatch_get_main_queue(), ^{ if (*heap) (*heap)(); });
    }

    BOOL isExternalDisplayRole(UISceneSessionRole role)
    {
        if (@available(iOS 16.0, *)) {
            if ([role isEqualToString:UIWindowSceneSessionRoleExternalDisplayNonInteractive]) return YES;
        }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        return [role isEqualToString:UIWindowSceneSessionRoleExternalDisplay];
#pragma clang diagnostic pop
    }

    // Brings an existing session to the front, or (session nil) asks for a
    // new scene carrying `activity`.
    void activateSession(UISceneSession* session, NSUserActivity* activity,
                         void (^onError)(NSError*))
    {
        UIApplication* app = UIApplication.sharedApplication;
        if (@available(iOS 17.0, *)) {
            UISceneSessionActivationRequest* request =
                session ? [UISceneSessionActivationRequest requestWithSession:session]
                        : [UISceneSessionActivationRequest request];
            request.userActivity = activity;
            [app activateSceneSessionForRequest:request errorHandler:onError];
        } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            [app requestSceneSessionActivation:session userActivity:activity options:nil
                                  errorHandler:onError];
#pragma clang diagnostic pop
        }
    }

    class IOSPlatformWindow final : public Widgets::PlatformWindow
    {
    public:
        IOSPlatformWindow(std::shared_ptr<GPU::Device> device, std::string title,
                          Widgets::Size requested_size, Widgets::WindowKind kind)
            : view_(std::make_unique<Widgets::View>())
            , device_(std::move(device))
            , title_(std::move(title))
            , requested_size_(requested_size)
            , kind_(kind)
        {
            static int next_id = 1;
            id_ = "w" + std::to_string(next_id++) + "-" +
                  std::to_string((long long)NSDate.date.timeIntervalSince1970);
            liveWindows().insert(this);
        }

        ~IOSPlatformWindow() override
        {
            liveWindows().erase(this);
            pendingWindows().erase(id_);
            UISceneSession* session = scene_ ? scene_.session : nil;
            teardown();
            // Closing a connected window closes its scene (it leaves the app
            // switcher). An external display just goes back to mirroring.
            if (session && kind_ != Widgets::WindowKind::external_display)
                [UIApplication.sharedApplication requestSceneSessionDestruction:session
                                                                        options:nil
                                                                   errorHandler:^(NSError* e) {
                    NSLog(@"campello_widgets: could not close a window's scene: %@", e);
                }];
        }

        const std::string& windowId() const noexcept { return id_; }

        // Asks UIKit for this window's scene; it connects later (attach()).
        void requestScene()
        {
            pendingWindows()[id_] = this;
            NSUserActivity* activity = [[NSUserActivity alloc] initWithActivityType:kActivityType];
            activity.userInfo = @{ kWindowIdKey : [NSString stringWithUTF8String:id_.c_str()] };
            std::weak_ptr<int> alive = alive_;
            IOSPlatformWindow* owner = this;
            activateSession(nil, activity, ^(NSError* error) {
                NSLog(@"campello_widgets: no scene for a new window: %@", error);
                // Never going to appear: let the app drop it.
                postMain([owner, alive] { if (!alive.expired()) owner->fireCloseRequested(); });
            });
        }

        void attach(UIWindowScene* scene, CampelloSceneDelegate* delegate)
        {
            pendingWindows().erase(id_);
            scene_    = scene;
            delegate_ = delegate;
            delegate.owner = this;

            controller_ = [[CampelloViewController alloc] initWithWidgetsView:view_.get()
                                                                       device:device_];
            std::weak_ptr<int> alive = alive_;
            IOSPlatformWindow* owner = this;
            controller_.onMetricsChanged = ^{
                if (!alive.expired()) owner->fireMetricsChanged();
            };
            [controller_ setRootRenderBox:root_];

            window_ = [[CampelloUIWindow alloc] initWithWindowScene:scene];
            window_.widgetsView        = view_.get();
            window_.rootViewController = controller_;
            delegate.window            = window_;
            scene.title = [NSString stringWithUTF8String:title_.c_str()];
            // Kept with the session: if iPadOS restores it after a relaunch,
            // the id no longer matches a window and the scene is closed.
            scene.session.userInfo = @{ kWindowIdKey : [NSString stringWithUTF8String:id_.c_str()] };
            if (kind_ == Widgets::WindowKind::external_display) window_.hidden = NO;
            else [window_ makeKeyAndVisible];
            fireMetricsChanged();  // the real size is known from now on
        }

        // The scene went away: the user closed the window (or the display
        // was disconnected). The app is told; by default it destroys us.
        void sceneDisconnected()
        {
            teardown();
            fireCloseRequested();
        }

        // --- PlatformWindow
        Widgets::View&      view() override { return *view_; }
        Widgets::WindowKind kind() const override { return kind_; }

        void setRootRenderBox(std::shared_ptr<Widgets::RenderBox> root) override
        {
            root_ = std::move(root);
            if (controller_) [controller_ setRootRenderBox:root_];
        }

        void setTitle(const std::string& title) override
        {
            title_ = title;
            if (scene_) scene_.title = [NSString stringWithUTF8String:title.c_str()];
        }

        // iPadOS decides window sizes (Split View, Stage Manager).
        void setSize(Widgets::Size logical_size) override { requested_size_ = logical_size; }

        Widgets::WindowMetrics metrics() const override
        {
            if (controller_) return [controller_ windowMetrics];
            Widgets::WindowMetrics m;  // not connected yet
            m.logical_size       = requested_size_;
            m.device_pixel_ratio = static_cast<float>(UIScreen.mainScreen.scale);
            return m;
        }

        void show() override
        {
            // Pending: it is shown when its scene connects.
            if (scene_ && kind_ != Widgets::WindowKind::external_display)
                activateSession(scene_.session, nil, nil);
        }

        void requestClose() override
        {
            std::weak_ptr<int> alive = alive_;
            postMain([this, alive] { if (!alive.expired()) fireCloseRequested(); });
        }

    private:
        void fireCloseRequested()
        {
            auto callback = on_close_requested;  // may destroy `this`
            if (callback) callback();
        }

        void fireMetricsChanged()
        {
            auto callback = on_metrics_changed;
            if (callback) callback();
        }

        void teardown()
        {
            if (delegate_) {
                delegate_.owner  = nullptr;
                delegate_.window = nil;
            }
            [controller_ detach];
            window_.rootViewController = nil;
            window_.widgetsView = nullptr;
            window_.hidden = YES;
            controller_ = nil;
            window_     = nil;
            delegate_   = nil;
            scene_      = nil;
        }

        std::unique_ptr<Widgets::View>   view_;  // first: outlives the controller
        std::shared_ptr<GPU::Device>     device_;
        std::string                      id_;
        std::string                      title_;
        Widgets::Size                    requested_size_;
        Widgets::WindowKind              kind_;
        std::shared_ptr<Widgets::RenderBox> root_;
        UIWindowScene*                   scene_      = nil;
        CampelloSceneDelegate*           delegate_   = nil;
        CampelloUIWindow*                window_     = nil;
        CampelloViewController*          controller_ = nil;
        std::shared_ptr<int>             alive_ = std::make_shared<int>(0);  // for posted tasks
    };

    class IOSPlatformWindowing final : public Widgets::PlatformWindowing
    {
    public:
        explicit IOSPlatformWindowing(std::shared_ptr<GPU::Device> device) : device_(std::move(device)) {}

        std::unique_ptr<Widgets::PlatformWindow> createWindow(const Widgets::WindowOptions& options) override
        {
            // iPhone (or no UIApplicationSupportsMultipleScenes): one window only.
            if (!device_ || !UIApplication.sharedApplication.supportsMultipleScenes) return nullptr;
            auto window = std::make_unique<IOSPlatformWindow>(device_, options.title, options.size,
                                                              Widgets::WindowKind::regular);
            window->requestScene();
            return window;
        }

        void postTask(std::function<void()> task) override { postMain(std::move(task)); }

    private:
        std::shared_ptr<GPU::Device> device_;
    };

    // The id of the window this scene was requested for, if any.
    NSString* requestedWindowId(UISceneSession* session, UISceneConnectionOptions* options)
    {
        for (NSUserActivity* activity in options.userActivities) {
            if (![activity.activityType isEqualToString:kActivityType]) continue;
            id value = activity.userInfo[kWindowIdKey];
            if ([value isKindOfClass:NSString.class]) return value;
        }
        id value = session.userInfo[kWindowIdKey];
        return [value isKindOfClass:NSString.class] ? value : nil;
    }
} // namespace

@implementation CampelloSceneDelegate

- (void)scene:(UIScene*)scene
    willConnectToSession:(UISceneSession*)session
                 options:(UISceneConnectionOptions*)connectionOptions
{
    if (![scene isKindOfClass:UIWindowScene.class]) return;
    auto* windowScene = (UIWindowScene*)scene;

    // An external display: offered to the app; unclaimed, it keeps mirroring.
    if (isExternalDisplayRole(session.role)) {
        auto window = std::make_unique<IOSPlatformWindow>(cwIOSDevice(), "External display",
                                                          Widgets::Size{0.0f, 0.0f},
                                                          Widgets::WindowKind::external_display);
        IOSPlatformWindow* raw = window.get();
        std::unique_ptr<Widgets::PlatformWindow> base = std::move(window);
        // (The handler may also have dropped it straight away.)
        if (Widgets::WindowController::offerPlatformWindow(base) && liveWindows().count(raw))
            raw->attach(windowScene, self);
        return;
    }

    // A window the app asked for (WindowController::create()).
    if (NSString* windowId = requestedWindowId(session, connectionOptions)) {
        auto it = pendingWindows().find(windowId.UTF8String);
        if (it != pendingWindows().end()) {
            it->second->attach(windowScene, self);
        } else {
            // Gone meanwhile, or restored from an earlier run: close it.
            [UIApplication.sharedApplication requestSceneSessionDestruction:session
                                                                    options:nil
                                                               errorHandler:nil];
        }
        return;
    }

    // The main window (at launch, or its scene coming back).
    if (!cwIOSMainAttached()) {
        auto* window = [[CampelloUIWindow alloc] initWithWindowScene:windowScene];
        self.window    = window;
        self.hostsMain = YES;
        cwIOSAttachMain(window);
        return;
    }

    // The user asked iPadOS for another window of the app.
    auto window = std::make_unique<IOSPlatformWindow>(cwIOSDevice(), "", Widgets::Size{0.0f, 0.0f},
                                                      Widgets::WindowKind::regular);
    IOSPlatformWindow* raw = window.get();
    std::unique_ptr<Widgets::PlatformWindow> base = std::move(window);
    if (Widgets::WindowController::offerPlatformWindow(base) && liveWindows().count(raw)) {
        raw->attach(windowScene, self);
    } else {
        [UIApplication.sharedApplication requestSceneSessionDestruction:session
                                                                options:nil
                                                           errorHandler:nil];
    }
}

- (void)sceneDidDisconnect:(UIScene*)scene
{
    (void)scene;
    if (self.hostsMain) {
        // The widget tree stays mounted; the next unclaimed scene shows it.
        cwIOSDetachMain();
        self.window    = nil;
        self.hostsMain = NO;
        return;
    }
    if (IOSPlatformWindow* owner = self.owner) owner->sceneDisconnected();
}

@end

std::unique_ptr<Widgets::PlatformWindowing> makeIOSPlatformWindowing(std::shared_ptr<GPU::Device> device)
{
    return std::make_unique<IOSPlatformWindowing>(std::move(device));
}
