#import <campello_widgets/macos/run_app.hpp>
#import <campello_widgets/campello_widgets.hpp>
#import <campello_widgets/ui/system_mouse_cursor.hpp>
#import <campello_widgets/widgets/element.hpp>
#import <campello_widgets/widgets/render_object_element.hpp>
#import <campello_widgets/widgets/platform_menu_delegate.hpp>
#import <campello_widgets/ui/renderer.hpp>
#import <campello_widgets/ui/render_box.hpp>
#import <campello_widgets/ui/pointer_event.hpp>
#import <campello_widgets/ui/pointer_dispatcher.hpp>
#import <campello_widgets/ui/key_event.hpp>
#import <campello_widgets/ui/focus_manager.hpp>
#import <campello_widgets/ui/text_input_manager.hpp>
#import <campello_widgets/ui/ticker.hpp>
#import <campello_widgets/ui/frame_scheduler.hpp>
#import <campello_widgets/ui/thread_checker.hpp>
#import <campello_widgets/ui/raster_thread.hpp>
#import <campello_widgets/diagnostics/widget_inspector.hpp>

#include <chrono>

#import <campello_gpu/device.hpp>
#import <campello_gpu/texture_view.hpp>
#import <campello_gpu/constants/pixel_format.hpp>

#import "../gpu/metal/metal_draw_backend.hpp"

#import <Cocoa/Cocoa.h>
#import <MetalKit/MetalKit.h>

// Forward declaration for PlatformMenuDelegate initialization
extern "C" void campello_widgets_initialize_macos_menu_delegate();

// Namespace aliases - using global qualification to work correctly in Unity Build
namespace GPU     = ::systems::leal::campello_gpu;
namespace Widgets = ::systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// Internal state passed from runApp() to the delegates
// ---------------------------------------------------------------------------
namespace {
    Widgets::WidgetRef gRootWidget;
    std::string        gTitle;
    float              gWidth  = 800.0f;
    float              gHeight = 600.0f;
    MTKView*           gMetalView = nullptr;  // Global access for requestRefresh()
    std::shared_ptr<Widgets::Renderer> gRenderer;  // Global access for requestRefresh()
    Widgets::MediaQueryData gMediaData;

    // When non-null, runApp() will use this device instead of creating one.
    // This lets the host application share its GPU device with the widget framework.
    std::shared_ptr<GPU::Device> gExternalDevice;

    static Widgets::Brightness getSystemBrightness()
    {
        if (@available(macOS 10.14, *)) {
            NSAppearance* appearance = NSApp.effectiveAppearance;
            if (appearance) {
                NSAppearanceName name = appearance.name;
                if ([name isEqualToString:NSAppearanceNameDarkAqua]) {
                    return Widgets::Brightness::dark;
                }
            }
        }
        return Widgets::Brightness::light;
    }
}

// ---------------------------------------------------------------------------
// MTKView subclass — forwards mouse events to the PointerDispatcher
// ---------------------------------------------------------------------------

@interface CampelloMTKView : MTKView <NSTextInputClient>
- (void)setDispatcher:(Widgets::PointerDispatcher*)dispatcher;
- (void)setFocusManager:(Widgets::FocusManager*)focusManager;
- (void)setTextInputManager:(Widgets::TextInputManager*)textInputManager;
- (void)setAppearanceChangedCallback:(std::function<void()>)callback;
@end

@implementation CampelloMTKView {
    Widgets::PointerDispatcher* _dispatcher;
    Widgets::FocusManager*      _focusManager;
    Widgets::TextInputManager*  _textInputManager;
    std::function<void()>       _appearanceChangedCallback;
}

- (void)setDispatcher:(Widgets::PointerDispatcher*)dispatcher
{
    _dispatcher = dispatcher;
}

- (void)setFocusManager:(Widgets::FocusManager*)focusManager
{
    _focusManager = focusManager;
}

- (void)setTextInputManager:(Widgets::TextInputManager*)textInputManager
{
    _textInputManager = textInputManager;
}

- (void)setAppearanceChangedCallback:(std::function<void()>)callback
{
    _appearanceChangedCallback = std::move(callback);
}

- (void)viewDidChangeEffectiveAppearance
{
    [super viewDidChangeEffectiveAppearance];
    if (_appearanceChangedCallback) {
        _appearanceChangedCallback();
    }
}

- (BOOL)acceptsFirstResponder { return YES; }

- (Widgets::Offset)pointerOffsetForEvent:(NSEvent*)event
{
    // Return coordinates in logical pixels (points), not physical pixels.
    // The Renderer converts to physical pixels internally using DPR.
    const CGPoint pt = [self convertPoint:event.locationInWindow fromView:nil];
    const float flipped_y = (float)(self.bounds.size.height - pt.y);

    // generateDrawList() paints the root tree offset by (view_insets_.left,
    // .top) — the safe-area inset (e.g. the camera housing on notched
    // MacBook Pros) — so the tree's own local origin sits that far in from
    // the view's top-left. PointerDispatcher::hitTest() expects positions in
    // that same tree-local space, so this must subtract the same insets, or
    // clicks land offset from what's visually under the cursor whenever the
    // inset is non-zero.
    Widgets::EdgeInsets insets = gRenderer ? gRenderer->viewInsets() : Widgets::EdgeInsets::zero();
    return { (float)pt.x - insets.left, flipped_y - insets.top };
}

- (void)mouseDown:(NSEvent*)event
{
    if (!_dispatcher) return;
    auto pos = [self pointerOffsetForEvent:event];
    _dispatcher->handlePointerEvent({
        Widgets::PointerEventKind::down, 0,
        pos, 1.0f, Widgets::PointerDeviceKind::mouse});
}

- (void)mouseMoved:(NSEvent*)event
{
    if (!_dispatcher) return;
    _dispatcher->handlePointerEvent({
        Widgets::PointerEventKind::move, 0,
        [self pointerOffsetForEvent:event], 0.0f, Widgets::PointerDeviceKind::mouse});
}

- (void)mouseDragged:(NSEvent*)event
{
    if (!_dispatcher) return;
    _dispatcher->handlePointerEvent({
        Widgets::PointerEventKind::move, 0,
        [self pointerOffsetForEvent:event], 1.0f, Widgets::PointerDeviceKind::mouse});
}

- (void)mouseUp:(NSEvent*)event
{
    if (!_dispatcher) return;
    auto pos = [self pointerOffsetForEvent:event];
    _dispatcher->handlePointerEvent({
        Widgets::PointerEventKind::up, 0,
        pos, 1.0f, Widgets::PointerDeviceKind::mouse});
}

static Widgets::KeyCode macosKeyCodeToKeyCode(unsigned short kc)
{
    switch (kc) {
        case 0:  return Widgets::KeyCode::a;
        case 1:  return Widgets::KeyCode::s;
        case 2:  return Widgets::KeyCode::d;
        case 3:  return Widgets::KeyCode::f;
        case 4:  return Widgets::KeyCode::h;
        case 5:  return Widgets::KeyCode::g;
        case 6:  return Widgets::KeyCode::z;
        case 7:  return Widgets::KeyCode::x;
        case 8:  return Widgets::KeyCode::c;
        case 9:  return Widgets::KeyCode::v;
        case 11: return Widgets::KeyCode::b;
        case 12: return Widgets::KeyCode::q;
        case 13: return Widgets::KeyCode::w;
        case 14: return Widgets::KeyCode::e;
        case 15: return Widgets::KeyCode::r;
        case 16: return Widgets::KeyCode::y;
        case 17: return Widgets::KeyCode::t;
        case 18: return Widgets::KeyCode::digit_1;
        case 19: return Widgets::KeyCode::digit_2;
        case 20: return Widgets::KeyCode::digit_3;
        case 21: return Widgets::KeyCode::digit_4;
        case 22: return Widgets::KeyCode::digit_6;
        case 23: return Widgets::KeyCode::digit_5;
        case 25: return Widgets::KeyCode::digit_9;
        case 26: return Widgets::KeyCode::digit_7;
        case 28: return Widgets::KeyCode::digit_8;
        case 29: return Widgets::KeyCode::digit_0;
        case 31: return Widgets::KeyCode::o;
        case 32: return Widgets::KeyCode::u;
        case 34: return Widgets::KeyCode::i;
        case 35: return Widgets::KeyCode::p;
        case 36: return Widgets::KeyCode::enter;
        case 37: return Widgets::KeyCode::l;
        case 38: return Widgets::KeyCode::j;
        case 40: return Widgets::KeyCode::k;
        case 45: return Widgets::KeyCode::n;
        case 46: return Widgets::KeyCode::m;
        case 48: return Widgets::KeyCode::tab;
        case 49: return Widgets::KeyCode::space;
        case 51: return Widgets::KeyCode::backspace;
        case 53: return Widgets::KeyCode::escape;
        case 56: return Widgets::KeyCode::left_shift;
        case 57: return Widgets::KeyCode::caps_lock;
        case 58: return Widgets::KeyCode::left_alt;
        case 59: return Widgets::KeyCode::left_ctrl;
        case 60: return Widgets::KeyCode::right_shift;
        case 61: return Widgets::KeyCode::right_alt;
        case 62: return Widgets::KeyCode::right_ctrl;
        case 117: return Widgets::KeyCode::delete_forward;
        case 115: return Widgets::KeyCode::home;
        case 116: return Widgets::KeyCode::page_up;
        case 119: return Widgets::KeyCode::end;
        case 121: return Widgets::KeyCode::page_down;
        case 122: return Widgets::KeyCode::f1;
        case 120: return Widgets::KeyCode::f2;
        case 99:  return Widgets::KeyCode::f3;
        case 118: return Widgets::KeyCode::f4;
        case 96:  return Widgets::KeyCode::f5;
        case 97:  return Widgets::KeyCode::f6;
        case 98:  return Widgets::KeyCode::f7;
        case 100: return Widgets::KeyCode::f8;
        case 101: return Widgets::KeyCode::f9;
        case 109: return Widgets::KeyCode::f10;
        case 103: return Widgets::KeyCode::f11;
        case 111: return Widgets::KeyCode::f12;
        case 123: return Widgets::KeyCode::left;
        case 124: return Widgets::KeyCode::right;
        case 125: return Widgets::KeyCode::down;
        case 126: return Widgets::KeyCode::up;
        default:  return Widgets::KeyCode::unknown;
    }
}

static uint32_t macosModifiersToKeyModifiers(NSEventModifierFlags flags)
{
    uint32_t mods = Widgets::KeyModifiers::none;
    if (flags & NSEventModifierFlagShift)   mods |= Widgets::KeyModifiers::shift;
    if (flags & NSEventModifierFlagControl) mods |= Widgets::KeyModifiers::ctrl;
    if (flags & NSEventModifierFlagOption)  mods |= Widgets::KeyModifiers::alt;
    if (flags & NSEventModifierFlagCommand) mods |= Widgets::KeyModifiers::meta;
    return mods;
}

// Falls back to NSScreen.mainScreen (never nil while any display is
// attached) when `screen` is nil — e.g. before the window has been placed
// on a screen yet — and to a hardcoded 60 pre-macOS 12, where
// maximumFramesPerSecond doesn't exist.
static float macosScreenRefreshHz(NSScreen* screen)
{
    NSScreen* effective = screen ?: NSScreen.mainScreen;
    if (@available(macOS 12.0, *))
        return effective ? static_cast<float>(effective.maximumFramesPerSecond) : 60.0f;
    return 60.0f;
}

- (void)keyDown:(NSEvent*)event
{
    // Global debug shortcut: F12 dumps the widget and render trees
    if (event.keyCode == 111) // F12
    {
        Widgets::WidgetInspector::instance().dumpWidgetTree();
        Widgets::WidgetInspector::instance().dumpRenderObjectTree();
        return;
    }

    // For IME (Input Method Editor) support, we route text input through
    // the NSTextInputClient protocol by calling interpretKeyEvents:.
    // This allows proper handling of dead keys, accented characters, and CJK input.
    //
    // A Command-held keystroke is never text composition input — it's
    // always an app/system command by macOS convention — so it must reach
    // FocusManager::handleKeyEvent() (below) even while a text field has
    // IME's input target, or app-wide Cmd+<key> shortcuts (see
    // FocusManager::setGlobalKeyHandler()) would silently stop firing
    // the moment any TextField gains focus.
    const BOOL hasCommandModifier = (event.modifierFlags & NSEventModifierFlagCommand) != 0;

    if (_textInputManager && _textInputManager->hasInputTarget() && !hasCommandModifier)
    {
        // Check if this is a special key that should NOT go through IME
        // Arrow keys, Escape, Enter, Tab, Function keys, etc. should be handled directly
        Widgets::KeyCode keyCode = macosKeyCodeToKeyCode(event.keyCode);
        
        BOOL isNavigationKey = (keyCode == Widgets::KeyCode::left ||
                                keyCode == Widgets::KeyCode::right ||
                                keyCode == Widgets::KeyCode::up ||
                                keyCode == Widgets::KeyCode::down ||
                                keyCode == Widgets::KeyCode::home ||
                                keyCode == Widgets::KeyCode::end ||
                                keyCode == Widgets::KeyCode::page_up ||
                                keyCode == Widgets::KeyCode::page_down);
        
        BOOL isSpecialKey = (keyCode == Widgets::KeyCode::escape ||
                             keyCode == Widgets::KeyCode::tab ||
                             keyCode == Widgets::KeyCode::enter ||
                             keyCode == Widgets::KeyCode::backspace ||
                             keyCode == Widgets::KeyCode::delete_forward ||
                             keyCode == Widgets::KeyCode::f1 ||
                             keyCode == Widgets::KeyCode::f2 ||
                             keyCode == Widgets::KeyCode::f3 ||
                             keyCode == Widgets::KeyCode::f4 ||
                             keyCode == Widgets::KeyCode::f5 ||
                             keyCode == Widgets::KeyCode::f6 ||
                             keyCode == Widgets::KeyCode::f7 ||
                             keyCode == Widgets::KeyCode::f8 ||
                             keyCode == Widgets::KeyCode::f9 ||
                             keyCode == Widgets::KeyCode::f10 ||
                             keyCode == Widgets::KeyCode::f11 ||
                             keyCode == Widgets::KeyCode::f12);
        
        // If it's a navigation or special key, handle directly
        if (isNavigationKey || isSpecialKey)
        {
            if (_focusManager)
            {
                Widgets::KeyEvent ke;
                ke.kind      = event.isARepeat ? Widgets::KeyEventKind::repeat
                                               : Widgets::KeyEventKind::down;
                ke.key_code  = keyCode;
                ke.modifiers = macosModifiersToKeyModifiers(event.modifierFlags);
                ke.character = 0;
                _focusManager->handleKeyEvent(ke);
            }
            return;
        }
        
        // Route text input through NSTextInputClient protocol
        // This will call setMarkedText:, insertText:, unmarkText, etc.
        [self interpretKeyEvents:@[event]];
        return;
    }
    
    // No text input target, use regular key handling
    if (_focusManager)
    {
        Widgets::KeyEvent ke;
        ke.kind      = event.isARepeat ? Widgets::KeyEventKind::repeat
                                       : Widgets::KeyEventKind::down;
        ke.key_code  = macosKeyCodeToKeyCode(event.keyCode);
        ke.modifiers = macosModifiersToKeyModifiers(event.modifierFlags);
        NSString* chars = event.characters;
        ke.character = (chars.length > 0) ? (uint32_t)[chars characterAtIndex:0] : 0u;
        _focusManager->handleKeyEvent(ke);
    }
}

- (void)keyUp:(NSEvent*)event
{
    if (!_focusManager) return;
    Widgets::KeyEvent ke;
    ke.kind      = Widgets::KeyEventKind::up;
    ke.key_code  = macosKeyCodeToKeyCode(event.keyCode);
    ke.modifiers = macosModifiersToKeyModifiers(event.modifierFlags);
    ke.character = 0;
    _focusManager->handleKeyEvent(ke);
}

- (void)scrollWheel:(NSEvent*)event
{
    if (!_dispatcher) return;
    const Widgets::Offset pos = [self pointerOffsetForEvent:event];
    // Scroll deltas are in logical pixels (points), matching pointer coordinates.
    // Note: On macOS, scrollingDeltaY is positive when scrolling down (with Natural
    // Scrolling enabled). We negate the values to match the framework's expectation
    // that positive delta = scroll up (consistent with Windows and the internal
    // applyScrollDelta logic which adds the delta to the scroll offset).
    Widgets::PointerEvent e;
    e.kind           = Widgets::PointerEventKind::scroll;
    e.pointer_id     = 0;
    e.position       = pos;
    e.pressure       = 0.0f;
    e.device_kind    = Widgets::PointerDeviceKind::mouse;
    e.scroll_delta_x = -(float)event.scrollingDeltaX;
    e.scroll_delta_y = -(float)event.scrollingDeltaY;
    _dispatcher->handlePointerEvent(e);
}

// ============================================================================
// NSTextInputClient Protocol Implementation (for IME support)
// ============================================================================

/**
 * Returns YES if the receiver has marked text (is in the middle of a composition).
 * The marked text is the text currently being composed by the IME.
 */
- (BOOL)hasMarkedText
{
    if (!_textInputManager) return NO;
    return _textInputManager->isComposing();
}

/**
 * Returns the range of the marked text (composition in progress).
 * Returns {NSNotFound, 0} if there is no marked text.
 */
- (NSRange)markedRange
{
    if (!_textInputManager) return NSMakeRange(NSNotFound, 0);
    
    auto* controller = _textInputManager->activeController();
    if (!controller || !controller->isComposing())
    {
        return NSMakeRange(NSNotFound, 0);
    }
    
    int start = controller->composingStart();
    int end = controller->composingEnd();
    return NSMakeRange(static_cast<NSUInteger>(start), static_cast<NSUInteger>(end - start));
}

/**
 * Returns the range of the selected text.
 */
- (NSRange)selectedRange
{
    if (!_textInputManager) return NSMakeRange(NSNotFound, 0);
    
    auto* controller = _textInputManager->activeController();
    if (!controller) return NSMakeRange(NSNotFound, 0);
    
    int start = controller->selectionStart();
    int end = controller->selectionEnd();
    return NSMakeRange(static_cast<NSUInteger>(start), static_cast<NSUInteger>(end - start));
}

/**
 * Replaces a specified range in the receiver's text storage with the given string
 * and sets the selection.
 * 
 * This is called by the IME to update the composition text as the user types
 * dead keys or uses CJK input methods.
 */
- (void)setMarkedText:(id)string
        selectedRange:(NSRange)selectedRange
       replacementRange:(NSRange)replacementRange
{
    if (!_textInputManager) return;
    
    // Extract the string
    NSString* markedText;
    if ([string isKindOfClass:[NSAttributedString class]])
    {
        markedText = [(NSAttributedString*)string string];
    }
    else
    {
        markedText = (NSString*)string;
    }
    
    // Check if this is a standalone dead key character
    // Dead keys are accent characters that should not be inserted standalone
    if (markedText.length == 1)
    {
        unichar c = [markedText characterAtIndex:0];
        BOOL isDeadKey = (c == 0x00B4 || // ´ (acute accent)
                          c == 0x0060 || // ` (grave accent)
                          c == 0x005E || // ^ (circumflex)
                          c == 0x007E || // ~ (tilde)
                          c == 0x00A8 || // ¨ (diaeresis/umlaut)
                          c == 0x02C6 || // ˆ (modifier letter circumflex)
                          c == 0x02DC || // ˜ (small tilde)
                          c == 0x02D9 || // ˙ (dot above)
                          c == 0x00B8 || // ¸ (cedilla)
                          c == 0x02CA || // ˊ (modifier letter acute accent)
                          c == 0x02CB);  // ˋ (modifier letter grave accent)
        
        if (isDeadKey)
        {
            return;
        }
    }
    
    // Convert to UTF-8 and update composing text
    std::string utf8Text = [markedText UTF8String];
    _textInputManager->updateComposingText(utf8Text);
}

/**
 * Unmarks the marked text.
 * 
 * Called by the IME when the composition is committed (e.g., user presses
 * Return or Space to confirm the composed character).
 */
- (void)unmarkText
{
    if (!_textInputManager) return;
    _textInputManager->commitComposing();
}

/**
 * Returns an array of attribute names recognized by the receiver.
 * We don't support any special attributes for marked text.
 */
- (NSArray*)validAttributesForMarkedText
{
    return @[];
}

/**
 * Returns the first logical boundary rectangle for characters in the given range.
 * 
 * This is used by the IME to position the candidate window (where the user
 * selects from composition options).
 */
- (NSRect)firstRectForCharacterRange:(NSRange)range
                          actualRange:(NSRangePointer)actualRange
{
    if (!_textInputManager) return NSZeroRect;
    
    // Get the rect from the text input manager
    auto rect = _textInputManager->getCharacterRect(static_cast<int>(range.location));
    
    // Convert from view coordinates to window coordinates
    NSRect result = NSMakeRect(rect[0], rect[1], rect[2], rect[3]);
    
    // Adjust for view position
    result = [self convertRect:result toView:nil];
    
    if (actualRange)
    {
        *actualRange = range;
    }
    
    return result;
}

/**
 * Returns an attributed string derived from the given range in the receiver's
 * text storage.
 * 
 * Optional method - we return nil as we don't store attributed text.
 */
- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range
                                                actualRange:(NSRangePointer)actualRange
{
    // We don't support attributed text, return nil
    if (actualRange)
    {
        *actualRange = NSMakeRange(NSNotFound, 0);
    }
    return nil;
}

/**
 * Returns the index of the character whose bounding rectangle includes the given point.
 * 
 * Optional method for precise cursor positioning.
 */
- (NSUInteger)characterIndexForPoint:(NSPoint)point
{
    if (!_textInputManager) return NSNotFound;
    
    // Convert from view coordinates to the coordinate space used by getPositionForPoint
    NSPoint local = [self convertPoint:point fromView:nil];
    int idx = _textInputManager->getPositionForPoint(static_cast<float>(local.x),
                                                      static_cast<float>(local.y));
    return static_cast<NSUInteger>(idx);
}

/**
 * Returns the fraction of the distance from the left side of the character to
 * the right side that a given point lies.
 * 
 * Optional method for precise cursor positioning within characters.
 */
- (CGFloat)fractionOfDistanceThroughGlyphForPoint:(NSPoint)point
{
    return 0.5; // Default: point is in the middle of the character
}

/**
 * Returns the window level of the receiver.
 * 
 * Optional method - we return the normal window level.
 */
- (NSInteger)windowLevel
{
    return NSNormalWindowLevel;
}

/**
 * Invokes the action specified by the given selector.
 * 
 * This is called for special key combinations that the IME doesn't handle.
 */
- (void)doCommandBySelector:(SEL)selector
{
    // Pass through to the responder chain
    [super doCommandBySelector:selector];
}

/**
 * Inserts the given string into the receiver, replacing the specified content.
 * 
 * This is called for direct text insertion (e.g., when the user confirms
 * composition with the Return key, or pastes text).
 */
- (void)insertText:(id)string replacementRange:(NSRange)replacementRange
{
    if (!_textInputManager) return;
    
    // Extract the string from either NSString or NSAttributedString
    NSString* text;
    if ([string isKindOfClass:[NSAttributedString class]])
    {
        text = [(NSAttributedString*)string string];
    }
    else
    {
        text = (NSString*)string;
    }
    
    // Check if this is a standalone dead key character that shouldn't be inserted
    // When a dead key is pressed, macOS may call insertText with just the accent,
    // then immediately start composition. We should skip the standalone accent.
    if (text.length == 1 && !_textInputManager->isComposing())
    {
        unichar c = [text characterAtIndex:0];
        // Check for common accent characters that are typically dead keys
        if (c == 0x00B4 || // ´ (acute accent)
            c == 0x0060 || // ` (grave accent)
            c == 0x005E || // ^ (circumflex)
            c == 0x007E || // ~ (tilde)
            c == 0x00A8 || // ¨ (diaeresis/umlaut)
            c == 0x02C6 || // ˆ (modifier letter circumflex)
            c == 0x02DC || // ˜ (small tilde)
            c == 0x02D9 || // ˙ (dot above)
            c == 0x00B8 || // ¸ (cedilla)
            c == 0x02CA || // ˊ (modifier letter acute accent)
            c == 0x02CB)   // ˋ (modifier letter grave accent)
        {
            return;
        }
    }
    
    // Convert to UTF-8 and insert
    std::string utf8Text = [text UTF8String];
    
    // If we were composing, commit first
    if (_textInputManager->isComposing())
    {
        _textInputManager->commitComposing();
    }
    
    _textInputManager->insertText(utf8Text);
}

@end

// ---------------------------------------------------------------------------
// MTKView delegate — per-frame rendering
// ---------------------------------------------------------------------------

@interface CampelloMTKDelegate : NSObject <MTKViewDelegate>
- (instancetype)initWithDevice:(std::shared_ptr<GPU::Device>)device
                      renderer:(std::shared_ptr<Widgets::Renderer>)renderer;

/// Stops and joins the raster thread immediately. Safe to call defensively
/// from applicationWillTerminate: in case process exit short-circuits
/// normal Cocoa dealloc ordering. Idempotent — -dealloc calls this again.
- (void)stopRasterThread;
@end

@implementation CampelloMTKDelegate {
    std::shared_ptr<GPU::Device>           _device;
    std::shared_ptr<Widgets::Renderer>     _renderer;
    // Declared last so its destructor (stop()+join) runs before _device/
    // _renderer would otherwise be released by ARC's reverse-declaration-
    // order ivar teardown — but -dealloc below resets it explicitly first
    // anyway, so this ordering is belt-and-suspenders, not load-bearing.
    std::unique_ptr<Widgets::RasterThread> _rasterThread;
}

- (instancetype)initWithDevice:(std::shared_ptr<GPU::Device>)device
                      renderer:(std::shared_ptr<Widgets::Renderer>)renderer
{
    if (!(self = [super init])) return nil;
    _device   = device;
    _renderer = renderer;

    _rasterThread = std::make_unique<Widgets::RasterThread>(
        [renderer = _renderer](const Widgets::FramePackage& pkg) {
            renderer->rasterFrame(pkg);
        });
    _rasterThread->start();

    return self;
}

- (void)stopRasterThread
{
    _rasterThread.reset();
}

- (void)dealloc
{
    // Must run before _renderer/_device are released: stop() blocks until
    // any in-flight raster work finishes and joins the worker thread, so
    // no raster call can ever observe a half-destroyed Renderer/Device.
    [self stopRasterThread];
#if !__has_feature(objc_arc)
    [super dealloc];
#endif
}

- (void)drawInMTKView:(MTKView *)view
{
    if (!_renderer) return;

    id<CAMetalDrawable> drawable = view.currentDrawable;
    if (!drawable) return;

    CGFloat scale = view.window.backingScaleFactor;
    CGSize drawableSize = view.drawableSize;

    // Use drawableSize (physical pixels) for both renderer and backend.
    // The Renderer internally divides by DPR to get logical constraints,
    // but the draw commands and viewport must be in the same coordinate system.
    float w = (float)drawableSize.width;
    float h = (float)drawableSize.height;

    // Still UI-thread state: marks the root dirty on change, read back by
    // the next buildFrame()/layoutPass() call on this same thread.
    _renderer->setDevicePixelRatio(static_cast<float>(scale));

    // Note: PointerDispatcher/TickerScheduler are ticked inside
    // Renderer::buildFrame() itself — don't duplicate that here.
    auto colorView = GPU::TextureView::fromNative((__bridge void *)drawable.texture);
    if (!colorView) return;

    auto package = _renderer->buildFrame(w, h);
    if (!package) {
        // buildFrame skipped (widget tree not dirty) — clear the scheduled
        // present so the pointer doesn't dangle into the next frame.
        _device->scheduleNextPresent(nullptr);
        return;
    }

    package->target = colorView;

    // The drawable must stay alive until the raster thread has scheduled
    // its presentation and submitted the command buffer — which may run
    // well past this call's autorelease pool. CFBridgingRetain()/
    // CFBridgingRelease() (the *functions*, not the __bridge_retained cast
    // qualifier) correctly retain/release under both ARC and MRC, so this
    // keeps working unchanged regardless of this file's ARC setting.
    // Tying the retain to the package's shared_ptr<void> ties the
    // drawable's lifetime to the FramePackage itself, not to this stack
    // frame, and releases it automatically (even on an early return inside
    // rasterFrame()) when the package is destroyed.
    package->retained_drawable = std::shared_ptr<void>(
        (void*)CFBridgingRetain(drawable),
        [](void* p) { if (p) CFBridgingRelease(p); });

    if (_rasterThread) {
        _rasterThread->submit(std::move(*package));
    } else {
        _renderer->rasterFrame(*package);
    }
}

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size
{
    // No-op: under the depth-1 UI/raster handoff, at most one stale-sized
    // FramePackage can ever be in flight, and the next drawInMTKView: call
    // simply reads the new drawableSize — there's no separate "current
    // size" state here to invalidate.
    (void)view;
    (void)size;
}

@end

// ---------------------------------------------------------------------------
// Application delegate — creates the window and sets up the GPU
// ---------------------------------------------------------------------------

@interface CampelloAppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@end

@implementation CampelloAppDelegate {
    NSWindow*                             _window;
    CampelloMTKView*                      _metalView;
    CampelloMTKDelegate*                  _mtkDelegate;

    std::shared_ptr<GPU::Device>               _device;
    std::shared_ptr<Widgets::Renderer>         _renderer;
    std::shared_ptr<Widgets::Element>          _rootElement;
    std::shared_ptr<Widgets::PointerDispatcher>  _dispatcher;
    std::shared_ptr<Widgets::FocusManager>       _focusManager;
    std::shared_ptr<Widgets::TextInputManager>   _textInputManager;
    std::unique_ptr<Widgets::TickerScheduler>    _tickerScheduler;
}

- (void)dealloc
{
    // Clean up KVO observer
    if (@available(macOS 12.1, *)) {
        @try {
            [_window.contentView removeObserver:self forKeyPath:@"safeAreaInsets"];
        } @catch (NSException *exception) {
            // Observer may not have been added
        }
    }
    // Null out globals so late FrameScheduler callbacks don't message a
    // deallocated view or use a destroyed renderer.
    gMetalView = nullptr;
    gRenderer.reset();
#if !__has_feature(objc_arc)
    [super dealloc];
#endif
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    (void)notification;

    // Initialize the platform menu delegate
    campello_widgets_initialize_macos_menu_delegate();

    // -----------------------------------------------------------------------
    // Create window
    // -----------------------------------------------------------------------
    NSRect frame = NSMakeRect(0, 0, (CGFloat)gWidth, (CGFloat)gHeight);
    NSWindowStyleMask style =
        NSWindowStyleMaskTitled         |
        NSWindowStyleMaskClosable       |
        NSWindowStyleMaskMiniaturizable |
        NSWindowStyleMaskResizable;

    _window = [[NSWindow alloc] initWithContentRect:frame
                                          styleMask:style
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    _window.title = [NSString stringWithUTF8String:gTitle.c_str()];

    // -----------------------------------------------------------------------
    // Create Metal view
    // -----------------------------------------------------------------------
    id<MTLDevice> mtlDevice = MTLCreateSystemDefaultDevice();
    if (!mtlDevice) {
        NSLog(@"campello_widgets: Metal is not available on this device");
        return;
    }

    _metalView = [[CampelloMTKView alloc] initWithFrame:frame device:mtlDevice];
    gMetalView = _metalView;  // Store globally for requestRefresh()
    _metalView.colorPixelFormat         = MTLPixelFormatBGRA8Unorm;
    _metalView.depthStencilPixelFormat  = MTLPixelFormatInvalid;
    _metalView.clearColor               = MTLClearColorMake(1.0, 1.0, 1.0, 1.0);
    _metalView.preferredFramesPerSecond = static_cast<NSInteger>(macosScreenRefreshHz(nil));
    _metalView.autoresizingMask         = NSViewWidthSizable | NSViewHeightSizable;

    // On-demand rendering: stop the continuous display link.  Frames are now
    // produced only when setNeedsDisplay:YES is called — matching Flutter's
    // idle behaviour where the GPU is completely quiet when nothing changes.
    _metalView.paused               = YES;
    _metalView.enableSetNeedsDisplay = YES;

    // Register the FrameScheduler callback before mounting the widget tree so
    // that markNeedsPaint() calls triggered during mount already reach the view.
    Widgets::FrameScheduler::setCallback([] {
        if (gMetalView) [gMetalView setNeedsDisplay:YES];
    });

    _window.contentView = _metalView;
    _window.delegate = self;  // For window resize notifications

    // -----------------------------------------------------------------------
    // Use externally-provided device or create one
    // -----------------------------------------------------------------------
    if (gExternalDevice) {
        _device = gExternalDevice;
        NSLog(@"campello_widgets: using externally-provided device=%s",
              _device->getName().c_str());
    } else {
        _device = GPU::Device::createDefaultDevice(nullptr);
        if (!_device) {
            NSLog(@"campello_widgets: failed to create campello_gpu device");
            return;
        }
        NSLog(@"campello_widgets: device=%s  engine=%s",
              _device->getName().c_str(),
              GPU::Device::getEngineVersion().c_str());
    }

    // -----------------------------------------------------------------------
    // Create PointerDispatcher and FocusManager early so render objects can
    // register during widget tree mount.
    // -----------------------------------------------------------------------
    _dispatcher = std::make_shared<Widgets::PointerDispatcher>();
    Widgets::PointerDispatcher::setActiveDispatcher(_dispatcher.get());
    [_metalView setDispatcher:_dispatcher.get()];

    _focusManager = std::make_shared<Widgets::FocusManager>();
    Widgets::FocusManager::setActiveManager(_focusManager.get());
    [_metalView setFocusManager:_focusManager.get()];

    _textInputManager = std::make_shared<Widgets::TextInputManager>();
    Widgets::TextInputManager::setActiveManager(_textInputManager.get());
    [_metalView setTextInputManager:_textInputManager.get()];

    _tickerScheduler = std::make_unique<Widgets::TickerScheduler>();
    Widgets::TickerScheduler::setActive(_tickerScheduler.get());

    // -----------------------------------------------------------------------
    // Wrap root widget with MediaQuery and mount the element tree
    // -----------------------------------------------------------------------
    Widgets::MediaQueryData mediaData;
    mediaData.device_pixel_ratio = static_cast<float>(_window.backingScaleFactor);
    mediaData.platform_brightness = getSystemBrightness();
    mediaData.logical_size = Widgets::Size{
        static_cast<float>(_metalView.bounds.size.width),
        static_cast<float>(_metalView.bounds.size.height) };
    if (@available(macOS 12.1, *)) {
        NSEdgeInsets safeInsets = _window.contentView.safeAreaInsets;
        mediaData.padding.left   = static_cast<float>(safeInsets.left);
        mediaData.padding.top    = static_cast<float>(safeInsets.top);
        mediaData.padding.right  = static_cast<float>(safeInsets.right);
        mediaData.padding.bottom = static_cast<float>(safeInsets.bottom);
    }
    gMediaData = mediaData;
    
    // Wrap the root widget with MediaQuery
    auto wrappedRoot = Widgets::mw<Widgets::MediaQuery>(
        mediaData, gRootWidget);
    
    Widgets::ThreadChecker::instance().bindToCurrentThread();

    _rootElement = wrappedRoot->createElement();
    _rootElement->mount(nullptr);

    Widgets::WidgetInspector::instance().setRootElement(_rootElement);

    auto* roe = _rootElement->findDescendantRenderObjectElement();
    if (!roe) {
        NSLog(@"campello_widgets: widget tree produced no RenderObjectElement");
        return;
    }

    auto renderBox = std::dynamic_pointer_cast<Widgets::RenderBox>(
        roe->sharedRenderObject());
    if (!renderBox) {
        NSLog(@"campello_widgets: root render object is not a RenderBox");
        return;
    }

    // Now that the root RenderBox is known, wire it into the dispatcher.
    _dispatcher->setRoot(renderBox);

    // -----------------------------------------------------------------------
    // Create MetalDrawBackend + Renderer
    // -----------------------------------------------------------------------
    const Widgets::Color bgColor = Widgets::Color::white();
    const GPU::PixelFormat pixelFmt = GPU::PixelFormat::bgra8unorm;

    auto backendOwned = std::make_unique<Widgets::MetalDrawBackend>(
        _device, bgColor, pixelFmt);

    _renderer = std::make_shared<Widgets::Renderer>(
        _device, renderBox, bgColor);
    _renderer->setDrawBackend(std::move(backendOwned));
    gRenderer = _renderer;  // Store globally for requestRefresh()

    // Seeds the performance overlay's budget line with this window's
    // actual starting screen — windowDidChangeScreen: below keeps it
    // correct if the window is later dragged to a different display.
    _renderer->setDisplayRefreshHz(macosScreenRefreshHz(_window.screen));

    // -----------------------------------------------------------------------
    // Wire up appearance change detection
    // -----------------------------------------------------------------------
    [_metalView setAppearanceChangedCallback:^() {
        [self updateMediaQueryBrightness];
    }];

    // -----------------------------------------------------------------------
    // Wire up the MTKView delegate
    // -----------------------------------------------------------------------
    _mtkDelegate = [[CampelloMTKDelegate alloc]
        initWithDevice:_device
              renderer:_renderer];
    _metalView.delegate = _mtkDelegate;

    // -----------------------------------------------------------------------
    // Set up safe area handling
    // -----------------------------------------------------------------------
    [self updateSafeAreaInsets];
    
    // Observe safe area changes on macOS 12.1+
    if (@available(macOS 12.1, *)) {
        [_window.contentView addObserver:self
                              forKeyPath:@"safeAreaInsets"
                                 options:NSKeyValueObservingOptionNew
                                 context:nullptr];
    }

    // -----------------------------------------------------------------------
    // Show window
    // -----------------------------------------------------------------------
    [_window center];
    [_window makeKeyAndOrderFront:nil];
    [_window setAcceptsMouseMovedEvents:YES];
    [NSApp activateIgnoringOtherApps:YES];

    // Register the macOS cursor handler
    Widgets::registerCursorHandler([](Widgets::SystemMouseCursor c) {
        NSCursor* cursor = nil;
        switch (c) {
            case Widgets::SystemMouseCursor::pointer:
                cursor = [NSCursor pointingHandCursor]; break;
            case Widgets::SystemMouseCursor::text:
                cursor = [NSCursor IBeamCursor]; break;
            case Widgets::SystemMouseCursor::forbidden:
                cursor = [NSCursor operationNotAllowedCursor]; break;
            case Widgets::SystemMouseCursor::resize_ns:
                cursor = [NSCursor resizeUpDownCursor]; break;
            case Widgets::SystemMouseCursor::resize_ew:
                cursor = [NSCursor resizeLeftRightCursor]; break;
            default:
                cursor = [NSCursor arrowCursor]; break;
        }
        [cursor set];
    });
}



- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
    (void)sender;
    return YES;
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
    (void)notification;
    // Defensive: NSApp terminate: can short-circuit straight to exit()
    // without guaranteeing -dealloc runs on the delegate chain. Stop the
    // raster thread explicitly here too, so it's never left running
    // against a Renderer/Device that's about to be torn down.
    [_mtkDelegate stopRasterThread];
}

// ---------------------------------------------------------------------------
// System brightness / dark-mode handling
// ---------------------------------------------------------------------------

- (void)updateMediaQueryBrightness
{
    Widgets::Brightness newBrightness = getSystemBrightness();
    if (gMediaData.platform_brightness == newBrightness) return;

    gMediaData.platform_brightness = newBrightness;
    [self rebuildRootMediaQuery];
}

- (void)updateWindowMetrics
{
    Widgets::MediaQueryData newData = gMediaData;
    newData.logical_size = Widgets::Size{
        static_cast<float>(_metalView.bounds.size.width),
        static_cast<float>(_metalView.bounds.size.height) };
    if (@available(macOS 12.1, *)) {
        NSEdgeInsets safeInsets = _window.contentView.safeAreaInsets;
        newData.padding.left   = static_cast<float>(safeInsets.left);
        newData.padding.top    = static_cast<float>(safeInsets.top);
        newData.padding.right  = static_cast<float>(safeInsets.right);
        newData.padding.bottom = static_cast<float>(safeInsets.bottom);
    }
    if (newData != gMediaData) {
        gMediaData = newData;
        [self rebuildRootMediaQuery];
    }
}

- (void)rebuildRootMediaQuery
{
    if (!_rootElement) return;

    auto newMediaQuery = Widgets::mw<Widgets::MediaQuery>(
        gMediaData, gRootWidget);

    _rootElement->update(newMediaQuery);
    Widgets::FrameScheduler::scheduleFrame();
}

// ---------------------------------------------------------------------------
// Safe area handling (for notched MacBook Pros and future displays)
// ---------------------------------------------------------------------------

- (void)updateSafeAreaInsets
{
    if (!_renderer) return;
    
    Widgets::EdgeInsets insets = Widgets::EdgeInsets::zero();
    
    // macOS 12.1+ supports safeAreaInsets on NSView
    if (@available(macOS 12.1, *)) {
        // Get the content view's safeAreaInsets
        // These are in logical points already; no need to multiply by scale
        NSEdgeInsets safeInsets = _window.contentView.safeAreaInsets;
        
        insets.left   = static_cast<float>(safeInsets.left);
        insets.top    = static_cast<float>(safeInsets.top);
        insets.right  = static_cast<float>(safeInsets.right);
        insets.bottom = static_cast<float>(safeInsets.bottom);
    }
    
    _renderer->setViewInsets(insets);
}

// NSWindowDelegate method for frame changes
- (void)windowDidResize:(NSNotification *)notification
{
    (void)notification;
    [self updateSafeAreaInsets];
    // Request a redraw so the widget tree lays out at the new size.
    // updateSafeAreaInsets may not mark the tree dirty if insets are unchanged.
    Widgets::FrameScheduler::scheduleFrame();
}

// NSWindowDelegate method fired when the window moves to a different
// display (e.g. dragged from a 60Hz monitor onto a 144Hz one) — AppKit
// already vsync-paces actual presentation to whichever screen the window
// is now on, with no action needed here; this just keeps the performance
// overlay's budget line (Renderer::setDisplayRefreshHz()) and MTKView's
// own pacing hint in sync with that same reality.
- (void)windowDidChangeScreen:(NSNotification *)notification
{
    (void)notification;
    const float hz = macosScreenRefreshHz(_window.screen);
    _metalView.preferredFramesPerSecond = static_cast<NSInteger>(hz);
    if (_renderer) _renderer->setDisplayRefreshHz(hz);
}

// KVO for safeAreaInsets changes (macOS 12.1+)
- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey,id> *)change
                       context:(void *)context
{
    (void)object;
    (void)change;
    (void)context;
    
    if ([keyPath isEqualToString:@"safeAreaInsets"]) {
        [self updateSafeAreaInsets];
    }
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

int runApp(WidgetRef   root_widget,
           const char* title,
           float       width,
           float       height)
{
    gRootWidget = std::move(root_widget);
    gTitle      = title ? title : "campello_widgets";
    gWidth      = width;
    gHeight     = height;

    @autoreleasepool {
        NSApplication *app      = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];

        CampelloAppDelegate *delegate = [[CampelloAppDelegate alloc] init];
        [app setDelegate:delegate];

        [app run];
    }
    return 0;
}

int runApp(std::shared_ptr<campello_gpu::Device> device,
           WidgetRef                             root_widget,
           const char*                           title,
           float                                 width,
           float                                 height)
{
    gExternalDevice = std::move(device);
    int result = runApp(std::move(root_widget), title, width, height);
    gExternalDevice.reset();
    return result;
}

void requestRefresh()
{
    // forceRefresh() calls markNeedsLayout() → markNeedsPaint() which calls
    // FrameScheduler::scheduleFrame() → [gMetalView setNeedsDisplay:YES].
    // No manual draw call needed in on-demand rendering mode.
    if (gRenderer) {
        gRenderer->forceRefresh();
    }
}

} // namespace systems::leal::campello_widgets
