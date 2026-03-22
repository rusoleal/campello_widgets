#import <campello_widgets/ios/run_app.hpp>
#import <campello_widgets/campello_widgets.hpp>
#import <campello_widgets/widgets/element.hpp>
#import <campello_widgets/widgets/render_object_element.hpp>
#import <campello_widgets/ui/renderer.hpp>
#import <campello_widgets/ui/render_box.hpp>
#import <campello_widgets/ui/pointer_event.hpp>
#import <campello_widgets/ui/pointer_dispatcher.hpp>
#import <campello_widgets/ui/focus_manager.hpp>
#import <campello_widgets/ui/ticker.hpp>

#import <campello_gpu/device.hpp>
#import <campello_gpu/texture_view.hpp>
#import <campello_gpu/constants/pixel_format.hpp>

// MetalDrawBackend is in src/macos/ — on iOS we use the same Metal backend.
// The file is compiled for both macOS and iOS via GLOB_RECURSE in ios.cmake.
#import "../macos/metal_draw_backend.hpp"

#import <UIKit/UIKit.h>
#import <MetalKit/MetalKit.h>

#include <map>

namespace GPU     = systems::leal::campello_gpu;
namespace Widgets = systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
namespace {
    Widgets::WidgetRef gRootWidget;
}

// ---------------------------------------------------------------------------
// CampelloMTKView — MTKView subclass with touch and draw delegate
// ---------------------------------------------------------------------------

@interface CampelloMTKView : MTKView <MTKViewDelegate>
- (instancetype)initWithFrame:(CGRect)frame device:(id<MTLDevice>)device;
- (void)setupWithGPUDevice:(std::shared_ptr<GPU::Device>)gpuDevice
                rootWidget:(Widgets::WidgetRef)rootWidget;
@end

@implementation CampelloMTKView {
    std::shared_ptr<GPU::Device>              _device;
    std::shared_ptr<Widgets::Renderer>        _renderer;
    std::shared_ptr<Widgets::Element>         _rootElement;
    std::shared_ptr<Widgets::PointerDispatcher> _dispatcher;
    std::shared_ptr<Widgets::FocusManager>      _focusManager;
    std::unique_ptr<Widgets::TickerScheduler>   _tickerScheduler;
    Widgets::MetalDrawBackend*                  _backendPtr;

    // Touch → pointer_id mapping (UITouch* identity is stable per gesture)
    std::map<void*, int32_t>  _touchIds;
    int32_t                   _nextPointerId;
}

- (instancetype)initWithFrame:(CGRect)frame device:(id<MTLDevice>)device
{
    if (!(self = [super initWithFrame:frame device:device])) return nil;
    _nextPointerId  = 0;
    self.delegate   = self;
    return self;
}

- (void)setupWithGPUDevice:(std::shared_ptr<GPU::Device>)gpuDevice
                rootWidget:(Widgets::WidgetRef)rootWidget
{
    _device = gpuDevice;

    // Create dispatcher and focus manager before mounting so render
    // objects register during construction.
    _dispatcher = std::make_shared<Widgets::PointerDispatcher>();
    Widgets::PointerDispatcher::setActiveDispatcher(_dispatcher.get());

    _focusManager = std::make_shared<Widgets::FocusManager>();
    Widgets::FocusManager::setActiveManager(_focusManager.get());

    _tickerScheduler = std::make_unique<Widgets::TickerScheduler>();
    Widgets::TickerScheduler::setActive(_tickerScheduler.get());

    // Mount widget tree.
    _rootElement = rootWidget->createElement();
    _rootElement->mount(nullptr);

    auto* roe = _rootElement->findDescendantRenderObjectElement();
    if (!roe) return;

    auto renderBox = std::dynamic_pointer_cast<Widgets::RenderBox>(
        roe->sharedRenderObject());
    if (!renderBox) return;

    _dispatcher->setRoot(renderBox);

    // Create renderer.
    const Widgets::Color     bgColor   = Widgets::Color::white();
    const GPU::PixelFormat   pixelFmt  = GPU::PixelFormat::bgra8unorm;

    auto backendOwned = std::make_unique<Widgets::MetalDrawBackend>(
        _device, bgColor, pixelFmt);
    _backendPtr = backendOwned.get();

    _renderer = std::make_shared<Widgets::Renderer>(_device, renderBox, bgColor);
    _renderer->setDrawBackend(std::move(backendOwned));
}

// ------------------------------------------------------------------
// MTKViewDelegate
// ------------------------------------------------------------------

- (void)drawInMTKView:(MTKView*)view
{
    if (!_renderer) return;

    id<CAMetalDrawable> drawable = view.currentDrawable;
    if (!drawable) return;

    const CGSize sz = view.drawableSize;
    const float  w  = (float)sz.width;
    const float  h  = (float)sz.height;

    if (_backendPtr) _backendPtr->setViewport(w, h);

    auto colorView = GPU::TextureView::fromNative((__bridge void*)drawable.texture);
    if (colorView) _renderer->renderFrame(colorView, w, h);

    [drawable present];
}

- (void)mtkView:(MTKView*)view drawableSizeWillChange:(CGSize)size
{
    (void)view;
    (void)size;
}

// ------------------------------------------------------------------
// Touch → PointerEvent helpers
// ------------------------------------------------------------------

- (int32_t)acquirePointerIdForTouch:(UITouch*)touch
{
    void* key = (__bridge void*)touch;
    auto  it  = _touchIds.find(key);
    if (it != _touchIds.end()) return it->second;
    const int32_t pid = _nextPointerId++;
    _touchIds[key] = pid;
    return pid;
}

- (void)releasePointerIdForTouch:(UITouch*)touch
{
    _touchIds.erase((__bridge void*)touch);
}

- (Widgets::Offset)offsetForTouch:(UITouch*)touch
{
    const CGPoint   pt    = [touch locationInView:self];
    const CGFloat   scale = self.contentScaleFactor;
    return { (float)(pt.x * scale), (float)(pt.y * scale) };
}

- (float)pressureForTouch:(UITouch*)touch
{
    // force is 0 on devices without 3D Touch; treat as 1.0 (fully pressed).
    const float f = (float)touch.force;
    return (f > 0.0f) ? f : 1.0f;
}

// ------------------------------------------------------------------
// UIResponder touch callbacks
// ------------------------------------------------------------------

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    (void)event;
    if (!_dispatcher) return;
    for (UITouch* touch in touches)
    {
        _dispatcher->handlePointerEvent({
            Widgets::PointerEventKind::down,
            [self acquirePointerIdForTouch:touch],
            [self offsetForTouch:touch],
            [self pressureForTouch:touch]});
    }
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    (void)event;
    if (!_dispatcher) return;
    for (UITouch* touch in touches)
    {
        _dispatcher->handlePointerEvent({
            Widgets::PointerEventKind::move,
            [self acquirePointerIdForTouch:touch],
            [self offsetForTouch:touch],
            [self pressureForTouch:touch]});
    }
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    (void)event;
    if (!_dispatcher) return;
    for (UITouch* touch in touches)
    {
        _dispatcher->handlePointerEvent({
            Widgets::PointerEventKind::up,
            [self acquirePointerIdForTouch:touch],
            [self offsetForTouch:touch],
            0.0f});
        [self releasePointerIdForTouch:touch];
    }
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    (void)event;
    if (!_dispatcher) return;
    for (UITouch* touch in touches)
    {
        _dispatcher->handlePointerEvent({
            Widgets::PointerEventKind::cancel,
            [self acquirePointerIdForTouch:touch],
            [self offsetForTouch:touch],
            0.0f});
        [self releasePointerIdForTouch:touch];
    }
}

@end

// ---------------------------------------------------------------------------
// CampelloViewController
// ---------------------------------------------------------------------------

@interface CampelloViewController : UIViewController
@end

@implementation CampelloViewController

- (void)viewDidLoad
{
    [super viewDidLoad];

    id<MTLDevice> mtlDevice = MTLCreateSystemDefaultDevice();
    if (!mtlDevice)
    {
        NSLog(@"campello_widgets: Metal is not supported on this device");
        return;
    }

    CampelloMTKView* metalView =
        [[CampelloMTKView alloc] initWithFrame:self.view.bounds device:mtlDevice];
    metalView.colorPixelFormat         = MTLPixelFormatBGRA8Unorm;
    metalView.depthStencilPixelFormat  = MTLPixelFormatInvalid;
    metalView.clearColor               = MTLClearColorMake(1.0, 1.0, 1.0, 1.0);
    metalView.preferredFramesPerSecond = 60;
    metalView.autoresizingMask         =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

    [self.view addSubview:metalView];

    auto gpuDevice = GPU::Device::createDefaultDevice(nullptr);
    if (!gpuDevice)
    {
        NSLog(@"campello_widgets: failed to create campello_gpu device");
        return;
    }

    [metalView setupWithGPUDevice:gpuDevice rootWidget:gRootWidget];
}

@end

// ---------------------------------------------------------------------------
// CampelloAppDelegate
// ---------------------------------------------------------------------------

@interface CampelloAppDelegate : UIResponder <UIApplicationDelegate>
@property (strong, nonatomic) UIWindow* window;
@end

@implementation CampelloAppDelegate

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions
{
    (void)application;
    (void)launchOptions;

    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    self.window.rootViewController = [[CampelloViewController alloc] init];
    [self.window makeKeyAndVisible];
    return YES;
}

@end

// ---------------------------------------------------------------------------
// runApp()
// ---------------------------------------------------------------------------

namespace systems::leal::campello_widgets
{

int runApp(int argc, char** argv, WidgetRef root_widget)
{
    gRootWidget = std::move(root_widget);

    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil,
            NSStringFromClass([CampelloAppDelegate class]));
    }
}

} // namespace systems::leal::campello_widgets
