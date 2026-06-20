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
- [x] **Gesture arena (Flutter-equivalent gesture arbitration)** — see dedicated section below

### Gesture Arena / Recognizer Arbitration (found 2026-06-18)

There is currently **no arbitration mechanism between competing gesture recognizers**
— no equivalent of Flutter's `GestureArenaManager`. Every recognizer (`RenderGestureDetector`,
`RenderDraggable`, `RenderTreeView`'s built-in pan-to-scroll, `RenderListView`/scroll
physics, etc.) registers independently with `PointerDispatcher` and receives the
*identical* stream of down/move/up events. Each one decides for itself — using its own
slop threshold — whether to "claim" the gesture, with nothing to make that claim
exclusive. The only real mutual-exclusion primitive in the system is
`PointerDispatcher::capturePointer()`, which is coarse (whole pointer, not gesture-type-aware)
and is currently only used by `RenderTextField`.

**Concrete bug this caused (fixed for this pair — see below)**: dragging a `Draggable<T>`-wrapped
row inside a `TreeView` never produced any visual feedback. `RenderTreeView::onPointerEvent`
flagged `panning_ = true` once a single move sample exceeded its 8px `kTapSlop`, and from
then on intercepted the gesture as a scroll. `RenderDraggable::onPointerEvent` did the same
independently with a 10px threshold to decide whether to fire `on_drag_start`. Neither knew
the other existed; TreeView's smaller threshold meant it usually won the race, so the drag
never visibly started. The same class of bug is still latent for any *other* pairing — a
custom pan recognizer and/or a scrollable container (`ListView`, `GridView`,
`SingleChildScrollView`) nested together, or a second `Draggable`/`GestureDetector` — until
those are also migrated (see checklist below).

A quick fix exists for the specific TreeView/Draggable case (asymmetric slop +
`RenderTreeView` checking `DragManager::active()->isDragging()` to yield once a drag has
claimed the gesture) but that's a one-off patch, not a real fix — it would need to be
re-applied ad hoc for every future pair of competing recognizers.

**Proper fix**: a real gesture arena, mirroring Flutter's model:
- [x] Design a `GestureArenaManager`-equivalent: recognizers `add()` themselves to an
      arena keyed by pointer ID at `down`, instead of acting unilaterally.
      (`inc/campello_widgets/ui/gesture_arena_manager.hpp` / `src/ui/gesture_arena_manager.cpp`;
      wired into `PointerDispatcher::handlePointerEvent()` via `close()`/`sweep()`.)
- [x] Recognizers signal `resolve(GestureDisposition::accepted | rejected)` instead of
      directly mutating shared state — the arena picks at most one winner per pointer
      and only the winner continues receiving events for that gesture.
- [x] Support "first recognizer to claim wins immediately" (e.g. a drag exceeding its
      slop) as well as "last recognizer standing wins" (Flutter's default when nobody
      explicitly accepts/rejects before the pointer is released).
- [x] Migrate `RenderDraggable` and `RenderTreeView` onto the arena (the pair that
      exposed the bug). `RenderDraggable`'s slop was also tuned to 6px (below
      `RenderTreeView`'s 8px `kTapSlop`) so the more specific gesture (dragging the row)
      wins ties within the same monotonic drag.
- [x] Migrate `RenderGestureDetector`, `RenderListView`/`RenderGridView`/`RenderPageView`/
      `RenderSingleChildScrollView`/`RenderSlider` pan/scroll/drag handling onto the arena
      instead of raw `PointerDispatcher` handlers. `RenderSlider` claims its arena
      immediately on `down` (no slop) so it reliably preempts an ancestor scrollable;
      `RenderGestureDetector` self-rejects past slop unless it actually has
      `on_pan_update`/`on_pan_end` wired up, so a plain tap/long-press "button" never
      blocks an ancestor scrollable from winning.
- [x] Add a test exercising the exact nested-scrollable-plus-draggable case that exposed
      this (`tests/universal/test_render_tree_view.cpp`:
      `DraggableRowWinsArenaOverEnclosingPan`, plus a sanity check that plain TreeView
      panning and a non-hit Draggable row are both unaffected).
- [x] Add nested-pan-plus-tap and nested-scroll-plus-scroll tests now that
      `RenderGestureDetector`/`RenderListView`/`RenderGridView`/`RenderPageView`/
      `RenderSingleChildScrollView`/`RenderSlider` are migrated
      (`test_render_gesture_detector.cpp`'s `NestedGestureFixture` cases,
      `test_render_list_view.cpp`'s `NestedPerpendicularListViewInnerWinsArenaTie`,
      `test_render_slider.cpp`'s `NestedInListDoesNotLetListScrollWhileDraggingThumb`).

**Side effects found while verifying this end-to-end** (manually driving a live
`Draggable` row in the `macos_tree_view` example) — all unrelated to the arena itself,
but were silently breaking `Draggable`+`Overlay` for anyone who used them; see
CHANGELOG `[Unreleased] → Fixed` for full detail:
- `DraggableState` left a dangling global `DragManager` pointer on dispose (heap
  use-after-free, confirmed with ASan).
- `Element::updateChild()` was missing Flutter's same-`WidgetRef`-instance short-circuit,
  so inserting one `OverlayEntry` cascaded a full rebuild through every sibling,
  including `TreeViewElement::update()`'s unconditional unmount-all-rows path —
  destroying a row mid-drag.
- `Stack` only recognized a `Positioned` that was its *direct* widget child, not one
  produced several `StatefulWidget` layers down (e.g. through `OverlayEntry` →
  `ValueListenableBuilder`).
- `RenderStack` tightly constrained any positioned child to fill the remaining space
  even when only `left`/`top` were given, instead of sizing it intrinsically.
- `Positioned` had no Flutter-style `ParentDataElement`, so a `Positioned` rebuilding
  deep inside an `Overlay` entry (e.g. following a dragged pointer) never told its
  ancestor `Stack` to re-layout after the first mount — added `PositionedElement`.

### Raster/Render Performance Investigation (2026-06-18)

While building the new two-lane performance overlay (`Renderer::paintPerformanceOverlay`,
see CHANGELOG), found that the **raster lane (GPU command encode + submit) reports
unexpectedly high costs**:

- `campello_widgets_hello` (a single button, trivial widget tree): **~3.5ms** raster time.
- `campello_editor`'s main window (multi-panel UI: hierarchy tree, inspector, many
  text fields/widgets): **~16.6ms** raster time — i.e. consuming essentially the
  entire 60fps frame budget on CPU-side encoding alone, before the GPU even executes.

For comparison, a Flutter app's raster-thread time for similarly simple content is
typically well under 1ms. The fact that `hello` (3.5ms) and the editor (~16.6ms)
differ proportionally to UI complexity rules out a vsync-locked measurement
artifact — the numbers scale with real work, so this is a genuine cost, not a
metric bug. Two separate things worth investigating:

1. **Fixed per-frame overhead is higher than expected** — even `hello`'s ~3.5ms for
   one button suggests something costs more per frame than it should (encoder
   setup, render pass begin/end, or per-draw-call state changes that aren't
   amortized).
2. **No apparent draw-call batching** — `campello_editor`'s heavier UI (many rects/
   text glyphs) scaling all the way up to the frame budget suggests each widget's
   draw commands are issued as separate GPU draw calls/state changes rather than
   batched (e.g. one quad per character, no glyph-atlas batching, no instancing
   for repeated rect/border draws).

**Findings (2026-06-20)** — instrumented `Renderer::renderFrame()`/`flushDrawList()`
with sub-phase timestamps, gated behind a new (zero-cost-when-off)
`DebugFlags::printRasterSubPhaseTimings`, and measured `hello` in Release:

- **Hypothesis 1 (fixed per-frame overhead) is ruled out.** `createCommandEncoder()`,
  `beginRenderPass()`, `endRenderPass()`, `finish()`, and `submit()` together cost
  **<0.3ms total** per frame. Essentially 100% of raster time (3.1–8ms measured) is
  spent inside `flushDrawList()` actually issuing draw commands.
- **Hypothesis 2 (no batching) is confirmed, and the cause is worse than "no batching"
  — there's no caching at all.** Per-draw-command-type breakdown for one `hello`
  frame: 62–68 `DrawRectCmd`s at ~0.02–0.05ms each (~1.3–3ms total) vs. only
  **13** `DrawTextCmd`s at **~0.13–0.32ms each** (~1.7–4.2ms total) — text draws are
  6–10× costlier per call than rects despite being a fifth of the draw-call count.
- **Root cause located**: `MetalDrawBackend::drawText()`
  (`src/macos/metal_draw_backend.mm:632-746`). For *every* `DrawTextCmd`, *every
  frame*, regardless of whether the text/style changed since the last frame, it:
  1. `CTFontCreateWithName(...)` — font lookup/creation from scratch.
  2. Builds an `NSAttributedString`, then `CTLineCreateWithAttributedString` +
     `CTLineGetTypographicBounds` to measure it.
  3. Allocates a fresh CPU pixel buffer + `CGBitmapContext`.
  4. `CTLineDraw(...)` — full CoreText glyph layout and CPU rasterization.
  5. **Allocates a brand-new GPU texture** (`device_->createTexture()`) sized to
     fit, and `texture->upload()`s the freshly rasterized pixels into it.
  6. Draws a textured quad with that one-off texture, which is then discarded.

  There is no glyph atlas and no per-string/style texture cache of any kind —
  static, unchanging text (e.g. a button label that never updates) re-runs this
  entire CPU rasterize + GPU texture allocate/upload pipeline on every single
  frame. This scales with *text widget count*, not with whether anything visible
  actually changed, which is exactly consistent with `campello_editor`'s far
  worse ~16.6ms (many more, mostly-static, text-bearing widgets: tree labels,
  inspector fields) and explains why even `hello`'s "trivial" UI (13 text draws)
  already costs several ms.

**Fix implemented (2026-06-20)**: `MetalDrawBackend` now caches the rasterized
texture per `TextSpan` (text content + font family/size/color/weight/italic —
already DPR-correct since `font_size` is pre-scaled to physical pixels before
reaching the backend). `lookupOrCreateTextTexture()` only runs the CoreText
rasterize + GPU texture allocate/upload path on a cache miss; a hit just
returns the existing entry. Eviction: `evictStaleTextTextures()` runs once per
frame (hooked into the existing `setViewport()` per-frame call) and drops any
entry not drawn in the last `kTextTextureMaxAgeFrames` (120) frames, so text
that's scrolled away/unmounted or changes every frame (e.g. a live counter)
doesn't grow the cache unboundedly — see `src/macos/metal_draw_backend.hpp`/`.mm`.

**Second, larger cause found while verifying the fix**: re-measuring after the
cache change alone showed **no improvement** — average per-text-draw cost was
unchanged even with ~7 of 9 distinct strings hitting the cache every frame
(confirmed via temporary hit/miss logging). The actual dominant cost turned out
to be in `MetalDrawBackend::drawTexturedQuad()` (used by every text **and**
image draw), independent of caching:
1. **Leftover debug `std::cerr` logging on every single call** — two
   unconditional prints (`"drawTexturedQuad: tex=..."` and `"Draw call
   submitted"`), executed on every textured-quad draw regardless of any debug
   flag. Removed.
2. `device_->createBindGroup()` and `device_->createBuffer()` are still
   allocated fresh on *every* draw call, cached texture or not — this is a
   separate, not-yet-addressed cost (see "further work" below).

Removing just the two stray `std::cerr` calls (independent of the text-texture
cache) is what produced the actual measured improvement.

**Result**: `hello` in Release, steady-state (post-warmup) average raster time
across 27 frames: **~3.85ms → ~2.15ms (-44%)**. First-frame (cold cache) cost
is unchanged since every string is necessarily a miss once. Full universal
test suite (389 tests) still passes; `VisualFidelity*`/`CanvasApiFidelity`
golden-image tests were skipped in this environment (missing Flutter golden
fixtures, not GPU-related) so pixel-level correctness of the cached-texture
path wasn't re-verified by automated tests — manually verified via stderr
trace (correct hit/miss pattern, no Metal/CoreText errors across 4 runs) since
screenshot capture isn't available in this sandboxed dev environment (no
attached display compositor).

**`campello_editor` re-measured (2026-06-20, user-reported)**: moving an empty
scene now shows **UI: 2.5ms / RASTER: 5.7ms** — down from the original
~16.6ms raster baseline (**-66%**), a larger drop than `hello`'s -44% as
predicted (the editor has far more static, mostly-unchanging text: tree
labels, inspector fields). Confirms the text-texture cache + debug-print
removal were the right first fix. RASTER is still ~2× the 16ms/frame budget on
its own (UI 2.5ms + RASTER 5.7ms ≈ 8.2ms total, within budget at 60fps, but
RASTER alone would blow the budget if UI cost grew), so there's still room —
see next item.

**`campello_editor` re-measured again after the BindGroup/Buffer fix below
(2026-06-20, user-reported)**: empty scene now shows **UI: 2.6ms / RASTER:
1.7ms** — another **-70%** on top of the previous 5.7ms, **-90% from the
original ~16.6ms baseline**. Again a larger drop than `hello`'s -58% for the
same fix, consistent with the editor having far more total draw calls per
frame to amortize the buffer-pool/bind-group-cache savings across. UI (2.6ms)
+ RASTER (1.7ms) ≈ 4.3ms total — comfortable headroom under the 16ms/60fps
budget even on the editor's heavier UI.

**Fix implemented (2026-06-20)**: addressed the per-draw `createBindGroup`/
`createBuffer` cost identified above.
- **BindGroup caching for text**: `TextTextureCacheEntry` now also stores a
  `BindGroup` built once alongside the texture (same cache, same eviction —
  see `src/macos/metal_draw_backend.hpp`/`.mm`). `drawTexturedQuad()` takes
  an optional `cached_bind_group` parameter; `drawText()` passes the cached
  one through, skipping `Device::createBindGroup()` entirely on a cache hit.
  `drawImage()`/`drawBackdropFilter()` still build a fresh bind group each
  call (their source textures aren't in this cache) — not addressed here.
- **Pooled uniform buffers**: added `UniformBufferPool` (4-generation ring of
  reusable `Buffer`s; each draw still gets fresh contents via `upload()`, but
  the underlying GPU buffer objects are recycled across frames instead of
  calling `Device::createBuffer()` — a real GPU allocation — on every single
  draw). Wired into all four uniform-buffer call sites: `drawFilledRect`,
  `drawShape`, `drawLine`, `drawTexturedQuad` (`BlurUniforms`/
  `ShaderMaskUniforms` in the offscreen-compositing path were left
  untouched — lower frequency, out of scope for this pass).

**Result**: `hello` in Release, steady-state average raster time:
**~2.15ms → ~0.89ms (-58%)**, on top of the earlier -44% — **~3.85ms → ~0.89ms
overall (-77%)** from the original baseline. Per-draw averages now: rect
~0.004–0.05ms (was ~0.02–0.05ms), text ~0.002–0.05ms (was ~0.13–0.32ms before
any fix). This is now in Flutter's "well under 1ms for simple content"
ballpark even with 100+ draw calls (the performance overlay's own scrolling
history bars). Full test suite (389 tests) still passes; same visual-
verification caveat as above (no screenshot capture available in this
sandboxed environment; verified via clean stderr traces across multiple runs
instead).

**Further work (not started)**:
- `drawImage()`/`drawBackdropFilter()` still rebuild their `BindGroup` every
  call — lower priority since images are typically far less numerous than
  text/rects in this codebase's UIs so far, but worth revisiting if an
  image-heavy screen shows up as a bottleneck.
- Get real screenshot/visual verification working in this dev environment
  (or run the Flutter golden generation step) so future raster changes here
  can be confirmed pixel-correct, not just by absence of errors.

**UI-side counterpart found (2026-06-20, user-reported)**: same bug, different
side of the frame. `RenderBox::setChild()` (`src/ui/render_box.cpp`) had two
unconditional `std::cerr` lines on every call — and `setChild()` is invoked
from `SingleChildRenderObjectElement::performBuild()` → `syncChildRenderObject()`
for **every single rebuild** of *any* single-child wrapper widget (`Padding`,
`Center`, `Container`, `ConstrainedBox`, `Align`, `ClipRect`, `Opacity`,
`DecoratedBox`, …) — not just at mount. Since that's nearly every widget in a
typical tree, this fired continuously during any interaction that triggers
rebuilds (e.g. dragging/moving an object in the editor). Removed both lines.

While auditing for the same pattern, also found and fixed
`ImageWidgetState::build()` (`src/widgets/image_widget.cpp`) logging twice on
every rebuild in its steady-state (`completed`) case — same bug, hits any
`Image` widget in a continuously-rebuilding subtree (icons/thumbnails). Left
the *other* `std::cerr` calls in `image_loader.cpp`/`image_provider.cpp`
(async load lifecycle, fires once per actual load, not per rebuild) and
`run_app.mm`'s mouseDown/mouseUp logging (once per click, not per frame) — not
hot-path, lower priority, can be cleaned up later as general hygiene rather
than a performance fix.

Full test suite (389 tests) still passes. Awaiting re-measurement in
`campello_editor` to quantify the UI-lane improvement.

### RepaintBoundary — paint-level repaint scoping (2026-06-20)

Following up on the `RenderBox::setChild()` fix above, re-measured
`campello_editor`'s UI time while orbiting the camera over an empty scene:
**UI: 2.0ms**. Added temporary timing instrumentation directly in
`scene_editor_tab.cpp` (around `renderScene()` and the rest of
`buildViewportPanel()`'s `LayoutBuilder` callback, plus `buildHierarchyPanel()`/
`buildInspectorPanel()` in `build()`) — sum of all of it: **~0.3-0.4ms**,
leaving **~1.6ms unaccounted for**, not attributable to anything specific to
`SceneEditorTab`.

**Root cause traced into `campello_widgets` itself**: `RenderObject::layout()`
(`src/ui/render_object.cpp`) correctly skips `performLayout()` when not dirty
and constraints are unchanged — but `RenderObject::paint()`, despite its own
doc comment claiming the same ("Calls `performPaint()` if the object is
paint-dirty"), called `performPaint()` **unconditionally**, every time. Since
`markNeedsPaint()` propagates upward to the root, and `root_->paint()` walks
the *entire* tree once dirty, **every widget in the app — menu bar, dock
panels, hierarchy, inspector, status bar — re-walked and re-emitted draw
commands every single frame**, for as long as anything anywhere was dirty
(e.g. for the whole duration of a camera drag), regardless of whether that
widget's own content changed.

**Fix — added `RepaintBoundary`** (mirrors Flutter's), the first
paint-level caching primitive in the framework:
- `RenderObject::paint()` is now `virtual` (`inc/campello_widgets/ui/render_object.hpp`).
- `Canvas::appendRecorded(const DrawList&)` (`canvas.hpp`) — bulk-appends a
  pre-recorded `DrawList` into the live recording, used to "replay" a cached
  subtree without re-walking it.
- `RenderRepaintBoundary` (`inc/campello_widgets/ui/render_repaint_boundary.hpp` /
  `src/ui/render_repaint_boundary.cpp`) overrides `paint()`: if clean and a
  cache exists, replays the cached `DrawList` (recorded in local coordinates)
  via `save()`/`translate()`/`appendRecorded()`/`restore()` — **never calls
  `performPaint()`, never walks the child subtree**. If dirty, records the
  child's output into a standalone headless `PaintContext` first, caches it,
  then replays via the same path. No new invalidation plumbing needed —
  `markNeedsLayout()`/`markNeedsPaint()`'s existing upward propagation already
  correctly dirties a boundary whenever anything inside it changes.
- `RepaintBoundary` widget (`inc/campello_widgets/widgets/repaint_boundary.hpp` /
  `src/widgets/repaint_boundary.cpp`) — mirrors `ClipRect`'s widget/render-object
  pairing exactly, no clipping (matches Flutter: a pure compositing/caching
  boundary, not a clip).
- New tests: `tests/universal/test_render_repaint_boundary.cpp` (4 tests) —
  verify cache-hit skips `performPaint()` entirely, `markNeedsPaint()` and a
  constraints change both correctly force re-recording. Full suite: 393/393
  passing (was 389; +4 new).

**Important caveat found while applying this to the editor's viewport**:
this fix alone will likely **not** move `campello_editor`'s specific ~1.6ms,
because the cause there is one level up from paint. Checked
`Flex::updateRenderObject()` (Row/Column) and `ColoredBox::updateRenderObject()`
— both call `markNeedsLayout()`/`markNeedsPaint()` **unconditionally on every
reconciliation pass**, regardless of whether the new value actually differs
from the old one. This is a consistent framework-wide pattern, not a one-off.
Since `SceneEditorTabState::build()` reconstructs *all three* panels
(hierarchy/viewport/inspector) with fresh `WidgetRef` instances on every
camera-drag frame (via the `setState()` override documented in
`scene_editor_tab.h`), every render object in all three panels gets
re-dirtied by its own reconciliation, every frame — independent of any
`RepaintBoundary`. The boundary helps the *general* Flutter case (an
unrelated sibling whose Element was never revisited during `buildScope()`),
not this specific "whole tab rebuilds on every camera tick" architecture.

Applied `RepaintBoundary` around the viewport's content in
`buildViewportPanel()` anyway (correct, low-risk, real infrastructure other
screens will benefit from) — built cleanly into `campello_editor` (had to
force a `cmake` reconfigure of the editor's own build for the new
campello_widgets sources to be picked up, since `file(GLOB_RECURSE ...)`
without `CONFIGURE_DEPENDS` doesn't auto-detect new files).

**Real fix for the editor's specific gap — implemented (2026-06-20)**: scoped
the *rebuild*, not just the paint, entirely within `campello_editor` (no
further `campello_widgets` changes needed). Extracted `SceneEditorTab`'s
viewport into its own `ViewportPanel`/`ViewportPanelState`
(`inc/editor/tabs/viewport_panel.h` / `src/editor/tabs/viewport_panel.cpp`),
owning its own `setState()`. Camera orbit/pan/zoom now only marks that nested
Element dirty; `SceneEditorTabState`'s Element is never revisited during a
pure camera-drag frame, so hierarchy/inspector's render objects' `needs_paint_`
genuinely stays false — which is what makes wrapping them in `RepaintBoundary`
(in `SceneEditorTabState::build()`, moved off the viewport since it no longer
needs it there) actually skip their repaint.

Two cases still need to reach the parent explicitly, since they mutate scene
data the (sibling, not ancestor) inspector displays — `ViewportPanel` gained
an `onSceneMutated` callback prop for these: gizmo-axis dragging (mutates the
selected entity's `TransformComponent` directly) and GLTF drag-drop import
(adds entities). Plain camera movement doesn't touch it.

Verified: full `campello_widgets` test suite unaffected (394/394, this was a
`campello_editor`-only change). Awaiting user re-measurement of UI time during
camera orbit, and functional verification that gizmo-drag still live-updates
the inspector and GLTF drop still refreshes the hierarchy.

**Visual regression found and fixed (2026-06-20, user-reported)**: as
predicted, no measurable UI-time change in the editor — but the user also
reported the scene viewport rendering *wrong-clipped*, with the floor grid
visible outside the viewport widget's bounds, "like the clipping area has
some x offset." Root cause: `RenderRepaintBoundary`'s first implementation
recorded the child's paint into a **separate, origin-relative headless
`PaintContext`** (offset `{0,0}`), then replayed the cached `DrawList` by
wrapping it in `canvas.translate(offset)` on the live context. That's correct
for ordinary draw-command geometry, which is *transform-deferred* — each
command's coordinates are multiplied by the ambient transform at flush time
(`Renderer::flushDrawList()`), so a `PushTransformCmd{translate(offset)}`
wrapper correctly relocates them. **Clip rects are not transform-deferred**:
`Canvas::clipRect()` bakes an *absolute* rect at record time, and
`flushDrawList()`'s `PushClipRectCmd` handling is a direct assignment
(`current_clip = c.rect;`) with no transform applied at flush time. So the
viewport's `ClipRect` (around its content, to keep projected grid/gizmo lines
that land outside the viewport's pixel rect from bleeding into other panels)
got recorded as clipping to `(0,0)-(w,h)` — the fabricated local origin — and
the wrapping translate never moved it, leaving the clip stuck at the
top-left of the screen while the (correctly-translated) draw commands
rendered at the real position.

**Fix**: record the cache by painting the child into the *live* context at
its real on-screen offset (no separate headless context), and remember which
slice of the resulting `DrawList` that produced; replay is then a direct
`appendRecorded()` with no translate wrapper, since the cached commands
already have everything — including clip rects — baked in correctly for that
exact offset. Documented the resulting constraint clearly in
`RenderRepaintBoundary`'s class doc: a clean replay assumes the boundary's
on-screen offset hasn't changed since it was recorded (`positionChild()`,
used by `Stack`/`Positioned`, deliberately doesn't mark needs-paint on a pure
reposition — don't wrap something repositioned without also dirtying it).
Added a dedicated regression test,
`RenderRepaintBoundary.CleanReplayKeepsClipRectAtCorrectAbsolutePosition`
(wraps a `RenderClipRect` — the same shape as the production usage — and
asserts the recorded/replayed `PushClipRectCmd` lands at the real offset, not
a fabricated origin); confirmed it fails against the old implementation.
Suite: 394/394 passing (+1 from this test). Editor rebuilt cleanly; awaiting
visual confirmation the clipping is now correct.

### Evaluation: Splitting UI and Raster into Two Real Threads (Flutter-style) (2026-06-19)

Today everything — widget rebuild, layout, paint-command recording, **and** GPU
command encoding/submission — runs sequentially on a single thread (the one
`ThreadChecker` binds to at startup, which is also the platform's main/event
thread). The "Build" and "Raster" phases in `Renderer::renderFrame()`
(`src/ui/renderer.cpp:66-207`) are only separated for *measurement* purposes
(`build_sampler_` / `raster_sampler_`) — they are not on different threads.
This section captures a full evaluation of what a genuine two-thread split
(UI thread + raster thread, as in Flutter) would require, so the analysis
isn't lost before someone picks this up.

**What already works in our favor:**
- `DrawList` (`inc/campello_widgets/ui/draw_command.hpp:271`) is already a
  `std::vector<DrawCommand>` of value types (plus one `shared_ptr<Texture>`
  for images), fully decoupled from the live `RenderObject` tree. This is the
  functional equivalent of Flutter's Layer Tree / Scene — the hardest
  precondition for a UI/raster split already exists.

**What would need to change:**
1. **No pipeline exists yet.** Build and raster run sequentially in the same
   call stack today. To get the actual Flutter benefit (UI thread starts
   building frame N+1 while the raster thread is still rasterizing frame N),
   we'd need a queue/pipeline with double-buffered `DrawList` + per-frame
   metadata (target `TextureView`, viewport dims, DPR, pending drawable,
   backdrop-filter flags). Without this, moving raster to another thread only
   relocates the work — it doesn't parallelize it.
2. **Shared global atomics are a real race, not a hypothetical one.**
   `RenderObject::setActiveBackend` / `setActiveDevicePixelRatio`
   (`inc/campello_widgets/ui/render_object.hpp:158-187`) are single global
   atomics (`memory_order_relaxed`) representing "the current frame's"
   backend/DPR. If the UI thread starts frame N+1's `layoutPass()` (which
   rewrites these) while the raster thread is still in `flushDrawList()` for
   frame N (which reads them via `draw_backend_`), that's a textbook data
   race. These would need to become per-frame data carried with the
   `DrawList`, not global mutable state.
3. **`ThreadChecker` assumes exactly one UI thread.** `assertOnBoundThread()`
   is called from `Renderer::renderFrame`, `RenderObject::markNeedsLayout` /
   `markNeedsPaint`, `PointerDispatcher`, `FocusManager`, `TickerScheduler`,
   and `AnimationController` — all assuming the same thread. A second bound
   thread (raster) would need its own checker, and `Renderer`'s per-frame
   member state (`backdrop_tex_`, `has_backdrop_filter_`, `frame_encoder_`,
   `frame_target_`, `pending_drawable_`) would need to move from shared
   members to an immutable frame package handed off through the queue.
4. **`FrameScheduler` is explicitly main-thread-only.** Its own doc comment
   (`inc/campello_widgets/ui/frame_scheduler.hpp:29-32`) says
   `setCallback()`/`scheduleFrame()` are not synchronized and must be called
   from the UI event-loop thread. This is consistent with a UI/raster split
   (only the UI thread would schedule frames) but confirms that today "UI
   thread" == the platform thread (AppKit run loop, etc.), unlike Flutter
   where platform and UI are already separate threads.
5. **Platform run loops currently drive raster on the main thread by
   construction.** On macOS (`src/macos/run_app.mm:659-690`),
   `drawInMTKView:` fires via `[MTKView setNeedsDisplay:YES]` inside AppKit's
   run loop — raster already executes on the main thread because that's how
   `MTKView`'s delegate model works. Moving raster to its own thread means
   bypassing that automatic delegate and manually pulling the drawable /
   `getSwapchainTextureView()` and handing frame data to a raster thread —
   doable, but it has to be redone **once per platform** (macOS, iOS,
   Android, Windows, Linux). iOS/Windows/Linux platform integration is
   already only partial per the phase table above, so this adds maintenance
   surface in the least mature part of the project.
6. **`campello_gpu` thread-safety for `Device` is unverified.** No doc
   comment in `device.hpp` states whether `createCommandEncoder()` /
   `submit()` are safe to call from a thread other than the one that created
   the `Device`. Metal/D3D12 generally allow this; Vulkan command-queue
   submission requires external synchronization. This needs to be confirmed
   per backend (`metal_draw_backend.mm`, `vulkan_draw_backend.cpp`,
   `d3d_draw_backend.cpp`) before relying on it, not assumed.

**Why this likely isn't the right next step:** the raster-cost numbers
recorded above (~3.5ms for `hello`, ~16.6ms for `campello_editor`) are a cost
in the GPU encode/submit work itself — likely missing draw-call batching —
not a symptom of build and raster contending for the same thread. Splitting
threads does **not** reduce that absolute cost; it only stops a slow raster
phase from blocking the *next* frame's input handling/build. If raster stays
at ~16.6ms, a 60fps app still drops frames whether or not raster has its own
thread — the thread split improves input responsiveness/perceived smoothness
under load, but doesn't fix the root cause already identified above.

**Recommendation:** investigate and fix the raster-cost root cause (batching,
per-draw-call overhead — see "Suggested next steps" above) first. Revisit a
UI/raster thread split afterward, once raster is cheaper and there's less
margin for the synchronization issues described in points 1–6 to matter.

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
