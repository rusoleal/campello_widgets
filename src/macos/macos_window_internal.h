// Private to the macOS platform layer: the AppKit/MetalKit classes that host
// one campello_widgets window, shared by run_app.mm (the main window) and
// platform_windowing.mm (windows opened through WindowController).
#pragma once

#import <Cocoa/Cocoa.h>
#import <MetalKit/MetalKit.h>

#include <functional>
#include <memory>

namespace systems::leal::campello_gpu
{
    class Device;
    class Surface;
}
namespace systems::leal::campello_widgets
{
    class View;
    class Renderer;
    class PointerDispatcher;
    class FocusManager;
    class TextInputManager;
    class PlatformWindowing;
}

/// The macOS PlatformWindowing (platform_windowing.mm): secondary windows
/// share `device` with the main window.
std::unique_ptr<systems::leal::campello_widgets::PlatformWindowing>
makeMacPlatformWindowing(std::shared_ptr<systems::leal::campello_gpu::Device> device);

/// The MTKView that renders one window and turns AppKit events into
/// campello_widgets pointer, key and text input events for that window.
@interface CampelloMTKView : MTKView <NSTextInputClient>
- (void)setDispatcher:(systems::leal::campello_widgets::PointerDispatcher*)dispatcher;
- (void)setFocusManager:(systems::leal::campello_widgets::FocusManager*)focusManager;
- (void)setTextInputManager:(systems::leal::campello_widgets::TextInputManager*)textInputManager;
- (void)setAppearanceChangedCallback:(std::function<void()>)callback;
/// The window's View: its renderer supplies the safe-area insets used to map
/// event positions. Also sets the dispatcher, focus and text input managers.
- (void)setWidgetsView:(systems::leal::campello_widgets::View*)view;
@end

/// Drives one window's frames: builds on the UI thread (drawInMTKView:) and
/// rasterizes on the window's own RasterThread.
@interface CampelloMTKDelegate : NSObject <MTKViewDelegate>
/// Legacy path: presents MTKView's currentDrawable through the device-wide
/// Device::scheduleNextPresent() slot. Only safe with a single window.
- (instancetype)initWithDevice:(std::shared_ptr<systems::leal::campello_gpu::Device>)device
                      renderer:(std::shared_ptr<systems::leal::campello_widgets::Renderer>)renderer;
/// Per-window path: acquires and presents through `surface` (a
/// campello_gpu::Surface over the MTKView's CAMetalLayer) on the raster
/// thread, so several windows never share a present slot.
- (instancetype)initWithDevice:(std::shared_ptr<systems::leal::campello_gpu::Device>)device
                      renderer:(std::shared_ptr<systems::leal::campello_widgets::Renderer>)renderer
                       surface:(std::shared_ptr<systems::leal::campello_gpu::Surface>)surface;
/// Stops and joins the raster thread. Idempotent.
- (void)stopRasterThread;
@end

/// An NSWindow that runs every event it dispatches inside its View's
/// View::Scope, so code reached from AppKit event handling (key handlers,
/// text input, focus traversal...) resolves to this window.
///
/// While a modal window blocks its View (View::inputBlocker()), clicks and
/// key presses bring that modal window forward (with a beep) instead.
@interface CampelloWindow : NSWindow
@property (nonatomic, assign) systems::leal::campello_widgets::View* widgetsView;
/// Lets a borderless window (a popup) become key, to get keys like Escape.
@property (nonatomic, assign) BOOL acceptsKey;
@end

/// The CampelloWindow showing `view`, if any.
CampelloWindow* campelloWindowForView(const systems::leal::campello_widgets::View* view);

/// The display's refresh rate (60 if unknown).
float campelloScreenRefreshHz(NSScreen* screen);
