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
#import <campello_widgets/ui/thread_checker.hpp>

#include <chrono>

#import <campello_gpu/device.hpp>
#import <campello_gpu/texture_view.hpp>
#import <campello_gpu/constants/pixel_format.hpp>

// MetalDrawBackend lives in src/gpu/metal/ — the single Metal backend shared
// by macOS and iOS (compiled in via GLOB_RECURSE in both macos.cmake and
// ios.cmake).
#import "../gpu/metal/metal_draw_backend.hpp"

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
    Widgets::MediaQueryData gMediaData;

    // Maps a physical (hardware) keyboard press to the framework's
    // platform-agnostic KeyCode — the iOS/Simulator equivalent of
    // macosKeyCodeToKeyCode() in src/macos/run_app.mm. UIKeyboardHIDUsage
    // values are USB HID keyboard-page usage IDs (stable, standardized —
    // not an Apple-specific numbering), available via UIKey.keyCode since
    // iOS 13.4. Only covers the subset of keys this framework's KeyCode
    // enum represents (see key_event.hpp); anything else maps to unknown.
    static Widgets::KeyCode iosHIDUsageToKeyCode(UIKeyboardHIDUsage usage)
    {
        switch (usage)
        {
        case UIKeyboardHIDUsageKeyboardA: return Widgets::KeyCode::a;
        case UIKeyboardHIDUsageKeyboardB: return Widgets::KeyCode::b;
        case UIKeyboardHIDUsageKeyboardC: return Widgets::KeyCode::c;
        case UIKeyboardHIDUsageKeyboardD: return Widgets::KeyCode::d;
        case UIKeyboardHIDUsageKeyboardE: return Widgets::KeyCode::e;
        case UIKeyboardHIDUsageKeyboardF: return Widgets::KeyCode::f;
        case UIKeyboardHIDUsageKeyboardG: return Widgets::KeyCode::g;
        case UIKeyboardHIDUsageKeyboardH: return Widgets::KeyCode::h;
        case UIKeyboardHIDUsageKeyboardI: return Widgets::KeyCode::i;
        case UIKeyboardHIDUsageKeyboardJ: return Widgets::KeyCode::j;
        case UIKeyboardHIDUsageKeyboardK: return Widgets::KeyCode::k;
        case UIKeyboardHIDUsageKeyboardL: return Widgets::KeyCode::l;
        case UIKeyboardHIDUsageKeyboardM: return Widgets::KeyCode::m;
        case UIKeyboardHIDUsageKeyboardN: return Widgets::KeyCode::n;
        case UIKeyboardHIDUsageKeyboardO: return Widgets::KeyCode::o;
        case UIKeyboardHIDUsageKeyboardP: return Widgets::KeyCode::p;
        case UIKeyboardHIDUsageKeyboardQ: return Widgets::KeyCode::q;
        case UIKeyboardHIDUsageKeyboardR: return Widgets::KeyCode::r;
        case UIKeyboardHIDUsageKeyboardS: return Widgets::KeyCode::s;
        case UIKeyboardHIDUsageKeyboardT: return Widgets::KeyCode::t;
        case UIKeyboardHIDUsageKeyboardU: return Widgets::KeyCode::u;
        case UIKeyboardHIDUsageKeyboardV: return Widgets::KeyCode::v;
        case UIKeyboardHIDUsageKeyboardW: return Widgets::KeyCode::w;
        case UIKeyboardHIDUsageKeyboardX: return Widgets::KeyCode::x;
        case UIKeyboardHIDUsageKeyboardY: return Widgets::KeyCode::y;
        case UIKeyboardHIDUsageKeyboardZ: return Widgets::KeyCode::z;

        case UIKeyboardHIDUsageKeyboard0: return Widgets::KeyCode::digit_0;
        case UIKeyboardHIDUsageKeyboard1: return Widgets::KeyCode::digit_1;
        case UIKeyboardHIDUsageKeyboard2: return Widgets::KeyCode::digit_2;
        case UIKeyboardHIDUsageKeyboard3: return Widgets::KeyCode::digit_3;
        case UIKeyboardHIDUsageKeyboard4: return Widgets::KeyCode::digit_4;
        case UIKeyboardHIDUsageKeyboard5: return Widgets::KeyCode::digit_5;
        case UIKeyboardHIDUsageKeyboard6: return Widgets::KeyCode::digit_6;
        case UIKeyboardHIDUsageKeyboard7: return Widgets::KeyCode::digit_7;
        case UIKeyboardHIDUsageKeyboard8: return Widgets::KeyCode::digit_8;
        case UIKeyboardHIDUsageKeyboard9: return Widgets::KeyCode::digit_9;

        case UIKeyboardHIDUsageKeyboardSpacebar:           return Widgets::KeyCode::space;
        case UIKeyboardHIDUsageKeyboardReturnOrEnter:      return Widgets::KeyCode::enter;
        case UIKeyboardHIDUsageKeyboardTab:                return Widgets::KeyCode::tab;
        case UIKeyboardHIDUsageKeyboardDeleteOrBackspace:  return Widgets::KeyCode::backspace;
        case UIKeyboardHIDUsageKeyboardEscape:              return Widgets::KeyCode::escape;
        case UIKeyboardHIDUsageKeyboardDeleteForward:       return Widgets::KeyCode::delete_forward;

        case UIKeyboardHIDUsageKeyboardLeftArrow:  return Widgets::KeyCode::left;
        case UIKeyboardHIDUsageKeyboardRightArrow: return Widgets::KeyCode::right;
        case UIKeyboardHIDUsageKeyboardUpArrow:    return Widgets::KeyCode::up;
        case UIKeyboardHIDUsageKeyboardDownArrow:  return Widgets::KeyCode::down;
        case UIKeyboardHIDUsageKeyboardHome:       return Widgets::KeyCode::home;
        case UIKeyboardHIDUsageKeyboardEnd:        return Widgets::KeyCode::end;
        case UIKeyboardHIDUsageKeyboardPageUp:     return Widgets::KeyCode::page_up;
        case UIKeyboardHIDUsageKeyboardPageDown:   return Widgets::KeyCode::page_down;

        case UIKeyboardHIDUsageKeyboardF1:  return Widgets::KeyCode::f1;
        case UIKeyboardHIDUsageKeyboardF2:  return Widgets::KeyCode::f2;
        case UIKeyboardHIDUsageKeyboardF3:  return Widgets::KeyCode::f3;
        case UIKeyboardHIDUsageKeyboardF4:  return Widgets::KeyCode::f4;
        case UIKeyboardHIDUsageKeyboardF5:  return Widgets::KeyCode::f5;
        case UIKeyboardHIDUsageKeyboardF6:  return Widgets::KeyCode::f6;
        case UIKeyboardHIDUsageKeyboardF7:  return Widgets::KeyCode::f7;
        case UIKeyboardHIDUsageKeyboardF8:  return Widgets::KeyCode::f8;
        case UIKeyboardHIDUsageKeyboardF9:  return Widgets::KeyCode::f9;
        case UIKeyboardHIDUsageKeyboardF10: return Widgets::KeyCode::f10;
        case UIKeyboardHIDUsageKeyboardF11: return Widgets::KeyCode::f11;
        case UIKeyboardHIDUsageKeyboardF12: return Widgets::KeyCode::f12;

        case UIKeyboardHIDUsageKeyboardLeftShift:    return Widgets::KeyCode::left_shift;
        case UIKeyboardHIDUsageKeyboardRightShift:   return Widgets::KeyCode::right_shift;
        case UIKeyboardHIDUsageKeyboardLeftControl:  return Widgets::KeyCode::left_ctrl;
        case UIKeyboardHIDUsageKeyboardRightControl: return Widgets::KeyCode::right_ctrl;
        case UIKeyboardHIDUsageKeyboardLeftAlt:      return Widgets::KeyCode::left_alt;
        case UIKeyboardHIDUsageKeyboardRightAlt:     return Widgets::KeyCode::right_alt;
        case UIKeyboardHIDUsageKeyboardLeftGUI:      return Widgets::KeyCode::left_meta;
        case UIKeyboardHIDUsageKeyboardRightGUI:     return Widgets::KeyCode::right_meta;
        case UIKeyboardHIDUsageKeyboardCapsLock:     return Widgets::KeyCode::caps_lock;

        default: return Widgets::KeyCode::unknown;
        }
    }

    static uint32_t iosModifiersToKeyModifiers(UIKeyModifierFlags flags)
    {
        uint32_t mods = Widgets::KeyModifiers::none;
        if (flags & UIKeyModifierShift)     mods |= Widgets::KeyModifiers::shift;
        if (flags & UIKeyModifierControl)   mods |= Widgets::KeyModifiers::ctrl;
        if (flags & UIKeyModifierAlternate) mods |= Widgets::KeyModifiers::alt;
        if (flags & UIKeyModifierCommand)   mods |= Widgets::KeyModifiers::meta;
        return mods;
    }

    static Widgets::Brightness getSystemBrightness()
    {
        if (@available(iOS 12.0, *)) {
            UIUserInterfaceStyle style = UITraitCollection.currentTraitCollection.userInterfaceStyle;
            if (style == UIUserInterfaceStyleDark) {
                return Widgets::Brightness::dark;
            }
        }
        return Widgets::Brightness::light;
    }
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
    Widgets::MediaQueryData                     _mediaData;

    // Touch → pointer_id mapping (UITouch* identity is stable per gesture)
    std::map<void*, int32_t>  _touchIds;
    int32_t                   _nextPointerId;

    id<UITextInputTokenizer>  _tokenizer;
    __weak id<UITextInputDelegate> _inputDelegate;
    UIView*                   _emptyInputView;
    UIView*                   _emptyInputAccessoryView;
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

// viewDidAppear: makes this view first responder proactively (see its own
// comment) so hardware-key routing works even with nothing focused. On a
// device with no hardware keyboard attached, UIKit's default behavior for
// any UIKeyInput-conforming first responder is to raise the on-screen
// keyboard — which would then pop up on launch, with no TextField actually
// focused. Returning a non-nil, zero-size view here while no TextField
// holds input focus suppresses that (UIKit shows *this* as "the keyboard"
// instead of its own); returning nil once a TextField is focused restores
// the real system keyboard. reloadInputViews (called from
// setOnInputTargetChanged below) is what makes UIKit re-query this after
// focus changes — it isn't polled automatically.
- (UIView*)inputView
{
    if (_textInputManager && _textInputManager->hasInputTarget())
        return nil;

    if (!_emptyInputView)
        _emptyInputView = [[UIView alloc] initWithFrame:CGRectZero];
    return _emptyInputView;
}

// Without this, iPadOS still shows its own default accessory bar (the
// predictive-text/dictation strip, with a floating mic button) docked to
// the bottom of the screen even though -inputView above suppresses the
// actual keyboard — that default accessory is supplied independently
// whenever a UITextInput-conforming responder is first responder and this
// method returns nil. Same nil-while-editing / suppressed-otherwise split
// as -inputView, for the same reason.
- (UIView*)inputAccessoryView
{
    if (_textInputManager && _textInputManager->hasInputTarget())
        return nil;

    if (!_emptyInputAccessoryView)
        _emptyInputAccessoryView = [[UIView alloc] initWithFrame:CGRectZero];
    return _emptyInputAccessoryView;
}

// Neither -inputView nor -inputAccessoryView above is enough on iPadOS: a
// floating dictation/predictive-text control still appears independently,
// because UIKit decides whether to treat the first responder as a "text
// input" (and show that control) by asking
// -conformsToProtocol:@protocol(UITextInput) — a real, dynamic message
// send, not the static <UITextInput> in this class's @interface. Denying
// conformance while no TextField is focused makes UIKit fall back to
// plain UIResponder/UIKeyInput behavior (no text-editing system UI at
// all); pressesBegan:/pressesEnded: keep working regardless, since those
// are delivered to any first responder and don't depend on this.
- (BOOL)conformsToProtocol:(Protocol*)aProtocol
{
    if (aProtocol == @protocol(UITextInput) &&
        !(_textInputManager && _textInputManager->hasInputTarget()))
    {
        return NO;
    }
    return [super conformsToProtocol:aProtocol];
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
            if (has_target) {
                if (!strongSelf.isFirstResponder) [strongSelf becomeFirstResponder];
            } else if (strongSelf.isFirstResponder) {
                // Resigning here is what dismisses any on-screen keyboard
                // raised for the TextField that just lost focus. But nothing
                // else in this class ever re-requests first-responder status
                // afterward (viewDidAppear: only claims it once, at launch),
                // so without immediately reclaiming it here, this view would
                // permanently stop receiving pressesBegan:/pressesEnded: —
                // breaking hardware-keyboard input everywhere, not just in
                // text fields, for the rest of the session.
                [strongSelf resignFirstResponder];
                [strongSelf becomeFirstResponder];
            }
            // -inputView's answer depends on hasInputTarget(), which just
            // changed — UIKit doesn't re-query it on its own, so without
            // this the keyboard would show/hide a step behind (or not at
            // all) rather than tracking focus.
            [strongSelf reloadInputViews];
        }
    });

    // True continuous rendering: MTKView's own internal CADisplayLink drives
    // drawInMTKView: every tick (paused defaults to NO already); each call
    // is a cheap no-op via Renderer::buildFrame()'s "nothing dirty" early
    // exit when there's genuinely nothing to paint. This is deliberately
    // NOT on-demand (enableSetNeedsDisplay=YES + manual setNeedsDisplay()
    // per FrameScheduler tick, which is what src/macos/run_app.mm uses):
    // that combination was found to fully starve UIKit's touch delivery
    // (hitTest: stops being invoked entirely, not just delayed) once a
    // continuous AnimationController is running — reproduced identically on
    // both the Simulator and a real device. Driving the redraw loop by
    // rapidly re-invoking setNeedsDisplay() from application code doesn't
    // get the same run-loop-friendly scheduling as MTKView's own
    // CADisplayLink-paced path.
    self.enableSetNeedsDisplay = NO;
    Widgets::FrameScheduler::setCallback([weakSelf] {
        if (weakSelf) [weakSelf setNeedsDisplay];
    });

    // Wrap root widget with MediaQuery
    Widgets::MediaQueryData mediaData;
    mediaData.device_pixel_ratio = static_cast<float>(self.contentScaleFactor);
    mediaData.platform_brightness = getSystemBrightness();
    mediaData.logical_size = Widgets::Size{
        static_cast<float>(self.bounds.size.width),
        static_cast<float>(self.bounds.size.height) };
    if (@available(iOS 11.0, *)) {
        UIEdgeInsets safeInsets = self.safeAreaInsets;
        mediaData.padding.left   = static_cast<float>(safeInsets.left);
        mediaData.padding.top    = static_cast<float>(safeInsets.top);
        mediaData.padding.right  = static_cast<float>(safeInsets.right);
        mediaData.padding.bottom = static_cast<float>(safeInsets.bottom);
    }
    _mediaData = mediaData;
    gMediaData = mediaData;
    
    auto wrappedRoot = Widgets::mw<Widgets::MediaQuery>(
        mediaData, rootWidget);

    // Bind the UI thread before any widget tree mutation.
    Widgets::ThreadChecker::instance().bindToCurrentThread();

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

- (void)rebuildMediaQuery
{
    if (!_rootElement) return;
    auto newMediaQuery = Widgets::mw<Widgets::MediaQuery>(
        _mediaData, gRootWidget);
    _rootElement->update(newMediaQuery);
    Widgets::FrameScheduler::scheduleFrame();
}

- (void)updateMediaQueryBrightness
{
    Widgets::Brightness newBrightness = getSystemBrightness();
    if (_mediaData.platform_brightness == newBrightness) return;
    _mediaData.platform_brightness = newBrightness;
    gMediaData.platform_brightness = newBrightness;
    [self rebuildMediaQuery];
}

- (void)updateWindowMetrics
{
    Widgets::MediaQueryData newData = _mediaData;
    newData.logical_size = Widgets::Size{
        static_cast<float>(self.bounds.size.width),
        static_cast<float>(self.bounds.size.height) };
    if (@available(iOS 11.0, *)) {
        UIEdgeInsets safeInsets = self.safeAreaInsets;
        newData.padding.left   = static_cast<float>(safeInsets.left);
        newData.padding.top    = static_cast<float>(safeInsets.top);
        newData.padding.right  = static_cast<float>(safeInsets.right);
        newData.padding.bottom = static_cast<float>(safeInsets.bottom);
    }
    if (newData != _mediaData) {
        _mediaData = newData;
        gMediaData = newData;
        [self rebuildMediaQuery];
    }
}

- (void)traitCollectionDidChange:(UITraitCollection *)previousTraitCollection
{
    [super traitCollectionDidChange:previousTraitCollection];
    [self updateMediaQueryBrightness];
}

- (void)drawInMTKView:(MTKView*)view
{
    if (!_renderer) return;

    id<CAMetalDrawable> drawable = view.currentDrawable;
    if (!drawable) return;

    CGFloat scale = self.contentScaleFactor;
    CGRect bounds = self.bounds;
    CGSize drawableSize = view.drawableSize;

    // Use drawableSize (physical pixels) for the Renderer, matching macOS's
    // drawInMTKView: — Renderer::renderFrame's viewport_width/height are
    // PHYSICAL pixels; it divides by DPR internally to get logical layout
    // constraints (see Renderer::layoutPass), and the backend's scissor/NDC
    // math (applyScissor) compares against that same physical viewport.
    // Passing logical points here (as this used to) desyncs the two: clip
    // rects computed in physical pixels get clamped against a viewport 1/DPR
    // too small, collapsing most content's scissor rect to empty.
    (void)bounds;
    float physical_width = (float)drawableSize.width;
    float physical_height= (float)drawableSize.height;

    _renderer->setDevicePixelRatio(static_cast<float>(scale));
    if (_backendPtr) _backendPtr->setViewport(physical_width, physical_height);

    // Tie presentation to GPU completion + vsync via presentDrawable: on the
    // command buffer rather than calling [drawable present] on the CPU.
    _device->scheduleNextPresent((__bridge void*)drawable);

    // Note: PointerDispatcher/TickerScheduler are ticked inside
    // Renderer::renderFrame() itself — don't duplicate that here.
    auto colorView = GPU::TextureView::fromNative((__bridge void*)drawable.texture);
    _renderer->setPendingDrawable((__bridge void*)drawable);
    bool rendered = colorView && _renderer->renderFrame(colorView, physical_width, physical_height);
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

    // generateDrawList() paints the root tree offset by (view_insets_.left,
    // .top) — the safe-area inset (status bar / Dynamic Island) — so the
    // tree's own local origin sits that far in from the view's top-left.
    // PointerDispatcher::hitTest() expects positions in that same
    // tree-local space, so touch coordinates (which are in full-view space)
    // must have the same insets subtracted here, or every tap on a device
    // with a non-zero safe area (i.e. any iPhone) lands offset from what's
    // visually under the finger.
    Widgets::EdgeInsets insets = _renderer ? _renderer->viewInsets() : Widgets::EdgeInsets::zero();
    return { (float)pt.x - insets.left, (float)pt.y - insets.top };
}

- (float)pressureForTouch:(UITouch*)touch
{
    // force is 0 on devices without 3D Touch; treat as 1.0 (fully pressed).
    const float f = (float)touch.force;
    return (f > 0.0f) ? f : 1.0f;
}

- (Widgets::PointerDeviceKind)deviceKindForTouch:(UITouch*)touch
{
    return touch.type == UITouchTypePencil
        ? Widgets::PointerDeviceKind::stylus
        : Widgets::PointerDeviceKind::touch;
}

// altitudeAngle/azimuthAngleInView: are only meaningful for an actual Apple
// Pencil touch (UITouchTypePencil) — for anything else they either read 0
// or aren't reported at all, which already matches this codebase's "0.0 ==
// no tilt data" convention (see PointerEvent::tilt's doc comment), so no
// separate has-tilt check is needed before calling these.

- (float)tiltForTouch:(UITouch*)touch
{
    if (touch.type != UITouchTypePencil) return 0.0f;
    // UITouch.altitudeAngle: 0 = flat against the surface, pi/2 =
    // perpendicular to it. PointerEvent::tilt uses the opposite sense (0 =
    // perpendicular, pi/2 = flat, matching Android's AMOTION_EVENT_AXIS_TILT
    // convention) — take the complement.
    return (float)(M_PI_2 - touch.altitudeAngle);
}

- (float)tiltOrientationForTouch:(UITouch*)touch
{
    if (touch.type != UITouchTypePencil) return 0.0f;
    // UITouch.azimuthAngleInView: returns 0..2*pi, measured clockwise from
    // the view's positive x axis. PointerEvent::tilt_orientation uses
    // -pi..pi — rebase into that range.
    float azimuth = (float)[touch azimuthAngleInView:self];
    if (azimuth > (float)M_PI) azimuth -= (float)(2.0 * M_PI);
    return azimuth;
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
            .kind        = Widgets::PointerEventKind::down,
            .pointer_id  = [self acquirePointerIdForTouch:touch],
            .position    = [self offsetForTouch:touch],
            .pressure    = [self pressureForTouch:touch],
            .device_kind = [self deviceKindForTouch:touch],
            .tilt        = [self tiltForTouch:touch],
            .tilt_orientation = [self tiltOrientationForTouch:touch]});
    }
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    (void)event;
    if (!_dispatcher) return;
    for (UITouch* touch in touches)
    {
        _dispatcher->handlePointerEvent({
            .kind        = Widgets::PointerEventKind::move,
            .pointer_id  = [self acquirePointerIdForTouch:touch],
            .position    = [self offsetForTouch:touch],
            .pressure    = [self pressureForTouch:touch],
            .device_kind = [self deviceKindForTouch:touch],
            .tilt        = [self tiltForTouch:touch],
            .tilt_orientation = [self tiltOrientationForTouch:touch]});
    }
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    (void)event;
    if (!_dispatcher) return;
    for (UITouch* touch in touches)
    {
        _dispatcher->handlePointerEvent({
            .kind        = Widgets::PointerEventKind::up,
            .pointer_id  = [self acquirePointerIdForTouch:touch],
            .position    = [self offsetForTouch:touch],
            .pressure    = 0.0f,
            .device_kind = [self deviceKindForTouch:touch]});
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
            .kind        = Widgets::PointerEventKind::cancel,
            .pointer_id  = [self acquirePointerIdForTouch:touch],
            .position    = [self offsetForTouch:touch],
            .pressure    = 0.0f,
            .device_kind = [self deviceKindForTouch:touch]});
        [self releasePointerIdForTouch:touch];
    }
}

// ============================================================================
// Physical keyboard (UIPress) — mirrors src/macos/run_app.mm's keyDown:/
// keyUp:. A Bluetooth/Smart Keyboard, or (in Simulator) the host Mac's own
// keyboard, delivers presses here via UIKit's hardware-keyboard API
// (iOS 13.4+) rather than through UIKeyInput, which only ever sees the
// characters a text-composition session accepts. Requires this view to be
// first responder — see CampelloViewController's viewDidLoad, which calls
// becomeFirstResponder proactively (not just reactively when a TextField
// gains focus) so global shortcuts work even with nothing focused.
// ============================================================================

- (void)pressesBegan:(NSSet<UIPress*>*)presses withEvent:(UIPressesEvent*)event
{
    // Presses that carry a UIKey are always fully handled below (either
    // routed into text insertion or into FocusManager), so they're removed
    // here before forwarding to super. Any press WITHOUT a UIKey (e.g. from
    // a game controller) is left in and still reaches super's default
    // handling. Forwarding an already-handled UIKey press to super would
    // let UIKit's own UIResponder implementation auto-bridge it into
    // insertText:/deleteBackward a second time — since this view conforms
    // to UIKeyInput — producing a duplicate/garbled character on every
    // hardware keystroke.
    NSMutableSet<UIPress*>* unhandledPresses = [presses mutableCopy];
    for (UIPress* press in presses)
    {
        UIKey* key = press.key;
        if (!key) { continue; }
        [unhandledPresses removeObject:press];

        Widgets::KeyCode keyCode  = iosHIDUsageToKeyCode(key.keyCode);
        uint32_t         modifiers = iosModifiersToKeyModifiers(key.modifierFlags);

        // Mirrors macOS's isNavigationKey/isSpecialKey split: these never
        // flow through UIKeyInput's text-composition pipeline (insertText:/
        // deleteBackward), so they must be routed to FocusManager directly
        // regardless of whether a TextField currently has input focus.
        BOOL isNavigationKey =
            (keyCode == Widgets::KeyCode::left  || keyCode == Widgets::KeyCode::right ||
             keyCode == Widgets::KeyCode::up    || keyCode == Widgets::KeyCode::down  ||
             keyCode == Widgets::KeyCode::home  || keyCode == Widgets::KeyCode::end   ||
             keyCode == Widgets::KeyCode::page_up || keyCode == Widgets::KeyCode::page_down);

        BOOL isSpecialKey =
            (keyCode == Widgets::KeyCode::escape || keyCode == Widgets::KeyCode::tab ||
             keyCode == Widgets::KeyCode::enter  || keyCode == Widgets::KeyCode::backspace ||
             keyCode == Widgets::KeyCode::delete_forward ||
             keyCode == Widgets::KeyCode::f1  || keyCode == Widgets::KeyCode::f2  ||
             keyCode == Widgets::KeyCode::f3  || keyCode == Widgets::KeyCode::f4  ||
             keyCode == Widgets::KeyCode::f5  || keyCode == Widgets::KeyCode::f6  ||
             keyCode == Widgets::KeyCode::f7  || keyCode == Widgets::KeyCode::f8  ||
             keyCode == Widgets::KeyCode::f9  || keyCode == Widgets::KeyCode::f10 ||
             keyCode == Widgets::KeyCode::f11 || keyCode == Widgets::KeyCode::f12);

        // A Command- or Control-held keystroke is an app/system shortcut by
        // convention, never text composition — see macOS's identical
        // hasCommandModifier check for why this must bypass text input even
        // while a TextField holds it.
        BOOL hasShortcutModifier =
            (modifiers & (Widgets::KeyModifiers::meta | Widgets::KeyModifiers::ctrl)) != 0;

        if (_textInputManager && _textInputManager->hasInputTarget() &&
            !hasShortcutModifier && !isNavigationKey && !isSpecialKey)
        {
            // Plain character key while a TextField is focused. Unlike the
            // on-screen keyboard — whose taps call insertText: directly —
            // UIKit does NOT automatically bridge hardware key presses into
            // a custom UITextInput's insertText:/deleteBackward; for a
            // built-in UITextField that translation is hidden inside
            // Apple's own implementation, but ours has to do it explicitly
            // here, or hardware typing does nothing while the on-screen
            // keyboard is showing.
            if (key.characters.length > 0)
                [self insertText:key.characters];
            continue;
        }

        if (_focusManager)
        {
            Widgets::KeyEvent ke;
            ke.kind      = Widgets::KeyEventKind::down;
            ke.key_code  = keyCode;
            ke.modifiers = modifiers;
            ke.character = key.characters.length > 0
                ? (uint32_t)[key.characters characterAtIndex:0] : 0u;
            _focusManager->handleKeyEvent(ke);
        }
    }
    if (unhandledPresses.count > 0)
        [super pressesBegan:unhandledPresses withEvent:event];
}

- (void)pressesEnded:(NSSet<UIPress*>*)presses withEvent:(UIPressesEvent*)event
{
    if (_focusManager)
    {
        for (UIPress* press in presses)
        {
            UIKey* key = press.key;
            if (!key) continue;
            Widgets::KeyEvent ke;
            ke.kind      = Widgets::KeyEventKind::up;
            ke.key_code  = iosHIDUsageToKeyCode(key.keyCode);
            ke.modifiers = iosModifiersToKeyModifiers(key.modifierFlags);
            ke.character = 0;
            _focusManager->handleKeyEvent(ke);
        }
    }
    [super pressesEnded:presses withEvent:event];
}

- (void)pressesCancelled:(NSSet<UIPress*>*)presses withEvent:(UIPressesEvent*)event
{
    // Treat a cancelled press the same as a release — matches
    // touchesCancelled:'s "cancel is a variant of up" handling above.
    [self pressesEnded:presses withEvent:event];
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

- (void)viewDidAppear:(BOOL)animated
{
    [super viewDidAppear:animated];
    // Proactively claim first responder so hardware-keyboard presses
    // (pressesBegan:/pressesEnded: on CampelloMTKView) are delivered from
    // the start — not just once a TextField requests it via
    // setOnInputTargetChanged — so global shortcuts (e.g. the gallery's
    // Cmd/Ctrl+D debug-overlay toggle) work with nothing focused. Done here
    // rather than viewDidLoad: the view isn't guaranteed to be in a window
    // yet at that point, and becomeFirstResponder silently no-ops outside one.
    [_metalView becomeFirstResponder];
}

- (void)viewSafeAreaInsetsDidChange
{
    [super viewSafeAreaInsetsDidChange];
    [self updateSafeAreaInsets];
    [_metalView updateWindowMetrics];
}

- (void)viewDidLayoutSubviews
{
    [super viewDidLayoutSubviews];
    [self updateSafeAreaInsets];
    [_metalView updateWindowMetrics];
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
