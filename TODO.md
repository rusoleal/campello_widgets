# campello_widgets — Development Plan

Phases are ordered by dependency. Complete each phase before starting the next.
Items within a phase can be parallelised where noted.

---

## Phase 1 — Project Scaffolding

- [x] Set up CMakeLists.txt with C++20 standard
- [x] Add `campello_gpu` as a CMake dependency
- [x] Add `campello_input` as a CMake dependency
- [x] Define directory layout (`inc/`, `src/`, `tests/`, `examples/`, `dependencies/`)
- [x] Set up CI pipeline (Windows, macOS, Linux builds)
- [x] Add platform detection macros / abstractions

---

## Phase 2 — Core Widget Infrastructure

The foundation everything else builds on.

- [x] Define `Widget` base class (immutable, ref-counted)
- [x] Define `WidgetRef` — smart pointer alias used throughout the API
- [x] Define `BuildContext` — provides access to the current element in the tree
- [x] Define `StatelessWidget` — `build(BuildContext&)` interface
- [x] Define `StatefulWidget` + `State<T>` pair
  - [x] `setState()` triggers a rebuild of the subtree
  - [x] Lifecycle hooks: `initState`, `dispose`, `didUpdateWidget`
- [x] Define `Element` base class and specialisations
  - [x] `StatelessElement`
  - [x] `StatefulElement`
  - [x] `RenderObjectElement`
- [x] Widget tree reconciliation (diff old tree vs new tree, reuse elements)

---

## Phase 3 — Layout System

- [x] Define `BoxConstraints` (minWidth, maxWidth, minHeight, maxHeight)
- [x] Define `Size` and `Offset` value types
- [x] Define `EdgeInsets` (padding/margin helper)
- [x] Define `RenderObject` base class
  - [x] `layout(BoxConstraints)` virtual method
  - [x] `paint(PaintContext&)` virtual method
  - [x] Dirty flags (needs layout, needs paint)
- [x] Define `RenderBox` — concrete RenderObject for box-model widgets
- [x] Define `RenderObjectWidget` — bridge between Widget and RenderObject trees
- [x] Layout pass: top-down constraint propagation
- [x] Size reporting: bottom-up size resolution

---

## Phase 4 — Rendering Pipeline

Requires Phase 3 and a working `campello_gpu` integration.

- [x] Define `PaintContext` — wraps a `campello_gpu` RenderPassEncoder, transform/clip stacks, DrawCommand queue
- [x] Implement dirty-region tracking (avoid full repaints)
- [x] Implement layer compositing via `campello_gpu`
- [x] Frame loop: layout pass -> paint pass -> GPU submit (`Renderer`)
- [x] Clip and transform support in the render tree

---

## Phase 5 — Basic Render Widgets

Leaf widgets that talk directly to the render layer.

- [x] `RawRectangle` — filled/stroked rect with colour and corner radius
- [x] `RawText` — text rendering via GPU font atlas (stub measurement; real metrics in text phase)
- [x] `RawImage` — renders a `campello_gpu` texture
- [x] `RawCustomPaint` — exposes `PaintContext` to user code via `CustomPainter` interface

---

## Phase 6 — Composited Widgets

Higher-level widgets built by composing the basics.

- [x] `SizedBox`
- [x] `Padding`
- [x] `Align`
- [x] `Center` (shorthand for `Align` with center alignment)
- [x] `Container` (padding + decoration + child)
- [x] `Row`
  - [x] `MainAxisAlignment`, `CrossAxisAlignment`
  - [x] Flex / Expanded children
- [x] `Column` (same as Row, vertical)
- [x] `Stack` + `Positioned`
- [x] `Text` (wraps `RawText` with text style)
- [x] `Image` (wraps `RawImage`)
- [x] `Scaffold` (root layout structure)
- [x] `Transform` — applies a `Matrix4` (rotate, scale, translate) to a child; pivot via `Alignment`; layout-transparent

---

## Phase 7 — Input Handling

Requires Phase 2 (widget tree) and `campello_input` integration.

- [x] Connect platform pointer events to the widget framework (`PointerDispatcher`, macOS wired via `CampelloMTKView`)
- [x] Hit-testing pass on the RenderObject tree for pointer events (`HitTestResult`, `RenderBox::hitTest`)
- [x] Define `GestureRecognizer` base class
- [x] Implement recognizers:
  - [x] `TapGestureRecognizer` (built into `GestureDetector`)
  - [x] `DoubleTapGestureRecognizer` (built into `GestureDetector`)
  - [x] `LongPressGestureRecognizer` (built into `GestureDetector`, tick-driven)
  - [x] `PanGestureRecognizer` (built into `GestureDetector`)
  - [x] `ScrollGestureRecognizer` (built into `GestureDetector`, macOS `scrollWheel:` wired)
- [x] `GestureDetector` widget (tap + pan, registers via `PointerDispatcher`)
- [x] Keyboard focus system
  - [x] `FocusNode` / `Focus` widget
  - [x] Focus traversal (tab order, Tab/Shift+Tab intercept in FocusManager)
- [x] Touch support (multitouch, for iOS / Android)

---

## Phase 8 — Animation System

- [x] `AnimationController` — drives a value over time
- [x] `Tween<T>` — interpolates between two values (float, double, Color, Offset, Size)
- [x] `CurvedAnimation` — applies an easing curve
- [x] `AnimatedBuilder` widget
- [x] `AnimatedContainer` — implicit animation for color/width/height/padding
- [x] `AnimatedOpacity` — opacity baked into draw commands via Canvas opacity stack
- [x] Ticker / vsync integration with the frame loop (`TickerScheduler`)

---

## Phase 9 — Scrolling

- [x] `ScrollController`
- [x] `SingleChildScrollView`
- [x] `ListView` (virtualised, lazy-building children)
- [x] `GridView`
- [x] Scroll physics (momentum, bounce, clamped)

---

## Phase 10 — Platform Integration

- [~] Window / surface creation per platform (delegate to `campello_gpu`) — each platform has its own runner, abstract unification not done
- [x] iOS: UIKit integration with safe area insets
- [x] Android: ANativeWindow integration with safe area insets
- [x] macOS: NSView / CAMetalLayer integration with safe area insets
- [x] Windows: HWND / DXGI integration
- [x] Linux: X11 integration with Vulkan swapchain
- [x] Linux: Wayland integration with `wl_surface` / `xdg_toplevel`
- [ ] Platform channel / FFI abstraction for native calls

---

## Phase 11 — Developer Experience

- [x] Debug overlay (FPS counter, paint size, repaint rainbow, debug banner)
- [~] Hot-reload friendly design (not planned)
- [x] Comprehensive unit tests for layout engine
- [x] Integration test harness (headless rendering) — `GpuVisualRenderer` (Metal, offscreen) with CPU fallback
- [~] Example applications:
  - [x] Hello World
  - [x] Counter app (StatefulWidget demo)
  - [x] List view
  - [x] Animated transitions
  - [x] macOS Showcase (unified demo app)
  - [x] TextField / IME demo
  - [x] Image loading demo
  - [x] Gestures demo
  - [x] TableView demo
  - [x] TreeView demo
  - [x] Keyboard demo
  - [x] PlatformMenu test

---

## Phase 12 — Canvas API (Flutter-compatible)

Drawing API for custom painters and shape rendering.

- [x] Define `Canvas` class with transform/clip state stack
- [x] Drawing primitives:
  - [x] `drawRect` — filled/stroked rectangle
  - [x] `drawCircle` — circle with center and radius
  - [x] `drawOval` — ellipse within bounding rect
  - [x] `drawArc` — arc with start/sweep angles
  - [x] `drawLine` — line segment between two points
  - [x] `drawPath` — custom path rendering
  - [x] `drawRRect` — rounded rectangle
  - [x] `drawDRRect` — double rounded rectangle (outer/inner)
  - [x] `drawPoints` — point cloud / lines
  - [x] `drawColor` — solid color fill with blend mode
- [x] Transform methods:
  - [x] `translate`, `rotate`, `scale`, `skew`
  - [x] Matrix4 integration from vector_math
- [x] Clipping:
  - [x] `clipRect`, `clipPath`, `clipRRect`
- [x] State management:
  - [x] `save`, `restore`, `restoreToCount`, `getSaveCount`
- [x] `Path` class:
  - [x] `moveTo`, `lineTo`, `cubicTo`, `quadraticTo`, `arcTo`, `close`
  - [x] `getBounds`, `contains`, `transform`, `flatten`
- [x] `RRect` (rounded rectangle) with per-corner radii
- [x] `RRectComplex` for per-corner radii control
- [x] `BlendMode` enum (srcOver, modulate, plus, etc.)
- [x] `Paint` struct (color, style, strokeWidth, blendMode)
- [x] DrawCommand variant system for GPU backend integration
- [x] Fidelity tests for Canvas API:
  - [x] Basic shapes (rect, circle, oval)
  - [x] Lines and points
  - [x] Path drawing with curves
  - [x] Rounded rectangles (RRect/RRectComplex)
  - [x] Arcs and pie charts
  - [x] Transforms (translate, rotate, scale)
  - [x] Clipping operations
  - [x] Paint styles (fill, stroke, blend modes)
  - [x] Complex scenes combining multiple operations
  - [x] State management (save/restore)
  - [x] Path operations (bounds, commands)

---

## Phase 13 — Pending Widgets (Flutter Gap Analysis)

Widgets identified as missing after comparing against Flutter's widget catalog.

### State Management / Data Flow
- [x] `InheritedWidget` + `InheritedElement` — context-based data propagation; `BuildContext::dependOnInheritedWidgetOfExactType<T>()`
- [x] `LayoutBuilder` — builds UI based on parent's constraints at layout time
- [x] `ValueListenableBuilder` — rebuilds when a `ValueNotifier<T>` changes
- [x] `FutureBuilder` — builds UI based on the result of a `std::shared_future<T>`
- [x] `StreamBuilder` — builds UI based on a `Stream<T>` / `StreamController<T>`

### Input / Forms
- [x] `TextField` + `TextEditingController` — text input widget
- [x] `Checkbox` — boolean toggle
- [x] `Radio` + `RadioGroup` — single-selection from a group
- [x] `Switch` — on/off toggle
- [x] `Slider` — continuous value selector
- [x] `MouseRegion` — hover detection (enter/exit/move callbacks)
- [x] `Draggable` + `DragTarget` — drag-and-drop protocol

### Layout
- [x] `ConstrainedBox` — imposes additional `BoxConstraints` on a child
- [x] `AspectRatio` — sizes child to a fixed aspect ratio
- [x] `Wrap` — flows children into multiple rows or columns
- [x] `FractionallySizedBox` — sizes child as a fraction of available space
- [x] `IntrinsicWidth` / `IntrinsicHeight` — sizes to child's intrinsic dimensions
- [x] `ClipRect` — clips child to its bounding box
- [x] `ClipRRect` — clips child to a rounded rectangle
- [x] `ClipOval` — clips child to an oval
- [x] `ClipPath` — clips child to an arbitrary `Path`

### Navigation / Routing
- [x] `Navigator` — stack-based screen/route manager
- [x] `Route` / `PageRoute` — represents a single screen/dialog route

### Decoration / Painting
- [x] `DecoratedBox` + `BoxDecoration` — borders, shadows, border-radius (`BoxShadow`, `BoxBorder`, `DecorationPosition`)
- [x] `BackdropFilter` — applies an `ImageFilter` (blur, etc.) behind the child
- [x] `ShaderMask` — applies a shader gradient mask over the child

### Animation (Explicit Transitions)
- [x] `AnimatedSwitcher` — animates between two widgets when the child changes
- [x] `AnimatedSize` — animates its own size when the child's size changes
- [x] `AnimatedPositioned` — implicitly animates `Positioned` properties in a `Stack`
- [x] `AnimatedAlign` — implicitly animates `Alignment`
- [x] `FadeTransition` — explicit opacity transition driven by an `Animation<float>`
- [x] `ScaleTransition` — explicit scale transition
- [x] `SlideTransition` — explicit slide transition (fractional offsets via `FractionalTranslation`)
- [x] `RotationTransition` — explicit rotation transition

### Composited / Utility
- [x] `Button` (base interactive button widget)
- [x] `CircularProgressIndicator` — spinning activity indicator
- [x] `LinearProgressIndicator` — horizontal progress bar
- [x] `Tooltip` — overlay label shown on long-press / hover
- [x] `Divider` — thin horizontal rule

---

## Phase 14 — Logical Pixels

All layout, input, and rendering must operate in logical pixels (device-independent units),
with the device pixel ratio (DPR) applied only at the GPU boundary.

- [x] **Task 1 — Renderer + platform adapters**: Add `device_pixel_ratio` field and `setDevicePixelRatio(float)` to `Renderer`. In `layoutPass()`, divide viewport dimensions by DPR before building `BoxConstraints::tight`. Wire up DPR from `backingScaleFactor` (macOS), `contentScaleFactor` (iOS), and `GetDpiForWindow/96` (Windows) in the platform adapters; update on display/DPI change events.
- [x] **Task 2 — Pointer coordinates**: Remove the `* backingScaleFactor` / `* contentScaleFactor` multiplications from `pointerOffsetForEvent:` (macOS) and `touchOffsetForTouch:` (iOS) so all pointer positions entering `PointerDispatcher` are in logical pixels. Adjust scroll deltas accordingly on Windows.
- [x] **Task 3 — MediaQuery InheritedWidget**: Create `MediaQueryData` struct (`logical_size`, `device_pixel_ratio`, `padding`, `view_insets`) and `MediaQuery` InheritedWidget. Inject it above the root widget in `Renderer`. Add `MediaQuery::of(BuildContext&)` static accessor. Include in umbrella header.
- [x] **Task 4 — Text scaling at paint time**: Multiply `text_style.font_size` by DPR before passing to the draw backend during paint, so text is rasterised at physical resolution. Expose DPR to render objects via `RenderObject::activeDevicePixelRatio()` static (set by `Renderer` around layout/paint passes). Apply to `RenderText` and `RenderParagraph`.
- [x] **Task 5 — Safe area insets**: Remove `* scale` from safe area inset calculations in macOS and iOS platform adapters; insets stored in `Renderer::view_insets_` are now in logical pixels.
- [x] **Task 6 — Update examples**: Review all four macOS examples after the switch; verify or adjust hardcoded dimensions that were tuned for physical pixels.
- [x] **Task 7 — Unit tests**: Add tests verifying `layoutPass(800, 600)` with DPR=2 produces tight constraints of `(400, 300)`; test `MediaQueryData` forwarding; confirm pointer events are not scaled inside the dispatcher.

---

## Phase 15 — Design System

Capa de alto nivel intercambiable entre design systems (Material, Cupertino, custom).
Los widgets adaptativos no conocen el design system activo — simplemente delegan a él vía `Theme::of(ctx)`.

### Arquitectura

```
DesignTokens            — valores crudos (colores, tipografía, espaciado, motion, shape)
DesignSystem            — interfaz abstracta: tokens() + buildXxx(Config) por componente
Theme (InheritedWidget) — propaga el DesignSystem por el árbol
Button, Card, ...       — thin wrappers que llaman a Theme::of(ctx).buildXxx()
```

### Archivos a crear

| Archivo | Contenido |
|---|---|
| `inc/campello_widgets/ui/design_tokens.hpp` | `ColorScheme`, `Typography`, `ShapeTokens`, `SpacingTokens`, `MotionTokens`, `DesignTokens`, `Brightness` |
| `inc/campello_widgets/ui/design_system.hpp` | Config structs + clase abstracta `DesignSystem` |
| `inc/campello_widgets/widgets/theme.hpp` | `Theme : InheritedWidget` con `Theme::of(ctx)` y `Theme::tokensOf(ctx)` |
| `src/widgets/theme.cpp` | Implementación de `Theme` |

Los widgets adaptativos existentes (`Button`, `Card`, `TextField`, `NavigationBar`, ...) se refactorizan para delegar a `Theme::of(ctx).buildXxx()`.

### Tareas

- [x] **Task 1 — DesignTokens**: Crear `inc/campello_widgets/ui/design_tokens.hpp` con `ColorScheme`, `Typography`, `ShapeTokens`, `SpacingTokens`, `MotionTokens`, `DesignTokens`, `Brightness`.
- [x] **Task 2 — DesignSystem**: Crear `inc/campello_widgets/ui/design_system.hpp` con config structs agnósticas (`ButtonConfig`, `TextFieldConfig`, `CardConfig`, `NavigationBarConfig`, ...) y clase abstracta `DesignSystem`.
- [x] **Task 3 — Theme**: Crear `inc/campello_widgets/widgets/theme.hpp` + `src/widgets/theme.cpp`. `Theme : InheritedWidget` con `Theme::of(ctx)` y `Theme::tokensOf(ctx)`.
- [x] **Task 4 — Widgets adaptativos**: Refactorizar `Button`, `Card`, `Divider`, `ListTile`, `AppBar`, `NavigationBar`, `PrimaryActionButton` para ser thin wrappers que llaman a `Theme::of(ctx).buildXxx(config)`. Los widgets con estado complejo (Switch, Checkbox, Radio, Slider, TextField, ProgressIndicator, Tooltip, PopupMenuButton, DropdownButton, TabBar, Dialog, SnackBar) se mantienen como widgets canónicos configurados por el DesignSystem.
- [x] **Task 5 — Implementación custom**: Crear `CampelloDesignSystem : DesignSystem` como primera implementación concreta usando los tokens.

### Diseño de referencia

**DesignTokens:**
```cpp
struct ColorScheme {
    Color primary, on_primary;
    Color secondary, on_secondary;
    Color surface, on_surface;
    Color surface_variant;
    Color outline;
    Color error, on_error;
};
enum class Brightness { light, dark };
struct Typography {
    TextStyle display_large, display_medium;
    TextStyle headline_large, headline_medium;
    TextStyle title_large, title_medium;
    TextStyle body_large, body_medium;
    TextStyle label_large, label_medium;
};
struct ShapeTokens {
    float radius_none=0.f, radius_xs=4.f, radius_sm=8.f,
          radius_md=12.f,  radius_lg=16.f, radius_full=9999.f;
};
struct SpacingTokens {
    float xs=4.f, sm=8.f, md=16.f, lg=24.f, xl=32.f, xxl=48.f;
};
struct MotionTokens {
    Duration duration_fast=100ms, duration_medium=250ms, duration_slow=400ms;
    Curve curve_standard, curve_decelerate, curve_accelerate;
};
struct DesignTokens {
    ColorScheme colors; Typography typography;
    ShapeTokens shape;  SpacingTokens spacing;
    MotionTokens motion; Brightness brightness = Brightness::light;
};
```

**DesignSystem:**
```cpp
enum class ButtonVariant { filled, outlined, text, tonal };
struct ButtonConfig {
    WidgetRef label; std::function<void()> on_pressed;
    ButtonVariant variant = ButtonVariant::filled; bool enabled = true;
};
struct TextFieldConfig {
    std::string placeholder; std::function<void(std::string)> on_changed;
    bool obscure_text = false;
};
struct CardConfig { WidgetRef child; EdgeInsets padding=EdgeInsets::all(16.f); float elevation=1.f; };
struct NavigationBarConfig {
    struct Item { WidgetRef icon; std::string label; };
    std::vector<Item> items; int selected_index=0; std::function<void(int)> on_tap;
};

class DesignSystem {
public:
    virtual ~DesignSystem() = default;
    virtual const DesignTokens& tokens() const = 0;
    virtual WidgetRef buildButton(const ButtonConfig&)               const = 0;
    virtual WidgetRef buildTextField(const TextFieldConfig&)         const = 0;
    virtual WidgetRef buildCard(const CardConfig&)                   const = 0;
    virtual WidgetRef buildNavigationBar(const NavigationBarConfig&) const = 0;
};
```

**Theme:**
```cpp
class Theme : public InheritedWidget {
public:
    std::shared_ptr<const DesignSystem> data;
    static const DesignSystem& of(BuildContext& ctx) {
        return *ctx.dependOnInheritedWidgetOfExactType<Theme>()->data;
    }
    static const DesignTokens& tokensOf(BuildContext& ctx) { return of(ctx).tokens(); }
    bool updateShouldNotify(const InheritedWidget& old) const override {
        return static_cast<const Theme&>(old).data != data;
    }
};
```

**Widget adaptativo (patrón):**
```cpp
class Button : public StatelessWidget {
public:
    ButtonConfig config;
    WidgetRef build(BuildContext& ctx) const override {
        return Theme::of(ctx).buildButton(config);
    }
};
```

**Uso:**
```cpp
runApp(make_shared<Theme>(Theme{
    .data  = make_shared<CampelloDesignSystem>(myTokens),
    .child = make_shared<MyApp>(),
}));
```

---

## Backlog / Future

- Accessibility (semantic tree, screen reader support)
- Internationalisation (text direction, locale)
- [x] Rich text / inline spans
- [x] Dialog / overlay / modal system
- [x] Drag-and-drop (`Draggable` + `DragTarget`)

### IME (Input Method Editor) Platform Gaps

| Platform | Status | Notes |
|----------|--------|-------|
| macOS | ✅ Full | `NSTextInputClient` + candidate window positioning + `characterIndexForPoint:` |
| iOS | ✅ Full | `UITextInput` + software keyboard show/hide + `closestPositionToPoint:` |
| Windows | ✅ Full | `WM_IME_COMPOSITION` + `ImmSetCompositionWindow` candidate positioning |
| Android | ⚠️ Partial | Basic key events + soft keyboard show/hide via JNI. **Missing:** `InputConnection` for composed characters (accents, CJK, emoji). Soft keyboards expect `setComposingText` / `commitText` which requires a Java-side `InputConnection` implementation bridging to `TextEditingController`. |
| Linux | ✅ Full | IBus IME via D-Bus (`IbusIme` class) — works on both X11 and Wayland |

**Android IME — what would be needed to reach Flutter parity:**
1. Custom Java `Activity` extending `GameActivity` / `NativeActivity`
2. Override `onCreateInputConnection()` returning a custom `InputConnection`
3. `InputConnection` forwards `setComposingText`, `commitText`, `deleteSurroundingText` to native via JNI
4. JNI bridge calls `TextEditingController::{beginComposing,updateComposingText,commitComposing}`
5. Update `AndroidManifest.xml` + CMake/build system to compile Java sources
6. Estimated effort: 2–3 days

---

## Real GPU Rasterization for Visual Fidelity Tests

**Resolved in v0.1.2.** `campello_gpu` v0.4.1 implemented `copyTextureToBuffer()` (Metal backend), closing
the GPU→CPU readback pipeline. `GpuVisualRenderer` (`src/testing/gpu_visual_renderer.mm`) now provides a
headless Metal renderer that renders a `DrawList` to an offscreen RGBA8 texture and exports PNG.
`VisualRenderer` is kept as a CPU fallback for CI environments without a GPU.

Remaining work for full cross-platform coverage:
- [x] Metal backend readback — `GpuVisualRenderer` offscreen → PNG (macOS)
- [ ] Vulkan backend readback (`vkCmdCopyImageToBuffer`) for Android/Linux
- [ ] DirectX 12 backend readback (`CopyTextureRegion` into readback heap) for Windows
