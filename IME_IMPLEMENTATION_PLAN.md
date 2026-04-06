# IME (Input Method Editor) Support Implementation Plan

## Problem Statement

The current TextField widget cannot handle characters that require composition sequences:
- Accented characters: `´` + `e` → `é`
- Diacritical marks: `~` + `n` → `ñ`
- CJK (Chinese/Japanese/Korean) input methods
- Emoji input pickers

This is because the current implementation processes raw keycodes directly rather than interacting with the platform's Input Method Editor (IME).

---

## How Flutter Handles IME

Flutter's approach (which we should mirror):

### 1. **TextInputModel with Composing Range**
- Stores: text, selection, **composing range**
- Composing range = the range of text currently being composed
- When composing: `composing_range = (start, end)`
- When not composing: `composing_range = (-1, -1)` or empty

### 2. **Platform-Specific Text Input Protocols**
- **macOS**: Implements `NSTextInputClient` protocol
- **iOS**: Implements `UITextInput` protocol  
- **Windows**: Handles `WM_IME_COMPOSITION`, `WM_IME_STARTCOMPOSITION`, etc.
- **Linux**: Uses GTK's `GtkIMContext` or IBus/D-Bus

### 3. **Key Protocol Methods (macOS NSTextInputClient)**
```objc
// Called when composition starts/updates
- (void)setMarkedText:(id)string
        selectedRange:(NSRange)selectedRange
       replacementRange:(NSRange)replacementRange;

// Called when composition is committed
- (void)unmarkText;

// Called for direct text insertion
- (void)insertText:(id)string
    replacementRange:(NSRange)replacementRange;

// Query methods
- (BOOL)hasMarkedText;
- (NSRange)markedRange;
- (NSRange)selectedRange;
```

### 4. **TextEditingDelta Model**
Flutter sends delta updates rather than full text state:
```dart
{
  "oldText": "hello",
  "deltaText": "é",
  "deltaStart": 5,
  "deltaEnd": 6,
  "selectionBase": 6,
  "selectionExtent": 6,
  "composingBase": 5,   // -1 when not composing
  "composingExtent": 6  // -1 when not composing
}
```

---

## Implementation Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Platform Layer                            │
│  ┌─────────────┐  ┌──────────────┐  ┌──────────────────┐   │
│  │  macOS      │  │   Windows    │  │   Linux          │   │
│  │  MTKView    │  │   HWND       │  │   GTK/X11        │   │
│  │  +          │  │   +          │  │   +              │   │
│  │NSTextInput  │  │  IMM32       │  │  GtkIMContext    │   │
│  │  Client     │  │              │  │                  │   │
│  └──────┬──────┘  └──────┬───────┘  └────────┬─────────┘   │
└─────────┼────────────────┼───────────────────┼─────────────┘
          │                │                   │
          └────────────────┴───────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│                TextInputConnection                           │
│         (Bridge between platform and widget)                 │
└─────────────────────────┬───────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│              TextEditingController                           │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  - text: string                                     │   │
│  │  - selection: (start, end)                          │   │
│  │  - composing_range: (start, end)  ← NEW!            │   │
│  │  - is_composing: bool                               │   │
│  │                                                     │   │
│  │  - beginComposing()                                 │   │
│  │  - updateComposingText(text, selection)             │   │
│  │  - commitComposing()                                │   │
│  │  - cancelComposing()                                │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────┬───────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                RenderTextField                               │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  - Render text with composing region underlined     │   │
│  │  - Handle cursor/selection within composing region  │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

---

## Detailed Implementation Phases

### Phase 1: Core Data Model (TextEditingController)

**File**: `inc/campello_widgets/ui/text_editing_controller.hpp`

```cpp
class TextEditingController {
public:
    // Existing methods...
    
    // NEW: Composing region (IME composition state)
    bool isComposing() const noexcept;
    int composingStart() const noexcept;
    int composingEnd() const noexcept;
    
    // NEW: Composing operations
    void beginComposing();
    void setComposingRange(int start, int end);
    void updateComposingText(std::string_view text);
    void commitComposing();  // Accept the composed text
    void cancelComposing();  // Reject the composed text
    
private:
    std::string text_;
    int selection_start_ = 0;
    int selection_end_ = 0;
    
    // NEW fields
    int composing_start_ = -1;  // -1 means not composing
    int composing_end_ = -1;
};
```

**Behavior**:
- `beginComposing()`: Sets `composing_start_ = composing_end_ = selection_end_`
- `updateComposingText(text)`: Replaces the composing range with new text
- `commitComposing()`: Sets `composing_start_ = composing_end_ = -1` (composition done)
- `cancelComposing()`: Deletes the composing range, restores selection

---

### Phase 2: Platform Integration - macOS

**File**: `src/macos/run_app.mm` - Modify `CampelloMTKView`

```objc
@interface CampelloMTKView : MTKView <NSTextInputClient>
// ... existing code ...
@end

@implementation CampelloMTKView

// MARK: - NSTextInputClient Protocol Methods

// Called when user starts typing a composed character
- (void)setMarkedText:(id)string 
        selectedRange:(NSRange)selectedRange 
       replacementRange:(NSRange)replacementRange {
    
    NSString* text = [string isKindOfClass:[NSAttributedString class]] 
                     ? [(NSAttributedString*)string string] 
                     : string;
    
    // Get the focused text field's controller
    auto* controller = GetFocusedTextController();
    if (!controller) return;
    
    // Start composing if not already
    if (!controller->isComposing()) {
        controller->beginComposing();
    }
    
    // Update the composing text
    std::string utf8 = [text UTF8String];
    controller->updateComposingText(utf8);
    
    // Update selection within the composing text
    int selStart = static_cast<int>(selectedRange.location);
    int selEnd = selStart + static_cast<int>(selectedRange.length);
    controller->setSelection(
        controller->composingStart() + selStart,
        controller->composingStart() + selEnd
    );
    
    NotifyTextChanged();
}

// Called when composition is finalized (e.g., user pressed Space or Enter)
- (void)unmarkText {
    auto* controller = GetFocusedTextController();
    if (!controller) return;
    
    controller->commitComposing();
    NotifyTextChanged();
}

// Direct text insertion (bypasses composition)
- (void)insertText:(id)string replacementRange:(NSRange)replacementRange {
    NSString* text = [string isKindOfClass:[NSAttributedString class]] 
                     ? [(NSAttributedString*)string string] 
                     : string;
    
    auto* controller = GetFocusedTextController();
    if (!controller) return;
    
    // If we were composing, commit first
    if (controller->isComposing()) {
        controller->commitComposing();
    }
    
    // Insert the text
    controller->insertText([text UTF8String]);
    NotifyTextChanged();
}

- (BOOL)hasMarkedText {
    auto* controller = GetFocusedTextController();
    return controller && controller->isComposing();
}

- (NSRange)markedRange {
    auto* controller = GetFocusedTextController();
    if (!controller || !controller->isComposing()) {
        return NSMakeRange(NSNotFound, 0);
    }
    return NSMakeRange(
        static_cast<NSUInteger>(controller->composingStart()),
        static_cast<NSUInteger>(controller->composingEnd() - controller->composingStart())
    );
}

- (NSRange)selectedRange {
    auto* controller = GetFocusedTextController();
    if (!controller) return NSMakeRange(NSNotFound, 0);
    
    return NSMakeRange(
        static_cast<NSUInteger>(controller->selectionStart()),
        static_cast<NSUInteger>(controller->selectionEnd() - controller->selectionStart())
    );
}

// Return the bounding rect for a character range (for IME candidate window positioning)
- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actualRange {
    // Query RenderTextField for the rect of the given character range
    auto* renderField = GetFocusedRenderTextField();
    if (!renderField) return NSZeroRect;
    
    auto rect = renderField->getRectForCharacterRange(range.location, 
                                                       range.location + range.length);
    
    // Convert from view coordinates to window coordinates
    NSRect nsRect = NSMakeRect(rect.x, rect.y, rect.width, rect.height);
    return [self convertRect:nsRect toView:nil];
}

// Other required methods...
- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range actualRange:(NSRangePointer)actualRange {
    return nil;  // Simplified - return nil if not supporting attributed strings
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point {
    // Convert point to text position
    return NSNotFound;  // Implement if needed
}

- (NSArray*)validAttributesForMarkedText {
    return @[];  // No special attributes for marked text
}

- (void)doCommandBySelector:(SEL)selector {
    // Pass through to normal key handling
    [super doCommandBySelector:selector];
}

@end
```

**Key Changes to MTKView**:
1. Declare `<NSTextInputClient>` protocol conformance
2. Implement the 8 required methods
3. Disable the old `keyDown:` handler when IME is active
4. Add a way to get the focused TextField's controller

---

### Phase 3: Visual Feedback

**File**: `src/ui/render_text_field.cpp`

The composing text needs visual distinction (typically underlined):

```cpp
void RenderTextField::paint(PaintContext& context) {
    // ... existing paint code ...
    
    // Paint text with composing region highlighted
    const auto& text = controller_->text();
    int composeStart = controller_->composingStart();
    int composeEnd = controller_->composingEnd();
    
    // Split into: before_composing | composing | after_composing
    std::string_view before(text.data(), composeStart >= 0 ? composeStart : text.size());
    std::string_view composing;
    std::string_view after;
    
    if (composeStart >= 0 && composeEnd > composeStart) {
        composing = std::string_view(
            text.data() + composeStart,
            composeEnd - composeStart
        );
        after = std::string_view(
            text.data() + composeEnd,
            text.size() - composeEnd
        );
    }
    
    // Paint each segment
    float x = content_rect_.x + padding_;
    float y = content_rect_.y + (content_rect_.height - line_height_) / 2;
    
    // 1. Paint text before composing (normal)
    if (!before.empty()) {
        context.canvas->drawText(before, x, y, style_, text_color_);
        x += measureText(before, style_);
    }
    
    // 2. Paint composing text (underlined)
    if (!composing.empty()) {
        // Draw underline
        float composeWidth = measureText(composing, style_);
        context.canvas->drawLine(
            x, y + 2,  // 2px below baseline
            x + composeWidth, y + 2,
            underline_paint_  // typically gray or blue
        );
        
        // Draw composing text
        context.canvas->drawText(composing, x, y, style_, text_color_);
        x += composeWidth;
    }
    
    // 3. Paint text after composing (normal)
    if (!after.empty()) {
        context.canvas->drawText(after, x, y, style_, text_color_);
    }
    
    // ... rest of paint code ...
}
```

---

### Phase 4: Windows Support

**File**: `src/windows/run_app.cpp`

Windows requires IMM (Input Method Manager) integration:

```cpp
// In window procedure:
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_IME_STARTCOMPOSITION:
            // Composition started - disable direct key input
            if (auto* ctrl = GetFocusedController()) {
                ctrl->beginComposing();
            }
            return 0;
            
        case WM_IME_COMPOSITION:
            if (lparam & GCS_COMPSTR) {
                // Get the composition string
                HIMC hIMC = ImmGetContext(hwnd);
                if (hIMC) {
                    wchar_t compStr[256];
                    DWORD len = ImmGetCompositionStringW(hIMC, GCS_COMPSTR, 
                                                          compStr, sizeof(compStr));
                    if (auto* ctrl = GetFocusedController()) {
                        ctrl->updateComposingText(utf16_to_utf8(compStr, len/2));
                    }
                    ImmReleaseContext(hwnd, hIMC);
                }
            }
            if (lparam & GCS_RESULTSTR) {
                // Composition committed
                HIMC hIMC = ImmGetContext(hwnd);
                if (hIMC) {
                    wchar_t resultStr[256];
                    DWORD len = ImmGetCompositionStringW(hIMC, GCS_RESULTSTR,
                                                          resultStr, sizeof(resultStr));
                    if (auto* ctrl = GetFocusedController()) {
                        ctrl->commitComposing();
                        ctrl->insertText(utf16_to_utf8(resultStr, len/2));
                    }
                    ImmReleaseContext(hwnd, hIMC);
                }
            }
            return 0;
            
        case WM_IME_ENDCOMPOSITION:
            // Clean up if composition was cancelled
            return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}
```

---

## Modified Key Handling Flow

### Current Flow (Broken for IME):
```
User presses ´ key
    ↓
keyDown: event in MTKView
    ↓
event.character = 0 (´ is a dead key, not a printable char)
    ↓
KeyEvent with character=0 sent to FocusManager
    ↓
TextField ignores it (character == 0 check)
    ↓
NO TEXT INSERTED ❌
```

### New Flow (With IME):
```
User presses ´ key (dead key)
    ↓
NSTextInputContext intercepts it (setMarkedText called)
    ↓
setMarkedText:@"´" selectedRange:(0,0)
    ↓
Controller.beginComposing() → composing range set
    ↓
Controller.updateComposingText("´")
    ↓
Text shows with underline
    ↓
User presses e key
    ↓
NSTextInputContext calls setMarkedText:@"é"
    ↓
Controller.updateComposingText("é")
    ↓
User presses Space or Enter to confirm
    ↓
NSTextInputContext calls unmarkText
    ↓
Controller.commitComposing()
    ↓
Composition complete, é is now regular text ✓
```

---

## Implementation Timeline

| Phase | Description | Estimated Effort |
|-------|-------------|------------------|
| 1 | TextEditingController with composing range | 1 day |
| 2 | macOS NSTextInputClient integration | 2-3 days |
| 3 | Visual feedback (underline composing text) | 1 day |
| 4 | Windows IMM integration | 2-3 days |
| 5 | Linux/GTK integration | 2-3 days |
| 6 | Testing with various IMEs | 2 days |

**Total: ~2 weeks for full cross-platform support**

---

## Testing Checklist

### macOS
- [ ] Spanish: `´` + `e` → `é`
- [ ] French: `` ` `` + `a` → `à`
- [ ] German: `"` + `u` → `ü`
- [ ] Japanese Hiragana input
- [ ] Chinese Pinyin input
- [ ] Emoji picker (Ctrl+Cmd+Space)

### Windows
- [ ] Same accent tests as macOS
- [ ] Korean input
- [ ] Windows IME for Chinese/Japanese

### Linux
- [ ] IBus integration
- [ ] Fcitx integration

---

## References

1. **Flutter Engine Source**:
   - `shell/platform/darwin/macos/framework/Source/FlutterTextInputPlugin.mm`
   - `shell/platform/windows/text_input_plugin.cc`
   - `shell/common/text_input_model.cc`

2. **Apple Documentation**:
   - [NSTextInputClient Protocol](https://developer.apple.com/documentation/appkit/nstextinputclient)

3. **Windows Documentation**:
   - [Input Method Manager (IMM)](https://docs.microsoft.com/en-us/windows/win32/intl/input-method-manager)

4. **GTK Documentation**:
   - [GtkIMContext](https://docs.gtk.org/gtk3/class.IMContext.html)
