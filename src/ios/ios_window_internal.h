// Shared between src/ios/run_app.mm and src/ios/platform_windowing.mm
// (Phase 17 M5, iPadOS multiple scenes). Objective-C++ only.

#pragma once

#import <UIKit/UIKit.h>

#include <campello_widgets/ui/platform_windowing.hpp>

#include <memory>

namespace systems::leal::campello_gpu { class Device; }
namespace systems::leal::campello_widgets { class View; class RenderBox; class Renderer; }

/// A UIWindow whose events (touches, presses) are handled with its View as
/// the ambient one (View::Scope), like CampelloWindow on macOS.
@interface CampelloUIWindow : UIWindow
@property (nonatomic, assign) systems::leal::campello_widgets::View* widgetsView;
@end

/// Hosts one View: a CampelloMTKView drawing the View's render tree with its
/// own Renderer, plus its touch, key and text input.
@interface CampelloViewController : UIViewController
- (instancetype)initWithWidgetsView:(systems::leal::campello_widgets::View*)view
                             device:(std::shared_ptr<systems::leal::campello_gpu::Device>)device;
/// Runs once the view is loaded (sized), before anything is drawn.
@property (nonatomic, copy) void (^onLoaded)(CampelloViewController* controller);
/// Size, safe area or traits (e.g. dark mode) changed.
@property (nonatomic, copy) void (^onMetricsChanged)(void);
/// The render tree to draw (nullptr: none). Creates this window's Renderer.
- (void)setRootRenderBox:(std::shared_ptr<systems::leal::campello_widgets::RenderBox>)root;
- (systems::leal::campello_widgets::WindowMetrics)windowMetrics;
/// Stops drawing and unhooks everything from the View (call before the View goes).
- (void)detach;
@end

/// Scene delegate for every UIWindowScene (configured in code by the app
/// delegate, so apps only need UIApplicationSceneManifest in Info.plist).
@interface CampelloSceneDelegate : UIResponder <UIWindowSceneDelegate>
@property (strong, nonatomic) UIWindow* window;
@end

// --- run_app.mm: the runApp() window ("main"), whose View hosts the tree.
/// Shows the main View in `window` (mounting the widget tree the first time).
void cwIOSAttachMain(CampelloUIWindow* window);
/// The main window's scene went away; the tree stays mounted for the next one.
void cwIOSDetachMain();
bool cwIOSMainAttached();
std::shared_ptr<systems::leal::campello_gpu::Device> cwIOSDevice();

// --- platform_windowing.mm
std::unique_ptr<systems::leal::campello_widgets::PlatformWindowing>
makeIOSPlatformWindowing(std::shared_ptr<systems::leal::campello_gpu::Device> device);
