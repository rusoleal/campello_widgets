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
#import <campello_widgets/ui/frame_scheduler.hpp>
#import <campello_widgets/ui/text_input_manager.hpp>

#include <chrono>

#import <campello_gpu/device.hpp>
#import <campello_gpu/texture_view.hpp>
#import <campello_gpu/constants/pixel_format.hpp>

// MetalDrawBackend is in src/macos/ — on iOS we use the same Metal backend.
// The file is compiled for both macOS and iOS via GLOB_RECURSE in ios.cmake.
#import "../macos/metal_draw_backend.hpp"

#import <UIKit/UIKit.h>
#import <MetalKit/MetalKit.h>

#include <map>
#include <algorithm>

// Namespace aliases - using global qualification to work correctly in Unity Build
namespace GPU     = ::systems::leal::campello_gpu;
namespace Widgets = ::systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
namespace {
    Widgets::WidgetRef gRootWidget;
}

// ---------------------------------------------------------------------------
// UITextInput helpers
// ---------------------------------------------------------------------------

@interface CampelloTextPosition : UITextPosition
@property (nonatomic, assign) NSInteger index;
+ (instancetype)positionWithIndex:(NSInteger)index;
@end

@implementation CampelloTextPosition
+ (instancetype)positionWithIndex:(NSInteger)index
{
    CampelloTextPosition* pos = [[self alloc] init];
    pos.index = index;
    return pos;
}
@end

@interface CampelloTextRange : UITextRange
@property (nonatomic, assign) NSRange range;
+ (instancetype)rangeWithNSRange:(NSRange)range;
@end

@implementation CampelloTextRange
+ (instancetype)rangeWithNSRange:(NSRange)range
{
    CampelloTextRange* r = [[self alloc] init];
    r.range = range;
    return r;
}
- (UITextPosition*)start   { return [CampelloTextPosition positionWithIndex:self.range.location]; }
- (UITextPosition*)end     { return [CampelloTextPosition positionWithIndex:NSMaxRange(self.range)]; }
- (BOOL)isEmpty            { return self.range.length == 0; }
@end

// ---------------------------------------------------------------------------
// CampelloMTKView — MTKView subclass with touch, draw delegate, and UITextInput
// ---------------------------------------------------------------------------

@interface CampelloMTKView : MTKView <MTKViewDelegate, UITextInput>
- (instancetype)initWithFrame:(CGRect)frame device:(id<MTLDevice>)device;
- (std::shared_ptr<Widgets::Renderer>)setupWithGPUDevice:(std::shared_ptr<GPU::Device>)gpuDevice
                rootWidget:(Widgets::WidgetRef)rootWidget;
@end

@implementation CampelloMTKView {
    std::shared_ptr<GPU::Device>              _device;
    std::shared_ptr<Widgets::Renderer>        _renderer;
    std::shared_ptr<Widgets::Element>         _rootElement;
    std::shared_ptr<Widgets::PointerDispatcher> _dispatcher;
    std::shared_ptr<Widgets::FocusManager>      _focusManager;
    std::unique_ptr<Widgets::TickerScheduler>   _tickerScheduler;
    std::unique_ptr<Widgets::TextInputManager>  _textInputManager;
    Widgets::MetalDrawBackend*                  _backendPtr;

    // Touch → pointer_id mapping (UITouch* identity is stable per gesture)
    std::map<void*, int32_t>  _touchIds;
    int32_t                   _nextPointerId;

    id<UITextInputTokenizer>  _tokenizer;
    __weak id<UITextInputDelegate> _inputDelegate;
}

@synthesize inputDelegate = _inputDelegate;

- (instancetype)initWithFrame:(CGRect)frame device:(id<MTLDevice>)device
{
    if (!(self = [super initWithFrame:frame device:device])) return nil;
    _nextPointerId  = 0;
    self.delegate   = self;
    return self;
}

- (BOOL)canBecomeFirstResponder
{
    return YES;
}

- (std::shared_ptr<Widgets::Renderer>)setupWithGPUDevice:(std::shared_ptr<GPU::Device>)gpuDevice
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

    _textInputManager = std::make_unique<Widgets::TextInputManager>();
    Widgets::TextInputManager::setActiveManager(_textInputManager.get());

    // Show/hide the software keyboard when a TextField gains or loses focus.
    __weak CampelloMTKView* weakSelf = self;
    _textInputManager->setOnInputTargetChanged([weakSelf](bool has_target) {
        if (CampelloMTKView* strongSelf = weakSelf) {
            if (has_target && !strongSelf.isFirstResponder)
                [strongSelf becomeFirstResponder];
            else if (!has_target && strongSelf.isFirstResponder)
                [strongSelf resignFirstResponder];
        }
    });

    // On-demand rendering: stop the continuous display link.
    // Register the callback before mounting so initial markNeedsPaint() calls
    // during tree construction already reach the view.
    self.paused               = YES;
    self.enableSetNeedsDisplay = YES;
    Widgets::FrameScheduler::setCallback([weakSelf] {
        if (weakSelf) [weakSelf setNeedsDisplay];
    });

    // Wrap root widget with MediaQuery
    Widgets::MediaQueryData mediaData;
    mediaData.device_pixel_ratio = static_cast<float>(self.contentScaleFactor);
    
    auto wrappedRoot = Widgets::mw<Widgets::MediaQuery>(
        mediaData, rootWidget);

    // Mount widget tree.
    _rootElement = wrappedRoot->createElement();
    _rootElement->mount(nullptr);

    auto* roe = _rootElement->findDescendantRenderObjectElement();
    if (!roe) return nullptr;

    auto renderBox = std::dynamic_pointer_cast<Widgets::RenderBox>(
        roe->sharedRenderObject());
    if (!renderBox) return nullptr;

    _dispatcher->setRoot(renderBox);

    // Create renderer.
    const Widgets::Color     bgColor   = Widgets::Color::white();
    const GPU::PixelFormat   pixelFmt  = GPU::PixelFormat::bgra8unorm;

    auto backendOwned = std::make_unique<Widgets::MetalDrawBackend>(
        _device, bgColor, pixelFmt);
    _backendPtr = backendOwned.get();

    _renderer = std::make_shared<Widgets::Renderer>(_device, renderBox, bgColor);
    _renderer->setDrawBackend(std::move(backendOwned));
    
    return _renderer;
}

// ------------------------------------------------------------------
// MTKViewDelegate
// ------------------------------------------------------------------

- (void)drawInMTKView:(MTKView*)view
{
    if (!_renderer) return;

    id<CAMetalDrawable> drawable = view.currentDrawable;
    if (!drawable) return;

    CGFloat scale = self.contentScaleFactor;
    CGRect bounds = self.bounds;
    CGSize drawableSize = view.drawableSize;

    // The Renderer expects LOGICAL viewport dimensions (in points).
    // It internally divides by DPR to get logical constraints for layout.
    // The Metal drawable and backend need PHYSICAL dimensions (in pixels).
    float logical_width  = (float)bounds.size.width;
    float logical_height = (float)bounds.size.height;
    float physical_width = (float)drawableSize.width;
    float physical_height= (float)drawableSize.height;

    _renderer->setDevicePixelRatio(static_cast<float>(scale));
    if (_backendPtr) _backendPtr->setViewport(physical_width, physical_height);

    // Tie presentation to GPU completion + vsync via presentDrawable: on the
    // command buffer rather than calling [drawable present] on the CPU.
    _device->scheduleNextPresent((__bridge void*)drawable);

    auto now = std::chrono::steady_clock::now();
    uint64_t now_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count());
    if (auto* d  = Widgets::PointerDispatcher::activeDispatcher()) d->tick(now_ms);
    if (auto* ts = Widgets::TickerScheduler::active())            ts->tick(now_ms);

    auto colorView = GPU::TextureView::fromNative((__bridge void*)drawable.texture);
    bool rendered = colorView && _renderer->renderFrame(colorView, logical_width, logical_height);
    if (!rendered)
        _device->scheduleNextPresent(nullptr);
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
    // Return coordinates in logical pixels (points), not physical pixels.
    // The Renderer converts to physical pixels internally using DPR.
    const CGPoint pt = [touch locationInView:self];
    return { (float)pt.x, (float)pt.y };
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

// ============================================================================
// UIKeyInput
// ============================================================================

- (BOOL)hasText
{
    auto* controller = _textInputManager->activeController();
    return controller && !controller->text().empty();
}

- (void)insertText:(NSString*)text
{
    if (!_textInputManager) return;

    // Filter standalone dead keys (e.g. Bluetooth keyboard accents)
    if (text.length == 1 && !_textInputManager->isComposing()) {
        unichar c = [text characterAtIndex:0];
        BOOL isDeadKey = (c == 0x00B4 || c == 0x0060 || c == 0x005E || c == 0x007E ||
                          c == 0x00A8 || c == 0x02C6 || c == 0x02DC || c == 0x02D9 ||
                          c == 0x00B8 || c == 0x02CA || c == 0x02CB);
        if (isDeadKey) return;
    }

    if (_textInputManager->isComposing())
        _textInputManager->commitComposing();

    _textInputManager->insertText([text UTF8String]);
}

- (void)deleteBackward
{
    auto* controller = _textInputManager->activeController();
    if (controller) controller->deleteBackward();
}

// ============================================================================
// UITextInput
// ============================================================================

- (UITextRange*)selectedTextRange
{
    auto* controller = _textInputManager->activeController();
    if (!controller) return nil;
    int start = controller->selectionStart();
    int end   = controller->selectionEnd();
    return [CampelloTextRange rangeWithNSRange:NSMakeRange(start, end - start)];
}

- (void)setSelectedTextRange:(UITextRange*)selectedTextRange
{
    auto* controller = _textInputManager->activeController();
    if (!controller) return;
    CampelloTextRange* r = (CampelloTextRange*)selectedTextRange;
    int start = static_cast<int>(r.range.location);
    int end   = static_cast<int>(NSMaxRange(r.range));
    controller->setSelection(start, end);
}

- (UITextRange*)markedTextRange
{
    auto* controller = _textInputManager->activeController();
    if (!controller || !controller->isComposing()) return nil;
    int start = controller->composingStart();
    int end   = controller->composingEnd();
    return [CampelloTextRange rangeWithNSRange:NSMakeRange(start, end - start)];
}

- (void)setMarkedText:(NSString*)markedText selectedRange:(NSRange)selectedRange
{
    if (!_textInputManager) return;

    // Skip standalone dead-key accents
    if (markedText.length == 1) {
        unichar c = [markedText characterAtIndex:0];
        BOOL isDeadKey = (c == 0x00B4 || c == 0x0060 || c == 0x005E || c == 0x007E ||
                          c == 0x00A8 || c == 0x02C6 || c == 0x02DC || c == 0x02D9 ||
                          c == 0x00B8 || c == 0x02CA || c == 0x02CB);
        if (isDeadKey) return;
    }

    _textInputManager->updateComposingText([markedText UTF8String]);

    auto* controller = _textInputManager->activeController();
    if (controller) {
        int selStart = controller->composingStart() + static_cast<int>(selectedRange.location);
        int selEnd   = selStart + static_cast<int>(selectedRange.length);
        controller->setSelection(selStart, selEnd);
    }
}

- (void)unmarkText
{
    if (_textInputManager)
        _textInputManager->commitComposing();
}

- (NSDictionary*)markedTextStyle { return nil; }
- (void)setMarkedTextStyle:(NSDictionary*)markedTextStyle { (void)markedTextStyle; }

- (UITextPosition*)beginningOfDocument
{
    return [CampelloTextPosition positionWithIndex:0];
}

- (UITextPosition*)endOfDocument
{
    auto* controller = _textInputManager->activeController();
    NSInteger len = controller ? static_cast<NSInteger>(controller->text().size()) : 0;
    return [CampelloTextPosition positionWithIndex:len];
}

- (UITextRange*)textRangeFromPosition:(UITextPosition*)fromPosition toPosition:(UITextPosition*)toPosition
{
    NSInteger from = ((CampelloTextPosition*)fromPosition).index;
    NSInteger to   = ((CampelloTextPosition*)toPosition).index;
    if (from > to) std::swap(from, to);
    return [CampelloTextRange rangeWithNSRange:NSMakeRange(from, to - from)];
}

- (UITextPosition*)positionFromPosition:(UITextPosition*)position offset:(NSInteger)offset
{
    NSInteger idx = ((CampelloTextPosition*)position).index + offset;
    auto* controller = _textInputManager->activeController();
    NSInteger maxLen = controller ? static_cast<NSInteger>(controller->text().size()) : 0;
    idx = std::max<NSInteger>(0, std::min(idx, maxLen));
    return [CampelloTextPosition positionWithIndex:idx];
}

- (NSComparisonResult)comparePosition:(UITextPosition*)position toPosition:(UITextPosition*)other
{
    NSInteger a = ((CampelloTextPosition*)position).index;
    NSInteger b = ((CampelloTextPosition*)other).index;
    if (a < b) return NSOrderedAscending;
    if (a > b) return NSOrderedDescending;
    return NSOrderedSame;
}

- (NSInteger)offsetFromPosition:(UITextPosition*)from toPosition:(UITextPosition*)toPosition
{
    return ((CampelloTextPosition*)toPosition).index - ((CampelloTextPosition*)from).index;
}

- (UITextPosition*)positionFromPosition:(UITextPosition*)position inDirection:(UITextLayoutDirection)direction offset:(NSInteger)offset
{
    (void)direction;
    return [self positionFromPosition:position offset:offset];
}

- (UITextPosition*)positionWithinRange:(UITextRange*)range farthestInDirection:(UITextLayoutDirection)direction
{
    CampelloTextRange* r = (CampelloTextRange*)range;
    switch (direction) {
        case UITextLayoutDirectionUp:
        case UITextLayoutDirectionLeft:  return r.start;
        case UITextLayoutDirectionDown:
        case UITextLayoutDirectionRight: return r.end;
        default:                         return r.end;
    }
}

- (UITextRange*)characterRangeByExtendingPosition:(UITextPosition*)position inDirection:(UITextLayoutDirection)direction
{
    NSInteger idx = ((CampelloTextPosition*)position).index;
    auto* controller = _textInputManager->activeController();
    NSInteger maxLen = controller ? static_cast<NSInteger>(controller->text().size()) : 0;

    switch (direction) {
        case UITextLayoutDirectionUp:
        case UITextLayoutDirectionLeft:
            if (idx > 0) return [CampelloTextRange rangeWithNSRange:NSMakeRange(idx - 1, 1)];
            return [CampelloTextRange rangeWithNSRange:NSMakeRange(idx, 0)];
        case UITextLayoutDirectionDown:
        case UITextLayoutDirectionRight:
        default:
            if (idx < maxLen) return [CampelloTextRange rangeWithNSRange:NSMakeRange(idx, 1)];
            return [CampelloTextRange rangeWithNSRange:NSMakeRange(idx, 0)];
    }
}

- (NSWritingDirection)baseWritingDirectionForPosition:(UITextPosition*)position inDirection:(UITextStorageDirection)direction
{
    (void)position; (void)direction;
    return NSWritingDirectionLeftToRight;
}

- (void)setBaseWritingDirection:(NSWritingDirection)writingDirection forRange:(UITextRange*)range
{
    (void)writingDirection; (void)range;
}

- (CGRect)firstRectForRange:(UITextRange*)range
{
    auto rect = _textInputManager->getCharacterRect(
        static_cast<int>(((CampelloTextRange*)range).range.location));
    return CGRectMake(rect[0], rect[1], std::max(rect[2], 1.0f), std::max(rect[3], 1.0f));
}

- (CGRect)caretRectForPosition:(UITextPosition*)position
{
    auto rect = _textInputManager->getCharacterRect(
        static_cast<int>(((CampelloTextPosition*)position).index));
    return CGRectMake(rect[0], rect[1], std::max(rect[2], 1.0f), std::max(rect[3], 1.0f));
}

- (UITextPosition*)closestPositionToPoint:(CGPoint)point
{
    if (!_textInputManager) return [self endOfDocument];
    
    int idx = _textInputManager->getPositionForPoint(static_cast<float>(point.x),
                                                      static_cast<float>(point.y));
    auto* controller = _textInputManager->activeController();
    int maxLen = controller ? static_cast<int>(controller->text().size()) : 0;
    idx = std::max(0, std::min(idx, maxLen));
    return [CampelloTextPosition positionWithIndex:idx];
}

- (UITextPosition*)closestPositionToPoint:(CGPoint)point withinRange:(UITextRange*)range
{
    CampelloTextRange* r = (CampelloTextRange*)range;
    if (!_textInputManager) return r.start;
    
    int idx = _textInputManager->getPositionForPoint(static_cast<float>(point.x),
                                                      static_cast<float>(point.y));
    int start = static_cast<int>(r.range.location);
    int end   = static_cast<int>(r.range.location + r.range.length);
    idx = std::max(start, std::min(idx, end));
    return [CampelloTextPosition positionWithIndex:idx];
}

- (UITextRange*)characterRangeAtPoint:(CGPoint)point
{
    if (!_textInputManager) return [CampelloTextRange rangeWithNSRange:NSMakeRange(0, 0)];
    
    int idx = _textInputManager->getPositionForPoint(static_cast<float>(point.x),
                                                      static_cast<float>(point.y));
    auto* controller = _textInputManager->activeController();
    int maxLen = controller ? static_cast<int>(controller->text().size()) : 0;
    idx = std::max(0, std::min(idx, maxLen));
    return [CampelloTextRange rangeWithNSRange:NSMakeRange(idx, 0)];
}

- (NSString*)textInRange:(UITextRange*)range
{
    auto* controller = _textInputManager->activeController();
    if (!controller) return @"";
    CampelloTextRange* r = (CampelloTextRange*)range;
    const std::string& text = controller->text();
    NSUInteger start = r.range.location;
    NSUInteger len   = r.range.length;
    if (start >= text.size()) return @"";
    if (start + len > text.size()) len = text.size() - start;
    std::string sub = text.substr(static_cast<size_t>(start), static_cast<size_t>(len));
    return [NSString stringWithUTF8String:sub.c_str()];
}

- (void)replaceRange:(UITextRange*)range withText:(NSString*)text
{
    auto* controller = _textInputManager->activeController();
    if (!controller) return;
    CampelloTextRange* r = (CampelloTextRange*)range;
    int start = static_cast<int>(r.range.location);
    int end   = static_cast<int>(NSMaxRange(r.range));
    controller->setSelection(start, end);
    controller->insertText([text UTF8String]);
}

- (NSArray<UITextSelectionRect*>*)selectionRectsForRange:(UITextRange*)range
{
    (void)range;
    return @[];
}

- (id<UITextInputTokenizer>)tokenizer
{
    if (!_tokenizer) {
        _tokenizer = [[UITextInputStringTokenizer alloc] initWithTextInput:self];
    }
    return _tokenizer;
}

@end

// ---------------------------------------------------------------------------
// CampelloViewController
// ---------------------------------------------------------------------------

@interface CampelloViewController : UIViewController
@end

@implementation CampelloViewController
{
    std::shared_ptr<Widgets::Renderer> _renderer;
    CampelloMTKView* _metalView;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    id<MTLDevice> mtlDevice = MTLCreateSystemDefaultDevice();
    if (!mtlDevice)
    {
        NSLog(@"campello_widgets: Metal is not supported on this device");
        return;
    }

    _metalView =
        [[CampelloMTKView alloc] initWithFrame:self.view.bounds device:mtlDevice];
    _metalView.colorPixelFormat         = MTLPixelFormatBGRA8Unorm;
    _metalView.depthStencilPixelFormat  = MTLPixelFormatInvalid;
    _metalView.clearColor               = MTLClearColorMake(1.0, 1.0, 1.0, 1.0);
    _metalView.autoresizingMask         =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    // paused/enableSetNeedsDisplay are set inside setupWithGPUDevice.

    [self.view addSubview:_metalView];

    auto gpuDevice = GPU::Device::createDefaultDevice(nullptr);
    if (!gpuDevice)
    {
        NSLog(@"campello_widgets: failed to create campello_gpu device");
        return;
    }

    _renderer = [_metalView setupWithGPUDevice:gpuDevice rootWidget:gRootWidget];
    
    // Initial safe area update
    [self updateSafeAreaInsets];
}

- (void)viewSafeAreaInsetsDidChange
{
    [super viewSafeAreaInsetsDidChange];
    [self updateSafeAreaInsets];
}

- (void)viewDidLayoutSubviews
{
    [super viewDidLayoutSubviews];
    [self updateSafeAreaInsets];
    // View bounds may have changed (rotation, split-screen resize) — request a
    // frame so the widget tree lays out at the new size.
    Widgets::FrameScheduler::scheduleFrame();
}

- (void)updateSafeAreaInsets
{
    if (!_renderer) return;
    
    // Get safe area insets from the view.
    // On iOS 11+, this accounts for notches, home indicators, etc.
    // safeAreaInsets are already in logical points; no need to multiply by scale
    UIEdgeInsets safeInsets = self.view.safeAreaInsets;
    
    Widgets::EdgeInsets insets;
    insets.left   = static_cast<float>(safeInsets.left);
    insets.top    = static_cast<float>(safeInsets.top);
    insets.right  = static_cast<float>(safeInsets.right);
    insets.bottom = static_cast<float>(safeInsets.bottom);
    
    _renderer->setViewInsets(insets);
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
    // Namespace aliases for use inside this namespace block
    namespace GPU     = ::systems::leal::campello_gpu;
    namespace Widgets = ::systems::leal::campello_widgets;

int runApp(int argc, char** argv, WidgetRef root_widget)
{
    gRootWidget = std::move(root_widget);

    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil,
            NSStringFromClass([CampelloAppDelegate class]));
    }
}

} // namespace systems::leal::campello_widgets
