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

### iOS hardening + Android/Linux Vulkan backend unification (2026-07-24)

iOS moved from "builds, mostly untested" to real device + simulator bring-up:
- `src/ios/run_app.mm` — hardware-keyboard support (`UIKeyboardHIDUsage`→`KeyCode`
  mapping, `pressesBegan:/pressesEnded:/pressesCancelled:`), on-screen
  keyboard/predictive-bar suppression via `-inputView`/`-inputAccessoryView`/
  `-conformsToProtocol:` overrides, switched from on-demand `MTKView` redraw to
  continuous `CADisplayLink`-paced rendering (on-demand was found to starve
  `hitTest:`/touch delivery once any continuous `AnimationController` is
  running — reproduced on both Simulator and device), physical-pixel (not
  logical-point) viewport dimensions, and safe-area-inset-aware touch
  coordinates for notched devices.
- `build_metal_shaders.sh` now compiles three Metal shader variants
  (macOS / iOS device / iOS simulator — Metal bytecode is target-triple
  specific) via `xcrun -sdk {macosx,iphoneos,iphonesimulator}`, all three
  embedded into `src/shaders/metal_widgets.h`; picked at compile time via
  `TargetConditionals.h`'s `TARGET_OS_SIMULATOR`/`TARGET_OS_IPHONE`.
- `examples/gallery/ios/run.sh` (new) builds+installs+launches on a
  connected physical device via `xcrun devicectl` if attached, else
  Simulator via `xcrun simctl`. `CMakeLists.txt` gained a `POST_BUILD` step
  that copies + re-signs `campello_gpu`/`campello_image` dylibs into the
  app bundle's `Frameworks/` — required for on-device (sandboxed) launch,
  not needed on Simulator.

Android and Linux's Vulkan backends unified into one
`src/gpu/vulkan/vulkan_draw_backend.{hpp,cpp}` (Android's separate 463-line
copy deleted), with per-platform text rasterization injected via a new
`ITextRasterizer` interface (`src/gpu/vulkan/text_rasterizer.hpp`,
implemented once each in `src/android/android_text_rasterizer.cpp` /
`src/linux/linux_text_rasterizer.cpp`). Real perf/correctness work landed
as part of the merge:
- Push constants (`vkCmdPushConstants`) replace per-draw uniform
  buffers/bind groups for rect/rrect/circle/oval — was the dominant
  Vulkan-vs-Metal raster gap (~57 rect draws/frame each allocating a
  descriptor set). Directly relevant to the Vulkan-performance backlog
  item above.
- Triple-buffered per-frame GPU resource retention (`frame_*_` /
  `prev_frame_*_` / `prev2_frame_*_`), fixing real `VUID-vkDestroySampler-*`
  / `VUID-vkDestroyPipeline-*` validation crashes / black screens on
  Android caused by destroying resources still in flight under
  `campello_gpu`'s non-blocking 2-deep frame pipelining.
- Android now gets `drawCircle`/`drawOval`/`blurTexture`/`drawBackdropFilter`
  (previously no-ops), swapchain pixel format queried from the device
  instead of hardcoded `bgra8unorm` (some devices only expose RGBA8, which
  corrupted clip-shape/shader-mask composites), and a dedicated
  `RasterThread` (`src/android/run_app.cpp`) so GPU-submit-blocking work no
  longer stalls the same thread pumping `android_native_app_glue`'s input
  queue — a stalled input thread previously froze touch handling entirely.
  Also fixed: touch coordinates never divided by DPR (broke hit-testing on
  non-1x devices), and the first frame never being scheduled on launch.

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

## Phase 12b — Canvas API GPU Backend Implementation

The public Canvas API records `DrawCommand`s, but several commands are either
not dispatched by `Renderer::flushDrawList()` or not implemented by every
`IDrawBackend`. Track per-backend status here.

### Shared infrastructure
- [x] Add `IDrawBackend::drawArc`
- [x] Add `IDrawBackend::drawPath`
- [x] Add `IDrawBackend::drawPoints`
- [x] Add `IDrawBackend::saveLayerComposite` and `SaveLayerEndCmd`
- [x] Dispatch `DrawArcCmd`, `DrawPointsCmd`, `DrawPathCmd` in `Renderer::flushDrawList()`
- [x] Dispatch `SaveLayerCmd` / `SaveLayerEndCmd` in `Renderer::flushDrawList()`
- [x] Extract shared path flattening/tessellation helpers into `src/gpu/path_tessellation.hpp/.cpp` (used by Vulkan, Metal, DirectX)

### Metal
- [x] `drawArc` — tessellated to triangles via `rect_pipeline_`
- [x] `drawPath` — shared CPU flatten + ear-clip fill / quad stroke
- [x] `drawPoints` — decomposed to circles/lines
- [x] `saveLayer` — offscreen texture + opacity compositing
- [x] `drawShaderMaskComposite`
- [ ] `drawDRRect` (currently white-background hack)

### Vulkan
- [x] `drawLine` — line pipeline + shaders
- [x] `drawArc` — tessellated to triangles
- [x] `drawPath` — shared CPU flatten + ear-clip fill / quad stroke
- [x] `drawPoints` — decomposed to circles/lines
- [x] `drawShaderMaskComposite` — gradient LUT compositing
- [x] `saveLayer` — offscreen texture + opacity compositing
- [ ] `drawDRRect` (currently white-background hack)

### DirectX 12
- [x] `drawArc` — tessellated to triangles via `rect_pipeline_`
- [x] `drawPath` — shared CPU flatten + ear-clip fill / quad stroke
- [x] `drawPoints` — decomposed to circles/lines
- [x] `saveLayer` — offscreen texture + opacity compositing
- [x] `drawShaderMaskComposite` — gradient LUT compositing (new `shader_mask.hlsl`; run `build_dx12_shaders.bat` on Windows to regenerate embedded DXBC)
- [ ] `drawDRRect` (currently white-background hack)

### GPU-backed visual unit tests
- [x] Refactor `GpuVisualRenderer` into backend-agnostic core (`tests/gpu_visual_renderer.cpp`) + platform-specific factory files (Metal, Vulkan, DirectX, stub)
- [x] Vulkan `GpuVisualRenderer` backend for headless offscreen rendering on Linux/Android
- [x] DirectX `GpuVisualRenderer` factory for Windows (HLSL placeholders must be regenerated before ShaderMask tests run)
- [x] Shared DrawList replay loop handles `drawArc`/`drawPath`/`drawPoints` and offscreen composites (`saveLayer`, `clipRRect`/`clipOval`/`clipPath`, `shaderMask`)
- [x] Focused Canvas API visual tests: `VisualFidelityCanvasApi.Arcs`, `.Paths`, `.Points`, `.Clipping`, `.PaintStyles`, `.SaveLayer`, `.DrawColor`, `.Skew`, `.PathArc`
- [x] Matching Flutter `CustomPainter` golden generators in `flutter_fidelity_tester/test/visual_goldens_test.dart`
- [x] Generate Flutter PNG goldens (`tests/visual_fidelity/flutter_goldens/`)
- [x] Fix `Canvas::skew` matrix to match Flutter/Skia (`x' = x + sx*y, y' = y + sy*x`); was using `tan()` and swapped elements

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
- [x] **Performance overlay: add a real FPS counter** (found 2026-07-24,
      done 2026-07-25) — new `Renderer::present_fps_sampler_`/
      `recordPresentSample()` measures the wall-clock cadence between
      successive `rasterFrame()` completions (not per-phase cost), reset
      across any idle-gap wider than 200ms so resuming from an on-demand
      renderer's idle period doesn't register a bogus low reading. Shown as
      `FPS: …` in the overlay label. This is what surfaced the item below —
      the gallery's Images tab measured a real ~45fps despite both UI and
      raster *cost* looking fine, which the old overlay had no way to show.
- [x] **Performance overlay budget line should track actual display Hz,
      not hardcoded 60** (found 2026-07-25 during multi-monitor testing,
      done same day) — `Renderer::setDisplayRefreshHz()`/`displayRefreshHz()`
      feed the budget line's `kTargetMs`; wired up on macOS via
      `windowDidChangeScreen:` (`src/macos/run_app.mm`). **Follow-up**:
      iOS (ProMotion 120Hz via `UIScreen.maximumFramesPerSecond`),
      Windows, and Linux (per-monitor Hz via each platform's own display
      API) still just use the 60Hz default — only macOS is wired up so
      far since that's the multi-monitor setup this was found on.
- [x] **Vulkan raster performance** (found 2026-07-24, partially fixed
      2026-07-25) — investigated via the new FPS counter above: the
      gallery's Images tab was stuck at ~45fps on macOS/Metal too (not
      Vulkan-specific as originally suspected), root-caused to a
      dirty-region over-reporting bug affecting all backends alike — see
      CHANGELOG `[Unreleased] → Fixed`, "Gallery Images tab held at ~45fps
      instead of 60 on macOS". Fixed there; macOS confirmed at a stable
      60fps for that tab. Confirmed via `CW_TRACE_DIRTY=1` (readable on
      Android via `adb shell setprop debug.cw_trace_dirty 1` before
      relaunching — env vars set before `am start` aren't inherited by the
      app process) that this shared fix also lands `needs_capture=0` on a
      real Android/Vulkan device (Galaxy Tab S7 FE, Snapdragon 750G/Adreno
      619). **Still open, and the real remaining gap**: a *separate*,
      apparently content-independent ~28-33ms floor inside Vulkan
      `Device::submit()`, present even on a trivial 3-rect animation with
      no shadows/clips/backdrop-filter — identical magnitude to the heavy
      Images tab. Ruled out so far, each tested directly on-device:
        - Vulkan validation layers (confirmed `CAMPELLO_GPU_VALIDATION=OFF`)
        - Samsung GOS/Game Booster (disabled the 3 packages via
          `pm disable-user`, no change, re-enabled afterward)
        - GPU debug overlays / dev options (none active)
        - Battery saver / CPU governor (`schedutil`, 100% battery, no throttle)
        - Display refresh rate (genuinely locked 60.00Hz, confirmed via
          `dumpsys SurfaceFlinger`, not adaptively downshifted)
        - Debug vs. Release native build (Release halved CPU-encode cost —
          main-flush 15ms→8.4ms — but barely touched the ~28-33ms `submit()`
          floor: 30.8ms→27.98ms)
        - Fine-grained timing *inside* `campello_gpu`'s Vulkan
          `Device::submit()` (temporary local instrumentation, reverted):
          `vkResetFences`/`vkQueueSubmit`/`vkQueuePresentKHR`/the
          `genCommandBuffer` ring-slot replace are all consistently
          sub-millisecond.
        - Raster-thread priority elevation (`setpriority(PRIO_PROCESS,
          gettid(), -8)`, matching SurfaceFlinger's RenderThread nice
          value — confirmed applied via `adb shell ps -T`) — no
          measurable effect; reverted (not committed) since unproven.

      **Root-caused 2026-07-25 via a real Perfetto trace** (`adb shell
      perfetto`, config pushed as `-c -`/output as `-o -` to dodge
      `/data/local/tmp` permission issues; analyzed locally with the
      Python `perfetto` package's `TraceProcessor` SQL interface —
      `pip install perfetto`, no Perfetto UI needed). The plain-`fprintf`
      CPU timing above was misleading in isolation: the trace shows
      neither thread is CPU-bound or wrongly blocked.
        - SurfaceFlinger's own `vsyncCallback` ticks (ground truth from
          the compositor) land ~16.5-17ms apart — genuine 60Hz; the
          display/compositor itself is not the bottleneck.
        - Our own `AChoreographer_frameCallback` (UI thread, `Thread-3`)
          only arrives every ~47.7ms on average (≈21Hz) — roughly every
          3rd real vsync. `Thread-3` spends 91% of the trace asleep
          (`thread_state` = `S`), not busy; `TickerScheduler::tick()`
          (`src/ui/ticker.cpp`) correctly calls
          `FrameScheduler::scheduleFrame()` every tick while any
          `AnimationController` is active, so the re-arm logic isn't the
          bug either.
        - The raster thread (`Thread-4`) does real work fast — main-flush
          (draw-list encode) ~10-20ms, then `QueueSubmit`+`QueuePresentKHR`
          together under 1ms (per Android's own `libvulkan` loader
          atrace slices, not just our instrumentation) — then legitimately
          sleeps ~10ms waiting for the next `FramePackage`.
        - Conclusion: `vkQueueSubmit`/`vkQueuePresentKHR` returning fast
          only proves the CPU handed work to the GPU quickly, not that the
          GPU *finished* it — Vulkan submission is async by design.
          Android's compositor throttles vsync-callback delivery to an
          app based on how backed-up its buffer queue is; a ~3x throttle
          with every CPU-side step measured as fast points at genuine GPU
          hardware execution time (Adreno 619) exceeding one vsync period
          for this scene — invisible to any CPU-side timer, ours or
          Perfetto's ftrace, since neither observes actual GPU execution.
      **Actually root-caused and fixed 2026-07-25**, same day, via GPU
      timestamp queries: added temporary instrumentation calling
      `CommandBuffer::getGPUExecutionTime()` — a real `campello_gpu`
      feature (`VkQueryPool` timestamps, all 4 backends) that already
      existed in the pinned `v0.20.0` tag but had never actually been
      called from `campello_widgets`. Real GPU execution time measured a
      steady ~10ms/frame — comfortably within budget, ruling out "slow
      GPU" as the cause after all. Cross-referencing that against the
      Perfetto trace's `queueBuffer`→`AcquireNextImageKHR` frame period
      (which matched the observed ~40-50ms, not the 10ms GPU time) pointed
      at something in between, and a full unfiltered per-frame timeline
      dump found it: `CreateSwapchainKHR` firing on **literally every
      frame** (155/155 `AcquireNextImageKHR` calls in one capture), each
      costing 5-16ms plus cascading buffer teardown/reallocation via the
      platform's Gralloc HAL (3 fresh `dequeueBuffer`→`allocateHelper`
      calls per recreation). Root cause: `Device::submit()`
      (`campello_gpu` `src/vulkan/device.cpp`) treated the advisory
      `VK_SUBOPTIMAL_KHR` the same as the mandatory
      `VK_ERROR_OUT_OF_DATE_KHR`, and on this physically-rotated tablet
      (`currentTransform = ROTATE_90`) every present came back SUBOPTIMAL
      forever, because this renderer deliberately requests
      `preTransform = IDENTITY` (never pre-rotating content) whenever the
      surface supports it, regardless of its actual current transform —
      confirmed directly via a one-off diagnostic print
      (`present result=SUBOPTIMAL -> recreating swapchain`,
      `currentTransform=2 supportedTransforms=511`). Fixed in
      `campello_gpu` (only recreate on `VK_ERROR_OUT_OF_DATE_KHR` — see
      its own `CHANGELOG.md`) — confirmed `CreateSwapchainKHR` count drops
      to 0 and gallery Images tab FPS: **~21fps → ~34-43fps**, more than
      double, now bottlenecked by legitimate CPU/GPU work instead.

      **Further improved same day**, chasing a *separate* regression
      report ("buffer improvements, but no improvement seen" — UI 2ms,
      raster 22ms, 35fps, matching the post-swapchain-fix baseline almost
      exactly): `campello_gpu`'s new device-local-memory-preferring
      `Device::createBuffer()` (see its own `CHANGELOG.md` `[Added]`) was
      actually a small *net regression* on this Android/Adreno UMA
      device — device-local memory there is already host-visible, so the
      extra `findMemoryTypeIndex()` scanning was pure overhead with zero
      benefit, ~8-13% slower per draw call across every category. Cached
      `VkPhysicalDeviceMemoryProperties` once per `Device` instead of
      re-querying per `createBuffer()` call (`campello_gpu`), recovering
      about half the regression — but the *real* fix was noticing Metal's
      `UniformBufferPool` already avoids this whole problem by pooling
      vertex buffers across frames instead of allocating fresh ones per
      draw, and Vulkan's `vulkan_draw_backend.cpp` simply never had the
      same pattern. Ported it directly (`VulkanDrawBackend::
      UniformBufferPool`, see this repo's own `CHANGELOG.md`
      `[Unreleased] → Changed`): main-flush 14.50ms → 9.93ms (-31%),
      FPS 42.9 → 44.9 — beating the pre-regression baseline outright,
      since pooling also eliminates the *original* per-draw allocation
      cost that predated this specific regression.

      **Resolved 2026-07-30**, same investigation continued: `shadow`
      draws turned out to be the real next lever, exactly as flagged
      above. `getGPUExecutionTime()` (bracketing the whole command
      buffer, sampled every 10th frame) showed a rock-solid ~10-12ms of
      *real GPU hardware time* per frame regardless of scene — the same
      on a near-blank Draw-tab canvas as on the much busier Images tab —
      pointing at something fixed and shared rather than proportional to
      visual complexity. Confirmed via A/B test (temporarily bypassing
      `blurTexture()` entirely): the box-shadow Gaussian blur alone
      accounted for roughly half of it. `blur.frag` is a naive per-tap
      loop (up to 25 texture fetches/pixel/pass, two passes); since a
      shadow's blurred output is inherently low-frequency, rendering it
      at half resolution and upscaling via the existing linear-filtered
      composite is visually indistinguishable while cutting the pixel
      count 4x (`campello_widgets` `CHANGELOG.md`, `Renderer::
      applyBoxShadow()`) — `GPU_EXEC` ~10-12ms → ~5-7ms.
      Separately, `campello_gpu`'s traditional-render-pass-fallback path
      (this device reports Vulkan 1.1, no dynamic rendering) was calling
      `vkCreateRenderPass()`/`vkCreateFramebuffer()` fresh for every
      single offscreen composite despite `buildRenderPass()` being a
      pure function of 4 parameters with only 1-2 *distinct* combinations
      ever occurring per frame — now cached (`campello_gpu`
      `CHANGELOG.md`, `DeviceData::offscreenRenderPassCache`); confirmed
      via the same GPU timestamps that this was real CPU/driver overhead
      but not what was driving the `GPU_EXEC` floor (hardware timestamps
      only capture GPU execution, not CPU-side object creation before
      it). Finally, two swapchain-level changes in `campello_gpu`: raised
      `kFramesInFlight` 2→3 (helps bursty/event-driven workloads like the
      Draw tab; doesn't move a continuously-saturated ticker-driven one —
      it's already submitting flat-out every vsync) and switched the
      default present mode to `VK_PRESENT_MODE_MAILBOX_KHR` (falls back
      to FIFO automatically if unsupported) — this alone eliminated the
      entire `vkAcquireNextImageKHR`/"images in flight" fence wait, which
      turned out to be ~10ms/frame of the CPU just blocked waiting for
      the display to actually want the next image. Verified this doesn't
      hit the classic MAILBOX battery-drain failure mode (an unbounded
      render loop flooding the GPU with frames that get discarded before
      ever being shown): `campello_widgets`' own frame requests are
      already vsync-gated by `FrameScheduler`/the platform choreographer
      callback independent of present mode — measured frame count stayed
      ~60/sec under both FIFO and MAILBOX on this device.
      **Final measured result on the Galaxy Tab S7 FE**: gallery Draw tab
      26fps/37ms raster → **60fps/5.3ms raster** (Images tab: 60fps/6.5ms)
      — roughly a 7x improvement in raster time from the session's
      starting point, landing close to the original 1-3ms aspiration,
      with what remains being genuine GPU shader/fillrate cost rather
      than synchronization or driver overhead. All of the Vulkan-side
      fixes apply equally to Linux (same shared `src/vulkan/*` source set
      in `campello_gpu`, confirmed via `android.cmake`/`linux.cmake` both
      compiling the identical file list) even though only Android
      hardware was available to verify on directly this session.
- [x] **Stylus/pencil input support** (found 2026-07-24, done same session)
      — `PointerEvent` gained `tilt`/`tilt_orientation` fields alongside the
      existing `pressure`; `PointerDeviceKind::stylus`/`invertedStylus` are
      now actually populated by both platform bridges instead of never
      being set. iOS (`src/ios/run_app.mm`): sources pressure from
      `UITouch.force`/`maximumPossibleForce`, device kind from
      `touch.type == UITouchTypePencil`, tilt from `altitudeAngle`,
      orientation from `azimuthAngleInView:`. Android (`src/android/
      run_app.cpp`): sources from `AMotionEvent_getPressure()`/
      `getToolType()` (`AMOTION_EVENT_TOOL_TYPE_STYLUS`/`_ERASER`/
      `_MOUSE`)/`getAxisValue(..., AXIS_TILT/_ORIENTATION, ...)`. New
      `isPrecisePointer()` (`gesture_constants.hpp`) extends the existing
      mouse/trackpad "precise pointer" gesture-slop treatment to stylus
      input too (a pen tip isn't a fingertip). Verified live on a Galaxy
      Tab S7 FE with an S Pen (both stylus and finger input checked); iOS
      side is code-complete but not yet verified on a real iPad/Pencil.
- [x] **Gallery: new "Draw" tab — freehand canvas widget** (found
      2026-07-24, done same session) — new `RenderDrawSurface`
      (`RenderImage` subclass + `GestureArenaMember`, mirroring
      `RenderSlider`'s pointer-handling pattern) and `DrawSurface` widget.
      Strokes accumulate into a persistent, dedicated GPU texture rather
      than replaying the full stroke history every frame: only the new
      segment since the last paint is submitted, via a new
      `DrawSurfaceUpdateBeginCmd`/`EndCmd` draw-command bracket and
      `Renderer::applyDrawSurfaceUpdate()` (uses `beginOffscreenPass(...,
      preserve_content=true)`, a new parameter added to `IDrawBackend`
      for exactly this — LOAD instead of CLEAR). Strokes are built from
      stamped `drawCircle` calls rather than `drawLine`, since Vulkan
      doesn't implement the latter. Pressure modulates stroke width
      (mouse/finger reports a constant 1.0, so this degrades gracefully
      without a pencil, per the fallback requirement below). Resizing the
      canvas blits the old texture into the new one (`blit_source` on the
      update command, via `CommandEncoder::copyTextureToTexture`) instead
      of clearing it, cropping/extending like a real drawing app rather
      than wiping the drawing. Root-caused and fixed a real bug found
      during macOS testing along the way: `RenderImage::setTexture()`
      calls `markNeedsPaint()` internally, which — when called from
      *inside* `performPaint()` (where the base class had already cleared
      the dirty flag for that frame) — permanently wedged the flag with
      nothing left to ever clear it again, silently killing every future
      repaint after the first. Fixed by moving the texture (re)allocation
      into `performLayout()` instead, which always runs before paint in
      the same frame. Verified interactively on macOS (mouse) and Android
      (S Pen + finger, Galaxy Tab S7 FE).
- [ ] **Tap unresponsive after prolonged infinite animation** (found
      2026-07-24) — in the gallery example, the Images tab (or any tab
      driving an infinite/looping animation) stops responding to taps after
      a few seconds. Symptom suggests a queue backing up or an event being
      dropped/lost under sustained ticker load. Needs repro + tracing of
      `PointerDispatcher`/`TickerScheduler` interaction during a long-running
      animation to find where input is starved or queued indefinitely.
- [ ] **Android gallery example not fullscreen** (found 2026-07-24) — the
      Android gallery example should render fullscreen (edge-to-edge, under
      the system bars), not just in the space below them. Needs forcing
      Android fullscreen/edge-to-edge mode, then verifying `SafeArea` insets
      correctly account for the system bars once content is drawn behind
      them.

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

**The documented caveat above was hit for real (2026-06-21, user-reported)** —
and not by `Stack`/`Positioned` as originally worried about, but by plain
`Row`/`Column`: in `campello_editor`'s Scene tab, the hierarchy/inspector
panels wrapped in `RepaintBoundary` (see above) stayed visually stuck at
their old position, unclipped to their new bounds, when the window was
resized. `RenderFlex::performLayout()` recomputes each child's offset on
every layout pass but — correctly, by the same logic as `positionChild()` —
never calls `markNeedsPaint()` for a pure reposition, since every other
`RenderObject` always repaints at the fresh offset regardless. The boundary
had no way to know its cached offset had gone stale.

**Fix**: `RenderRepaintBoundary` now tracks the offset it last
painted/cached at itself and treats a change as dirty (forces a fresh
recording), instead of relying on every caller to remember not to
reposition it without also repainting it. Fully self-contained — no changes
needed to `RenderFlex`/`Stack`/`positionChild()`, which were never the
problem. New regression test:
`RenderRepaintBoundary.RepositionWithoutMarkNeedsPaintForcesReRecordingAtNewOffset`.
See CHANGELOG `[0.3.6]`.

### Paint/Compositing Architecture — Closing the Gap with Flutter (2026-07-01)

Prompted by a gallery-app performance report (continuous animation in one small
widget driving ~10ms UI / ~50% GPU on a page with only 4 animated images),
fixed short-term by manually wrapping the page's independent sections in
`RepaintBoundary` (see gallery `ImagesSection` / `RotatingTransformRow`,
`examples/gallery/gallery_app.cpp`). This section is the precise, code-grounded
answer to the follow-up question: what's actually missing to make our
paint/compositing pipeline behave like Flutter's, not just patchable
case-by-case.

**What we already have (verified, not assumed):**
- `RenderObject::layout()` (`src/ui/render_object.cpp:74-87`) already skips
  `performLayout()` per-node when clean and constraints are unchanged —
  layout is not part of this gap.
- Frame-level gating already exists: `Renderer::renderFrame()` bails before
  even generating a draw list when `!root_->needsPaint()`
  (`src/ui/renderer.cpp:89-90`), and `FrameScheduler` only requests a frame
  when something calls `scheduleFrame()` — an idle app costs ~0.
- `RepaintBoundary`/`RenderRepaintBoundary` (added 2026-06-20, hardened
  2026-06-20/21 — see above) is a real, working, Flutter-mirroring paint
  cache: it's the *only* place in the framework that checks `needsPaint()`
  before deciding whether to re-walk a subtree at all.
- Scroll offset changes correctly call `markNeedsPaint()`
  (`render_single_child_scroll_view.cpp:165`), so `RepaintBoundary`'s
  existing offset/dirty check is not silently bypassed by scroll — confirmed
  by reading the code, not assumed.
- Draw-call-level costs (text texture cache, bind-group/buffer pooling, UI/
  raster thread split) are already addressed — see the sections above. This
  section is specifically about the *tree-walk* and *compositing* layer, one
  level above those fixes.

**The real, remaining gaps vs Flutter, in order of impact:**

1. **No dirty-node list — paint is always O(whole reachable tree), not
   O(dirty boundaries).** `RenderObject::paint()` (`render_object.hpp:78`,
   `render_object.cpp:89-105`) calls `performPaint()` **unconditionally** —
   there is no `if (needsPaint())` gate anywhere except inside
   `RenderRepaintBoundary`'s own override. `RenderBox::paintChild()`
   (`render_box.cpp:26-30`) likewise calls `child_->paint()` unconditionally.
   So when `root_->paint()` runs (which it does every frame *anything*
   anywhere is dirty, since `markNeedsPaint()` always propagates to root),
   it re-walks and re-emits draw commands for **every** node in the tree,
   clean or not, unless that specific node happens to be a boundary that's
   currently clean. Flutter's `PipelineOwner.flushPaint()` iterates only
   `_nodesNeedingPaint` (a flat list of dirty repaint-boundary layers) —
   clean subtrees are never visited during paint, full stop, no manual
   wrapping required to get *that* baseline behavior for the boundaries that
   do exist.

2. **`RepaintBoundary` caches a flat `DrawList` slice, not an independent
   layer — so a cache hit still costs a full GPU resubmit, and a
   reposition still forces a full re-record.** Confirmed in
   `render_repaint_boundary.cpp` and `renderer.cpp:561-577`:
   `flushDrawList()` re-derives the composited transform/clip stack by
   replaying commands in order every frame (`current_transform =
   current_transform * c.transform`), and a boundary's "replay" is just
   `appendRecorded()` — splicing raw commands back into the same flat list.
   Two consequences: (a) a cache hit skips the C++ tree-walk/re-record cost
   but **not** the Metal draw-call submission cost — already flagged in this
   file's "further work" notes on the raster-cost investigation, but never
   tied back to `RepaintBoundary` specifically; (b) because there's no
   independent transform a parent can update without touching the cached
   geometry, any reposition must invalidate the whole cache (this is why
   `RenderRepaintBoundary` had to grow the `cached_offset_` check at
   2026-06-21 — a correct fix for a symptom of not having real layers, not a
   bug in that fix itself).

3. **Zero automatic repaint-boundary promotion anywhere in the framework.**
   Flutter's `RenderViewport`/`RenderShrinkWrappingViewport` unconditionally
   override `isRepaintBoundary => true` — every `ListView`, `GridView`,
   `SingleChildScrollView`, `PageView` in a Flutter app is a boundary whether
   or not the app author thinks about it, so scrolling never repaints the
   rest of the page and vice versa, automatically. Grepping this codebase
   (`grep -rn RenderRepaintBoundary src/ inc/`) turns up exactly one
   production usage site before today: the widget's own definition — every
   other use is 100% manual app-level opt-in (including the gallery fix that
   prompted this analysis). This is the single highest-leverage, lowest-risk
   item here: making our own `RenderSingleChildScrollView` / `RenderListView`
   / `RenderGridView` / `RenderPageView` internally boundary themselves closes
   the most common real-world case for free, no app code changes required.

4. **`markNeedsLayout`/`markNeedsPaint` always propagate all the way to
   root — no "relayout boundary" short-circuit.** Flutter stops
   `markNeedsLayout()` at the nearest ancestor whose own size can't depend on
   the child (`relayoutBoundary`), so a leaf resize inside a
   tightly-constrained subtree dirties nothing above that point. Ours
   (`render_object.cpp:48-58`) always calls `parent_->markNeedsLayout()`
   unconditionally to the root. Lower severity than #1 because `layout()`
   still dirty-checks per node (ancestors' `performLayout()` gets skipped
   correctly), but it compounds #1: every ancestor still gets *visited* by
   `paint()`, and paint has no per-node skip to fall back on.

5. **Widget-level reconciliation over-marks dirty regardless of value
   equality.** Already noted once in this file in passing (see the
   `campello_editor` RepaintBoundary writeup) but worth calling out as its
   own gap: `Flex`/`ColoredBox`'s `updateRenderObject()` call
   `markNeedsLayout()`/`markNeedsPaint()` unconditionally on every
   reconciliation, even when the new value is bit-identical to the old one.
   This is the pattern most likely to silently defeat every other fix on
   this list, since it's copy-pasted across most `RenderObjectWidget`s, not
   confined to one file.

**Why #1/#2 together are the real "Flutter parity" blocker**: fixing #1
alone (a dirty-node list) has nowhere to splice a skipped boundary's cached
output back into the frame without walking through its non-boundary
ancestors anyway — you need #2 (each boundary owns a persistent,
independently-positionable output) for #1 to actually pay off. In Flutter
these are the same feature: the `Layer` tree (`ContainerLayer`,
`PictureLayer`, `TransformLayer`, `ClipRectLayer`, `OpacityLayer`,
`OffsetLayer`) *is* both the dirty-list anchor and the thing that lets a
parent recomposite without touching a child's rasterized content. We don't
have a layer tree — `DrawList` is a flat, order-dependent command stream
where ambient state (transform/clip/opacity) is baked in via
Push/Pop-command pairs, not an addressable tree of independently-cacheable
nodes. That's the actual, single structural gap; everything above is a
symptom of not having it.

**Staged proposal:**

- [x] **Stage 0a — kill unconditional dirty-marking on reconciliation
      (gap #5) — implemented 2026-07-01.** Added `operator==` (defaulted)
      to `BoxBorder`, `BoxShadow`, `BoxDecoration` (`inc/campello_widgets/
      ui/box_border.hpp`/`box_shadow.hpp`/`box_decoration.hpp` — `EdgeInsets`
      and `Color` already had one). Guarded `Flex::updateRenderObject()`
      (`src/widgets/flex.cpp`), `ColoredBox::updateRenderObject()`
      (`colored_box.cpp`), `DecoratedBox::updateRenderObject()`
      (`decorated_box.cpp`), and `Padding::updateRenderObject()`
      (`padding.cpp`) with an equality check before touching the render
      object and calling `markNeedsLayout()`/`markNeedsPaint()`. Full
      universal suite unaffected (402/402 before → still 402/402 after,
      pre-existing coverage; no test asserted on the old always-dirty
      behavior). Not yet extended past these four — same pattern should be
      applied opportunistically to other `RenderObjectWidget`s as they're
      touched.

      **Extended 2026-07-01**: audited every `updateRenderObject()` in
      `src/widgets/` for the same unconditional-dirty-marking pattern.
      Found and fixed 11 more: `Align`, `AspectRatio`, `ClipRRect`,
      `ConstrainedBox`, `FractionallySizedBox`, `IntrinsicHeight`,
      `IntrinsicWidth`, `SizedBox`, `Stack`, `Transform`, `Wrap` — several
      of which are extremely common (`Transform` is the exact widget
      driving the gallery's own animation; `Stack`/`Align`/`SizedBox`
      appear in nearly every layout). `Transform`'s guard compares its
      `Matrix4` field via `vector_math::Mat`'s inherited `operator==` (no
      manual comparison needed — `Matrix4` extends `Mat<float,4,4>` which
      extends `Vec<float,16>`, which already has one). `ClipPath` was
      *not* fixed — its `clip_path_builder` field is a
      `std::function<Path(Size)>`, which has no meaningful equality
      comparison, so no guard is possible there. Full suite: still 424/424
      (no new tests — this exercises the same reconciliation path the
      Stage 0a tests already implicitly cover; the risk here is purely
      "did the equality comparison compile/typecheck correctly for each
      field," which the build itself verifies). Gallery rebuilt/relaunched
      for a visual sanity check.
- [x] **Stage 0b — auto-boundary the scrollables (gap #3) — implemented
      2026-07-01.** Extracted `RenderRepaintBoundary`'s caching mechanism
      into a standalone, composable `PaintCache`
      (`inc/campello_widgets/ui/paint_cache.hpp` / `src/ui/paint_cache.cpp`)
      — `maybeReplay(context, offset, dirty)` / `record(context, offset,
      paintContentFn)`, exactly the same offset-tracking/absolute-clip-rect
      semantics as before, just no longer tied to being a
      `RenderRepaintBoundary`. `RenderRepaintBoundary::paint()` itself now
      just delegates to a `PaintCache` member (refactor only, behavior
      identical — all 6 existing `RenderRepaintBoundary` tests pass
      unchanged). `RenderSingleChildScrollView`, `RenderListView`,
      `RenderGridView`, `RenderPageView` each gained their own `PaintCache`
      member and a `paint()` override that composes it around their
      existing `performPaint()`, mirroring Flutter's
      `RenderViewport.isRepaintBoundary` — every scrollable in the
      framework is now an implicit repaint boundary with no app code
      changes required. New test file
      `tests/universal/test_render_scrollable_paint_cache.cpp` (8 tests, 2
      per class: clean-replay-skips-child, dirty-forces-re-record) — note
      this verifies the *wiring*, not the underlying cache mechanics
      (already covered exhaustively by `test_render_repaint_boundary.cpp`).
      Full suite: 410/410 (402 + 8 new).

      *Superseded 2026-07-01 by Stage 0d below*: `PaintCache` was replaced
      by `PictureLayer`/`OffsetLayer` in all 5 classes named above, and
      `paint_cache.hpp`/`.cpp` were deleted. The 8 tests here still pass
      unchanged (same suite names, migrated implementation underneath) —
      see Stage 0d for what actually changed and why.
- [x] **Stage 0c — measure — done 2026-07-01.** Re-measured after 0a+0b+0d
      (UI 11ms / raster 2.5ms, unchanged from the 0a/0b baseline as expected —
      Stage 0d only helps clip-free content and this page has none) and again
      after the offscreen-texture-pooling fix below (raster 1.6ms, GPU
      ~50%→15-20%). See that section for the full result and the decision to
      stop there rather than pursue Stage 1/2/3.
- [x] **Stage 0d — `PictureLayer`/`OffsetLayer`: safe reposition without a
      full re-record (a narrow slice of gap #2 only) — implemented
      2026-07-01.** The user asked to proceed to "Stage 1" (below); before
      attempting the full rewrite, investigated (3 parallel Explore agents +
      1 Plan agent, cross-verified by hand against `render_object.cpp`/
      `canvas.cpp`/`renderer.cpp`) whether a smaller, safe first slice could
      ship without the full-rewrite risk. Two findings changed the scope:
      (1) a `Renderer`-owned dirty-layer registry doesn't pay off at any
      scope smaller than the full 41-class rewrite, since `markNeedsPaint()`
      (`render_object.cpp:60-72`) has no boundary short-circuit and
      `generateDrawList()` always walks `root_->paint()` unconditionally
      once root is dirty — so a registry would have nowhere new to plug in;
      (2) naively repositioning cached content via a wrapping
      `canvas.translate()` is *unsafe* for any content containing a clip,
      backdrop filter, or shader mask — `Renderer::flushDrawList()` treats
      `PushClipRectCmd` as an absolute reassignment
      (`current_clip = c.rect;`, `renderer.cpp:574-577`), never composed
      with the transform stack the way `PushTransformCmd` is
      (`current_transform = current_transform * c.transform`,
      `renderer.cpp:564`) — so a cached slice with a nested clip/backdrop-
      filter/shader-mask would silently clip/blur/mask at the *old* position
      after a naive translate-based reposition. Confirmed this isn't
      hypothetical: the gallery app that motivated this whole investigation
      wraps its images in `ClipRRect`.

      **Scope shipped** (user explicitly chose this over the full rewrite —
      see the two other options below, still undecided): extracted
      `PaintCache`'s mechanism into two classes —
      `PictureLayer` (`inc/campello_widgets/ui/picture_layer.hpp` /
      `src/ui/picture_layer.cpp`) records a `DrawList` slice and scans it
      once for the exact command types `flushDrawList()` treats as
      absolute-baked (`PushClipRectCmd`/`PushClipRRectCmd`/
      `PushClipOvalCmd`/`PushClipPathCmd`/`DrawBackdropFilterBeginCmd`/
      `DrawShaderMaskBeginCmd`/`SaveLayerCmd`), setting
      `hasUnsafeGeometry()`; `OffsetLayer`
      (`inc/campello_widgets/ui/offset_layer.hpp` / `src/ui/offset_layer.cpp`)
      composes a `PictureLayer` and is a strict, provably-safe superset of
      `PaintCache` — every path `PaintCache` handled is byte-identical
      (dirty/no-cache forces re-record, identity replay skips straight to
      `appendRecorded()`), plus one new path: when only the offset changed
      and the picture has no unsafe geometry, it replays under an
      additional delta `canvas.translate()` instead of re-invoking
      `paintContent` — safe because ordinary draw geometry *is*
      transform-deferred, confirmed by the Metal backend applying
      `transform` to every drawn quad's vertices.
      `RenderRepaintBoundary`/`RenderSingleChildScrollView`/
      `RenderListView`/`RenderGridView`/`RenderPageView` all migrated from
      `PaintCache` to `OffsetLayer` (mechanical two-line swap each);
      `paint_cache.hpp`/`.cpp` deleted (grepped for zero remaining
      references first).

      **Honest result, not oversold**: only `RenderRepaintBoundary` actually
      benefits from the new cheap-reposition path — its content is often
      clip-free (new test:
      `RenderRepaintBoundary.RepositionWithClipFreeChildReplaysViaDeltaTranslateWithoutRewalkingChild`).
      The four scrollables *always* clip their own content to their own
      viewport inside `performPaint()`, so `hasUnsafeGeometry()` is always
      true for them and the cheap path never triggers — reposition still
      falls back to a full re-record for all four, identical to
      `PaintCache`'s behavior (new tests, one per class:
      `RepositionForcesReRecordDueToOwnViewportClip` in
      `test_render_scrollable_paint_cache.cpp`, asserting the *unchanged*
      behavior explicitly rather than a win that doesn't apply to them).
      This slice does **not** reduce the O(tree) paint-walk cost to
      O(dirty) — that's still gap #1, unaddressed, and still requires the
      full `Layer` tree below. It also does **not** fix the original
      gallery GPU-usage complaint, since the gallery's `ClipRRect`-wrapped
      images are exactly the unsafe-geometry case that still falls back to
      full re-record — Stage 3 (GPU-side raster cache) remains the actual
      fix for that, unchanged from the original analysis.

      New tests: `tests/universal/test_picture_layer.cpp` (4),
      `tests/universal/test_offset_layer.cpp` (5, including
      `RepositionWithClipFreeContentReplaysViaDeltaTranslate` — the
      standalone mechanism test — and `RepositionWithUnsafeGeometryForces
      ReRecording`), plus 1 new `RenderRepaintBoundary` test and 4 new
      scrollable tests as above. Full suite: 424/424 (410 + 4 + 5 + 1 + 4).

### Offscreen texture pooling for `applyClipShape()`/`applyShaderMask()` (2026-07-01)

Re-measuring the gallery's Images tab after Stage 0d found UI time
unchanged (~11ms, was ~10ms) — expected, since Stage 0d only helps
clip-free content and every image on that page is wrapped in `ClipRRect`.
Rather than chase gap #1's full `Layer` tree (still unstarted, see Stage 1
below) or gap #2(a)'s GPU raster cache (Stage 3, targets *static* content),
identified a more targeted, narrower fix for the specific workload that
motivated this whole investigation: `RotatingTransformRow`'s 4
continuously-animating `ClipRRect`-wrapped images. This content is dirty
*every single frame by design* (it's an active animation) — no caching
strategy, including Stage 3's, can help it, since there's never a clean
state to cache. What actually costs real GPU time on every one of those
frames is `Renderer::applyClipShape()`
(`src/ui/renderer.cpp:745-821`, also `applyShaderMask()` at `renderer.cpp:665-743`)
calling `draw_backend_->createOffscreenTexture()` — which, in
`MetalDrawBackend` (`src/macos/metal_draw_backend.mm`), called
`device_->createTexture()` — a real GPU allocation — **on every single
call, every frame, with zero reuse**, despite the `IDrawBackend` interface's
own doc comment already saying "Allocates (or reuses)..."
(`inc/campello_widgets/ui/draw_backend.hpp:190-197`) — the "reuses" half was
never actually implemented.

**Fix**: added `MetalDrawBackend::OffscreenTexturePool`
(`src/macos/metal_draw_backend.hpp`/`.mm`), mirroring the existing
`UniformBufferPool` pattern exactly (a `kGenerations=4`-deep ring, advanced
once per real frame via `setViewport()` — not `setViewportSize()`, for the
same reason documented on `UniformBufferPool`: `setViewportSize()` is
called mid-frame, possibly several times per composite, and must not
advance the ring or it corrupts earlier not-yet-submitted draws in the same
command buffer that still reference a pooled resource by index). Textures
are pooled keyed by `(width, height)` since offscreen bounds vary per
widget (unlike uniform buffers, which have one fixed struct size per pool
instance) — a repeated `(size, count-per-frame)` pattern converges to zero
new GPU allocations after a ~4-frame warmup (one warmup frame per
generation slot). No `upload()`-equivalent refresh step is needed on reuse,
since `beginOffscreenPass()`'s `loadOp=clear` already re-clears the texture
before each use. Size buckets not acquired in `kMaxAgeFrames=120` frames are
evicted (mirroring `evictStaleTextTextures()`'s existing eviction pattern
exactly) so window resizes — which generate a whole new set of distinct
sizes each time — don't grow the pool unboundedly.
`MetalDrawBackend::createOffscreenTexture()` now delegates to the pool
instead of allocating directly.

Universal suite unaffected (still 424/424 — this is a macOS/Metal-backend-
only change with no CPU-testable surface). GPU visual-fidelity golden tests
remain skipped in this sandboxed dev environment (missing Flutter golden
fixtures, pre-existing limitation, unrelated to this change) so pixel
correctness wasn't re-verified by automated tests; verified instead via a
clean `darwin-debug-integration` build/run (real Metal device, Intel UHD
Graphics 630, created successfully) and a full gallery rebuild/relaunch,
with the user confirming correct rendering.

**Result (2026-07-01, user-reported)**: Images tab, `RotatingTransformRow`
animating — **raster: 2.5ms → 1.6ms** (-36%, on top of the earlier
Stage-0d-era 4ms → 2.5ms from the `RepaintBoundary`/self-boundaring work,
so ~3ms → 1.6ms overall for this specific fix's target). UI time unchanged
(~10-11ms), exactly as expected — this fix is GPU/raster-side only, no
CPU-side paint-recording cost was touched. **GPU utilization: ~50% → 15-20%**
— the actual metric from the original complaint ("I think very very high to
just 4 simple widgets") and the real headline result: roughly a 3× reduction.
Combined with the earlier session's manual `RepaintBoundary` wraps around
the page's static sections (which already stopped the ~50% baseline from
including the *static* BoxFit/Decorations rows' and BackdropFilter's
offscreen-composite/blur work re-running every animation frame — those
sections now clean-replay instead), the remaining 15-20% is plausibly close
to the floor achievable without a deeper architecture change: it's the
genuine, unavoidable cost of 4 GPU offscreen-composite passes per frame for
content that's continuously dirty by design (an active animation), which no
caching strategy — including Stage 3's raster cache — can reduce further,
since there's never a clean state to cache.

**If further reduction is wanted later**: the next real lever isn't Stage 1
or Stage 3, but replacing `ClipRRect`/`ClipOval`'s offscreen-texture+SDF-
composite approach with in-pass GPU clipping (stencil buffer or shader-
based), avoiding the extra render pass entirely for the animating case —
already flagged as a known simplification in `Canvas::clipRRect()`/
`clipOval()`'s own code comments ("For now, clip to the bounding rect... 
full implementation would need GPU stencil buffer or shader-based
clipping"), independent of anything in the Stage 1 Layer-tree discussion.
Not started; no immediate need given the result above.

### Bug: `BackdropFilter` inside a scroll didn't respect offset (2026-07-01, user-reported)

Root-caused as a real correctness bug in the paint-caching mechanism itself
(`RenderRepaintBoundary`/`OffsetLayer`), introduced by the `repaintBoundary
(blur_container)` wrap from earlier in this session (the very first manual
fix, before any of the "Stage 0" work) — a case where general-purpose paint
caching is fundamentally unsafe for one specific widget's semantics.

**Chain of causes**: (1) `blur_container` (the gallery's `BackdropFilter`
demo) is wrapped in a `RenderRepaintBoundary`. (2) When the page's outer
`SingleChildScrollView` scrolls, it calls `markNeedsPaint()` on *itself*
only — dirty propagation is upward-only (`render_object.cpp:60-66`), so
`blur_container`'s own `needs_paint_` never gets set. (3) `blur_container`'s
*logical* offset within its parent `Column` doesn't change during scroll
either — scrolling shifts content via `canvas.translate()`, a separate,
parallel mechanism from the `offset` parameter threaded through `paint()`
calls. (4) So `RenderRepaintBoundary::paint()` sees `needsPaint()==false`
and `offset == cached_offset_` → takes the clean **identity replay** path
(`OffsetLayer::maybeReplay()`, `canvas.appendRecorded(...)`) — which never
re-invokes `paintChild()`/`RenderBackdropFilter::performPaint()`.
(5) `RenderBackdropFilter::performPaint()`'s call to `Renderer::
noteBackdropFilter()` (`render_backdrop_filter.cpp:33-34`) therefore never
fires that frame. (6) `package.has_backdrop_filter` stays false →
`rasterFrame()` skips the entire full-viewport backdrop-capture-and-blur
pre-pass for that frame (`renderer.cpp:186-238`). (7) `blurred_backdrop_
tex_` never gets re-captured — it stays frozen at whatever was behind the
widget the last time it was genuinely re-recorded. Meanwhile the
*destination* quad position (`drawBackdropFilter()`'s `transform *
Vector4(cmd.bounds...)`, `metal_draw_backend.mm:1207-1208`) **does**
correctly track the ambient transform (including the scroll's translate),
since ordinary ambient-transform composition is unaffected by any of this
— so the frosted panel visually *slides* with the scroll, while the blur
it displays stays stale/frozen. That mismatch is exactly "doesn't respect
offset."

**Why this is a real, general hazard, not a one-off**: `RenderClipRRect`/
`RenderClipOval`/`RenderShaderMask` are all safely cacheable — their
offscreen-composite output depends *only* on their own child content, so
replaying a cached recording is semantically correct. `BackdropFilter` is
different: its output depends on ambient, external state (whatever's
currently behind it), refreshed via a side effect
(`noteBackdropFilter()`) that paint-caching's whole design intentionally
skips on a cache hit. Any future paint-caching mechanism — Stage 1's
`Layer` tree included — needs to keep BackdropFilter content permanently
uncacheable for this same reason, not just work around it locally.

**Fix**: added `PictureLayer::hasBackdropFilter()`
(`inc/campello_widgets/ui/picture_layer.hpp` / `src/ui/picture_layer.cpp`)
— a stricter, separate flag from `hasUnsafeGeometry()`, set when a
recording contains `DrawBackdropFilterBeginCmd`. `OffsetLayer::
maybeReplay()` now checks it *before* the identity-offset fast path (not
just the reposition fast path `hasUnsafeGeometry()` already gated) and
returns `false` unconditionally — forcing every caller
(`RenderRepaintBoundary` and all four self-boundaring scrollables) to
`record()` fresh on every single paint call, dirty or clean, moved or not,
whenever backdrop-filter content is present anywhere in the cached
subtree. This sacrifices the CPU-side caching benefit specifically for
BackdropFilter-containing content, which is correct and low-cost: the
backdrop-capture-and-blur pre-pass it triggers is already expensive
regardless of paint-caching, so skipping the (comparatively cheap)
`paintChild()` re-walk isn't adding meaningful additional overhead.

New tests: `PictureLayer.PlainClipContentHasNoBackdropFilter` (confirms
the two flags are independent — an ordinary clip must not trip the
backdrop-filter-specific lockout) and a `hasBackdropFilter()` assertion
added to the existing `PictureLayer.BackdropFilterContentIsUnsafe` test;
`OffsetLayer.BackdropFilterContentNeverReplaysEvenAtUnchangedOffset` (the
core regression test — proves an identity-offset, clean replay still
forces `record()`, three times in a row, not just once as a fallback).
Full suite: 426/426 (424 + 2 new). Gallery rebuilt/relaunched; awaiting
user confirmation that scrolling the Images tab now keeps the frosted
panel's blur content live instead of frozen.

### Bug: `Transform` content vanishes past a negative-scale rotation angle (2026-07-01, user-reported)

User reported the gallery's animated `Transform` demos (`RenderTransform::
scaling()`-based flip effects simulating X/Y-axis 3D rotation, see
`examples/gallery/gallery_app.cpp`'s `RotatingTransformRow`) make their
child "disappear... like the backside is disabled" for part of the
rotation cycle. Root-caused via the shader math, not by guessing:

Every `labeledTransform()` demo wraps its image in a `ClipRRect`, whose
paint goes through `Renderer::applyClipShape()` →
`MetalDrawBackend::drawClipShapeComposite()`. That function transforms the
clip bounds' corners by the ambient `transform`
(`tl = transform * Vector4(bounds.left, bounds.top, 0, 1)`, similarly for
`br`) and passed `dstRect = {tl.x, tl.y, br.x-tl.x, br.y-tl.y}` straight
into `ClipShapeUniforms` — **unnormalized**. When the ambient transform has
a negative scale axis (exactly what `scaling(1, cos(angle))`/
`scaling(cos(angle), 1)` produce for roughly half of every rotation cycle,
by design — that's how the flip effect works), `tl.x() > br.x()` (or
`.y()`), making `dstRect`'s width or height **negative**.

The fragment shader (`shaders/metal/widgets.metal`,
`clipShapeFragment()`) computes the rounded-rect/ellipse SDF as
`hs = rect_size * 0.5; q = abs(p) - hs + r; d = length(max(q,0)) +
min(max(q.x,q.y),0) - r`, assuming `hs` (half-extents) is non-negative. A
negative `rect_size` makes `hs` negative, which (traced through the
algebra) makes `d` large and positive everywhere, and `alpha =
1.0 - smoothstep(-0.5, 0.5, d)` saturates to **exactly 0** — fully
transparent, for the *entire* shape, not just clipped at an edge. This
exactly matches the reported symptom: the *destination position* of the
flipped content still correctly tracks the ambient transform (ordinary
transform composition is unaffected), so it looks like it's still
rotating right up until it silently vanishes. Backface culling was
checked and ruled out first (`CullMode::none` on every pipeline,
confirmed by grep) before tracing into the shader math. `ShaderMask`'s
fragment shader was also checked and confirmed *not* to have this
class of bug — its gradient math uses absolute fragment position, not a
sign-sensitive half-extent computation, so it was left untouched.

**Fix**: `drawClipShapeComposite()` now normalizes `dstRect` to always be
non-negative (`x0/y0 = min(tl,br)`, width/height via `std::abs`), and
separately computes `flip[2]` booleans from the original corner
ordering. `ClipShapeUniforms` gained a `float2 flip` field (both the C++
struct in `metal_draw_backend.mm` and the shader struct in
`widgets.metal`, kept in sync by hand — no shared header between them).
The vertex shader (`clipShapeVertex()`) still computes screen *position*
from the un-mirrored quad-corner interpolant against the now-always-positive
`dstRect` (correct — position must stay a normal, un-mirrored rect in
screen space), but computes the *UV* used for both texture sampling and
the SDF's local-position math as `flip.x > 0.5 ? 1.0-t.x : t.x` (and same
for `.y`) — mirroring the sampled content and preserving the SDF's
symmetry (`abs(p)` is unaffected by which direction UV runs) at the same
time. Rebuilt the shader via `./build_metal_shaders.sh` (the `.metal`
source is compiled offline into `src/shaders/metal_widgets.h`, a
generated header — editing the `.metal` file alone does nothing until
this script reruns).

No new automated test — this is a Metal/GPU-shader-only change with no
CPU-testable surface, and the golden-image visual-fidelity tests remain
skipped in this sandboxed environment (same pre-existing limitation noted
throughout this file). Full universal suite unaffected (426/426, expected
— nothing here touches CPU-side code). Verified via a clean build (shader
compiled without error) and gallery rebuild/relaunch; awaiting user
visual confirmation that the flip demos no longer vanish past their
zero-crossing angle.

**Separately raised, not fixed here — genuine 3D perspective**: the user
also asked why axis rotations don't have "3D perspective feeling." This
is a different, much larger question — not fixable as a small patch, and
not part of the paint/compositing Stage 1-3 sequence above (orthogonal
capability, not a caching/dirty-tracking gap). Recorded here as its own
backlog item.

### Backlog: genuine 3D perspective for `Transform` (raised 2026-07-01, not started — scope corrected same day)

**Initial framing (superseded below)**: originally described as "missing a
perspective (W) divide" — true, but investigation the same day found this
significantly understates the gap.

**Corrected understanding, verified by reading every quad-drawing vertex
shader in `shaders/metal/widgets.metal` (`quadVertex`, `shapeVertex`,
`clipShapeVertex`, `shaderMaskVertex`), not assumed from memory**: every
one of them builds its four vertices as `pos = dstRect.xy + t * dstRect.zw`
— one origin corner plus a width/height, interpolated by a corner selector
`t ∈ {0,1}²`. That expression can only ever produce an **axis-aligned
rectangle** on screen; there is no shader anywhere in this renderer that
accepts four independent vertex positions. On the C++ side,
`MetalDrawBackend::drawRect()` transforms all four corners by the ambient
matrix but then explicitly takes their axis-aligned bounding box (comment
in the code: *"exact for translate and scale transforms... for rotation
the AABB will be larger than the actual rotated quad"*); `drawImage()`
transforms only two opposite corners and subtracts to get a width/height.
Both discard rotation before it ever reaches the GPU — a rotated
rectangle becomes a resized axis-aligned box, not a tilted one.

This means the missing W-divide was the *smaller* of two problems. Even
with a perspective-correct divide added, there is currently nowhere in
the pipeline to route the resulting four independently-projected corners
— every shader would still collapse them back into a bounding box.
**The real prerequisite is arbitrary-quad rendering** (shaders taking four
independent vertex positions, backend code that stops collapsing
transformed corners into a bounding box) — perspective is the *second*
step on top of that, not the first.

**Why Flutter/Skia/Impeller's `setEntry(3,2,0.001)..rotateX(angle)` trick
can't just be ported as-is**: it works in Flutter because Skia (and
Impeller) transform a rect's four corners independently and rasterize
whatever quadrilateral results — rotated, skewed, or a perspective
trapezoid, all the same general mechanism, with the GPU rasterizer's
native perspective-correct interpolation handling texture mapping across
it. This renderer never had that general capability at all, independent
of perspective.

**User's explicit direction (2026-07-01): move toward this general
capability — "the exact way skia/impeller works," aiming for 1:1
rendering fidelity.** This is a large, foundational rendering-pipeline
change, not a bounded bug fix, and has not been scoped into concrete
steps yet — see the scoping conversation this triggered (Plan Mode) for
whatever gets decided as the first real step. Recording the shape of the
work as currently understood, for whoever picks this up:

- A general "arbitrary quad" (or full mesh) draw primitive: shaders
  accepting four (or more) independently-positioned, independently-UV'd
  vertices instead of `dstRect`+corner-selector. Needed for rotation to
  render as a genuinely tilted shape at all, before perspective is even
  relevant.
- True clip-space vertex output (`float4(x, y, z, w)` from a real
  projection matrix) with a GPU-native perspective divide, replacing the
  current `ndc = (px/viewport)*2-1` pattern used by every shader.
- `DrawRectCmd`/`DrawImageCmd` (or new commands) carrying real corner
  geometry instead of an axis-aligned `(x,y,w,h)` rect — a
  draw-command-contract change every backend must implement, not an
  internal-only change.
- The composite/offscreen paths (`applyClipShape()`, `applyShaderMask()`,
  backdrop-filter capture) have the *same* two-corner/bounding-box
  limitation as the primary draw paths (confirmed while fixing the
  negative-`dstRect` bug above — `drawClipShapeComposite()` still reduces
  the clip shape's own destination to an axis-aligned box) — meaning a
  `ClipRRect`-wrapped rotated/perspective widget would need this fixed at
  the composite layer too, not just the leaf draw calls, or the visible
  clip boundary would stay a flat rectangle even while the content inside
  it correctly tilts.
- Mirrored across Metal, Vulkan, and D3D for consistency, since this is a
  public rendering-capability change.
- "1:1 fidelity with Skia/Impeller" as a general target is effectively
  open-ended (those are mature, general-purpose 2D rendering engines with
  vastly larger scope — arbitrary paths, gradients, blend modes, full
  perspective-correct mesh rendering, etc.) — worth explicitly narrowing
  to "what `Transform` specifically needs" as a first bounded milestone
  rather than treating the whole engine's fidelity as one undertaking.

Not started. Scope/sequencing to be decided via the Plan Mode conversation
this direction triggered.

---

- [ ] **Stage 1 — introduce a real `Layer` tree (gaps #1 + #2, the
      structural fix).** New `Layer` base + `ContainerLayer`, `PictureLayer`
      (leaf; owns a recorded `DrawList`), `TransformLayer`, `ClipRectLayer`/
      `ClipRRectLayer`/`ClipPathLayer`, `OpacityLayer`, `OffsetLayer`.
      `RenderObject::paint()` records into this tree instead of directly
      into one flat `DrawList`. A `Renderer`-owned dirty-layer list
      (`PipelineOwner` equivalent) is populated by layer-owning nodes
      instead of (or alongside) the current upward `markNeedsPaint`
      propagation. This is the biggest, riskiest item on this list — it
      touches every `performPaint()` call site in the framework — and
      should be scoped as its own dedicated multi-week effort with its own
      test plan, not folded into an unrelated feature/bugfix branch.
- [ ] **Stage 2 — compositor pass over the Layer tree.** A
      `SceneBuilder`-equivalent that flattens the `Layer` tree into GPU
      commands, visiting only layers whose subtree is actually dirty;
      `TransformLayer`/`OffsetLayer` wrapping a clean `PictureLayer` updates
      its matrix without touching the picture. Depends on Stage 1 existing
      first.
- [ ] **Stage 3 — GPU-side raster cache for clean `PictureLayer`s (closes
      the "(a)" half of gap #2).** For pictures that are expensive to
      rasterize and rarely change (static `ClipRRect`/`BackdropFilter`
      content — the exact case from the original gallery complaint),
      opportunistically rasterize once to an offscreen texture and reuse it
      across frames instead of resubmitting Metal draw calls every frame.
      Mirrors Flutter/Skia's `RasterCache`. This is the piece that actually
      answers "why is the GPU at 50% for 4 simple widgets" at the root,
      rather than working around it by scoping what repaints.

**Recommendation**: do Stage 0 first — it's low-risk, ships independently,
and directly extends work already landed today (the gallery
`RepaintBoundary` fix). Re-measure before committing to Stage 1+, since
that's a genuine framework-core rewrite (comparable in scope to the UI/
raster thread split evaluated below) and its cost/risk should be weighed
against the *measured* remainder, not the current guess.

---

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

**Implemented (2026-06-21).** The raster-cost root cause above was fixed
first (see the per-frame raster-cost section earlier in this doc: ~3.85ms →
~0.89ms on `hello`), so this was revisited proactively ahead of an
anticipated heavier-scene workload, not as a reaction to a live regression.
Scoped to **macOS only**, with a **depth-1** handoff (UI can build frame N+1
while raster is still encoding/submitting/presenting frame N, never more
than one frame ahead) rather than Flutter's full depth-2 pipeline — see
CHANGELOG `[0.3.6]` for the user-facing summary. Notes on how the six
blockers above actually played out:

- **Blockers #2 (global atomics) and #6 (campello_gpu Device thread-safety)
  turned out to be non-issues on inspection**, not things that needed
  fixing: `RenderObject::s_active_backend_`/`s_active_dpr_` are only ever
  touched during layout/paint-recording (confirmed by grep — no call site
  exists in the raster-side code), and campello_gpu's Metal `Device`/
  `MTLCommandQueue` is safe to call from a different thread than created it
  (`MTLCommandQueue` is created once and reused; campello_gpu's own
  bookkeeping is atomics-only).
- **Blocker #1 (no pipeline) was the real work**: new `FramePackage` (value
  type, see CHANGELOG) and `RasterThread` (single-slot mailbox where pickup,
  not completion, unblocks the next `submit()`).
- **Blocker #3 (`ThreadChecker` assumes one thread)**: solved additively —
  `rasterInstance()` is a second, independent checker; the original
  `instance()` and all its existing call sites are untouched.
- **Blocker #5 (platform run loops drive raster on the main thread)**: only
  addressed for macOS (`CampelloMTKDelegate` in `src/macos/run_app.mm`).
  iOS/Android/Windows/Linux still call the synchronous `renderFrame()`
  back-compat wrapper unchanged — still an open item if this gets extended
  to those platforms.
- **A new bug class surfaced and was fixed along the way, unrelated to the
  six original blockers**: `run_app.mm` turned out to compile under MRC
  (`macos.cmake` never had `-fobjc-arc`, unlike `ios.cmake`), so the
  drawable-retain code needed for the cross-thread handoff had to use
  `CFBridgingRetain`/`CFBridgingRelease` rather than `__bridge_retained`
  (a silent no-op under MRC). Fixed properly by enabling ARC for the whole
  macOS target instead of working around it locally — see CHANGELOG
  `[0.3.6]` for why `platform_menu_delegate.mm` stays opted out.
- **Verification**: full test suite (487/487) plus `sample`-captured
  per-thread stacks confirming the main thread sits idle in AppKit's run
  loop while `RasterThread::workerLoop()` does the raster work
  concurrently — not just "it compiles."

### Backlog: `BackdropFilter` GPU cost — downsample the capture before blurring (raised 2026-07-01, not started)

After fixing the "BackdropFilter inside a scroll doesn't respect offset" bug
(above — the fix that made `OffsetLayer` never cache/replay backdrop-filter
content), GPU usage in the gallery's Images tab went from ~15-20% to
**~40%** while `RotatingTransformRow` animates. This is not a new bug — it's
the honest cost of the fix being correct. Traced precisely
(`src/ui/renderer.cpp:186-238`, the backdrop-capture pre-pass in
`rasterFrame()`):

- `backdrop_tex_` is allocated at **full physical viewport resolution**
  (`tw/th = package.viewport_width/height`, `renderer.cpp:192-193`) — no
  downsampling anywhere.
- The capture re-renders **the entire scene** (`flushDrawList(package.
  draw_list, ...)`, the *full* draw list, not just the region behind the
  `BackdropFilter` widget) into that full-res texture every time it runs.
- `blurTexture()` then runs a full-resolution, 2-pass separable Gaussian
  blur over the *entire* captured frame.

Before today's correctness fix, this pass almost never actually ran —
`RenderRepaintBoundary`'s clean-replay path silently skipped
`RenderBackdropFilter::performPaint()` (and therefore the `noteBackdropFilter()`
call that gates this pass) on all but the first frame, which is exactly what
made the blur go stale/frozen. Now it correctly runs every single frame
while anything on the page keeps triggering a repaint (in the gallery,
`RotatingTransformRow`'s continuous animation). Trading a visible
correctness bug for a real, honest GPU cost was the right call, but the
cost itself is worth reducing.

**Concrete next step, not started**: downsample the backdrop capture
before blurring — industry-standard for this exact feature (Skia/Flutter's
real `BackdropFilter` does this too; blur quality doesn't need full-
resolution input, since blurring is inherently a low-frequency operation).
Capture into a texture at, say, 1/2 or 1/4 linear resolution, blur *that*,
then let the existing compositing step (`drawBackdropFilter()`, already
doing a textured-quad draw) upsample naturally via the sampler's linear
filtering when compositing back at full size. Would cut both the capture
render's fill cost and the blur pass's per-pixel cost roughly by the square
of the downsample factor (4× at half-res, 16× at quarter-res) — the main
open question is how small the backdrop texture can go before the
blurred result looks visibly different from a full-res blur, which needs
empirical tuning against `ImageFilter::blur()`'s actual sigma range.

### Backlog: image caching — mostly good, one small redundant copy (raised 2026-07-01, not started)

User asked how the gallery's repeated single image (`mountains.jpg`,
embedded as `kMountainsJpeg`, drawn ~15 times across `fitSample`/deco/
`RotatingTransformRow`/`blur_container`) is cached. Investigated for the
record:

- **Decode + GPU texture: already cached correctly.** `ImageCache`
  (`inc/campello_widgets/ui/image_cache.hpp`, a global LRU singleton
  explicitly modeled on Flutter's `ImageCache`) is keyed by
  `ImageProvider::cacheKey()`. `MemoryImage::cacheKey()`
  (`src/ui/image_provider.cpp:364-373`) hashes the image *content* (first
  1KB of bytes), not any per-call identity — so all ~15 `mountainsImage()`
  call sites in the gallery, despite each constructing an independent
  `MemoryImage` provider, resolve to the *same* cache key and share one
  `LoadedImage` (`inc/campello_widgets/ui/image_provider.hpp:45-58`, which
  holds both the decoded pixels and the `campello_gpu::Texture`). Net
  result: exactly one JPEG decode and one GPU texture upload for the whole
  gallery, regardless of instance count — this part already works as
  intended.
- **One real, minor inefficiency**: `mountainsImage()`
  (`examples/gallery/gallery_app.cpp:1109-1117`) constructs a fresh
  `std::vector<uint8_t>` — a ~200KB copy of the embedded byte array — on
  *every call*, before that copy is even used to compute the (identical)
  cache key. 15 copies on initial build, plus 4 more every single
  animation frame (`RotatingTransformRow::build()` calls `image_for()` 4×
  per frame). A sub-millisecond memcpy each, so not a measured bottleneck
  today, but pure waste — the cache key doesn't need the whole array
  copied to be computed, and a cache hit doesn't need it retained either.
  Fix would be gallery-app-level (cache the `std::vector` once at startup
  and pass a reference/span instead of reconstructing it per call), not a
  framework change.

Not started — recorded for a future caching-improvement pass, not because
of any current measured impact.

### Dirty-region tracking — skip `BackdropFilter`'s capture pass when nothing relevant changed (2026-07-02, done)

Follow-up to the GPU-cost backlog entry above. Rather than downsampling
(which only reduces per-frame cost, not frequency), added a framework-wide
but narrowly-scoped dirty-region mechanism so the capture+blur pre-pass
(`Renderer::rasterFrame()`, `src/ui/renderer.cpp:186-238`) only runs when
something within blur range of a `BackdropFilter` actually changed —
instead of unconditionally, every frame that rasterizes at all.

**Mechanism** (see `inc/campello_widgets/ui/dirty_region.hpp` for the
core decision function):
- `Renderer` accumulates a per-frame list of dirty rects (capped at 32,
  overflow falls back to "assume everything dirty" — safe, conservative)
  and a list of each `BackdropFilter`'s own bounds
  (`noteDirtyRegion()`/`noteBackdropFilter()`, both reset each frame in
  `layoutPass()`).
- `RenderObject::paint()` (`src/ui/render_object.cpp`) reports a node's
  bounds as dirty whenever it was actually marked dirty (`needs_paint_`)
  — required no changes to any `markNeedsPaint()` call site, since
  `offset` (confirmed global/absolute, traced through
  `RenderBox::paintChild()`'s offset accumulation) and `size_` were
  already in scope there.
- `OffsetLayer::maybeReplay()` (`src/ui/offset_layer.cpp`) — the one place
  `RenderRepaintBoundary` and the 4 self-boundaring scrollables funnel
  through, all of which override `paint()` and so bypass the hook above —
  centrally reports dirty bounds for all 3 cases that mean "content
  visibly changed": genuinely dirty (`needsPaint()==true`), a cheap
  delta-translate reposition, and a forced full re-record due to unsafe
  geometry. Only the backdrop-filter-forced-safety re-record (dirty or
  not, moved or not, `noteBackdropFilter()`'s side effect must still fire
  every frame — unchanged from the earlier scroll-offset fix) is
  correctly excluded from counting as dirty.
- `Renderer::buildFrame()` gates the capture, once, *after* the full paint
  walk completes (not mid-walk) — a `BackdropFilter` can sample content
  painted anywhere in the frame, including subtrees walked after it, so
  the decision needs the complete picture. Margin = 2.5×sigma capped at
  12px, matching the blur shader's own kernel radius
  (`shaders/metal/widgets.metal`'s `RADIUS = min(ceil(2.5*sigma), 12)`).

**Bug found and fixed during verification** (worth remembering): the
first version only added dirty-reporting to `OffsetLayer`'s *reposition*
branch, missing the case where a boundary/scrollable is directly dirty
(`needsPaint()==true`) — since those 5 classes override `paint()` and
never hit the base-class hook. This meant active scrolling could look
"nothing changed" to the gate. Fixed by centralizing all 3 report cases
inside `maybeReplay()` itself (see above) rather than scattering partial
logic across call sites.

**Verified via a temporary trace flag** (`DebugFlags::printDirtyRegionTrace`,
left in place per user request — toggle with `CW_TRACE_DIRTY=1` when
launching the gallery from a terminal, `CW_TRACE_RASTER=1` for the
existing `printRasterSubPhaseTimings`): confirmed the gallery's
Clipping & FX tab (fully static `BackdropFilter` demo) goes completely
idle — zero frames rendered at all — once settled, measured at 0% GPU.
The Images tab stays at ~40% because `RotatingTransformRow` (a
perpetually-looping rotation demo) sits in the same scrollable column as
that tab's `BackdropFilter`, so every frame is genuinely dirty nearby —
correct behavior, not a regression; the fix simply can't help when
something is actually animating in range.

**Known, accepted limitation**: the 32-rect cap overflows quickly on
busy/animated screens, since every ancestor along a dirty leaf's
propagation chain reports its own (largely redundant, superset) bounds —
not just the leaf. Not incorrect (falls back to the safe "assume dirty"
path), just means the optimization provides less benefit than it could on
screens with continuous animation elsewhere. Worth revisiting (e.g. only
reporting the deepest dirty node per propagation chain) if a future
screen needs the optimization to survive nearby animation — not needed
for the motivating case (a static `BackdropFilter` demo).

The downsampling idea above is still valid as a complementary,
independent optimization (reduces per-capture cost; this fix reduces
capture frequency) — not started.

### Widget/RenderObject over-invalidation — a chain of "always dirty regardless of change" bugs (2026-07-02, done)

Follow-up investigation after the dirty-region fix above didn't move the
gallery's Images-tab numbers at all — `RotatingTransformRow` (a
perpetually-looping rotation demo) kept the whole tab's paint/layout
state thrashing every frame regardless. Traced with new tracing tools
(`DebugFlags::printDirtyRegionTrace`, toggled via `CW_TRACE_DIRTY=1`;
`DebugFlags::printRasterSubPhaseTimings`, via `CW_TRACE_RASTER=1` — both
wired into `examples/gallery/macos/main.mm`, left in place) that revealed
a *chain* of distinct bugs, each masking the next once fixed:

1. **`markNeedsPaint()` bubbled unconditionally to the true root**
   (`src/ui/render_object.cpp`), unlike Flutter's, which stops at the
   nearest `isRepaintBoundary`. Added `RenderObject::isRepaintBoundary()`
   (default false, overridden true by `RenderRepaintBoundary` and the 4
   self-boundaring scrollables) and made `markNeedsPaint()` stop
   propagating there — request a frame either way via a new decoupled
   `Renderer::notePaintRequested()` latch (`buildFrame()` now consumes
   this instead of checking `root_->needsPaint()`, since a dirty leaf
   under a boundary no longer reaches root at all).
2. **`RenderObjectElement::update()`** (`src/widgets/render_object_element.cpp`)
   called `render_object_->markNeedsLayout()` **unconditionally after every
   single widget update**, completely bypassing every widget's own
   equality-guarded `updateRenderObject()` override (Transform, SizedBox,
   Flex, etc.) — found via a captured backtrace at the exact
   `markNeedsLayout()` call site. Removed the blanket call; audited all 32
   `updateRenderObject()` overrides in `src/widgets/` first (most already
   self-guard correctly; `PageView`/`GridView`/`ListView` were missing
   guards on genuinely layout-relevant fields — fixed; `TableView` calls
   `markNeedsLayout()` explicitly now since its span types have no
   `operator==`; `TreeView` was already safe via `invalidateRowCache()`).
3. **`RenderFlex::insertChild()`/`clearChildren()`** (`src/ui/render_flex.cpp`)
   — the *actual* dominant cost, found only after fixing #2 exposed it —
   called unconditionally on **every** rebuild via
   `FlexElement::syncChildRenderObjects()` (`src/widgets/flex.cpp`),
   regardless of whether the child list or flex factors changed, because
   `clearChildren()` destroys the previous state before any comparison is
   possible. Fixed by computing the new (index, box, flex-factor) list
   first in `FlexElement`, comparing against what was last actually synced
   (`FlexElement::last_synced_`, `inc/campello_widgets/widgets/flex_element.hpp`),
   and skipping `clearChildren()`+`insertChild()` entirely when identical.
   `StackElement`/`Positioned` likely has the same latent issue (not yet
   hit in practice, not fixed — same fix shape would apply if it ever is).
4. **`RotatingTransformRowState::build()`** (`examples/gallery/gallery_app.cpp`)
   reconstructed a fresh `ImageWidget` (via `mountainsImage()`) on every
   tick even though the image content never changes — only the ancestor
   `Transform`'s rotation angle does. Each fresh `std::vector<uint8_t>`
   construction from the embedded JPEG measured ~1.1-1.6ms on its own
   (confirmed via direct timing — this is the same "redundant copy" noted
   in the image-caching backlog entry above, previously assessed as
   sub-millisecond and dismissed; that assessment was wrong), ×4 images
   ×60/sec. Fixed by constructing the 4 image `WidgetRef`s once in
   `initState()` and reusing the same pointer every `build()` — lets
   `Element::updateChild()`'s identical-widget-pointer fast path
   (`src/widgets/element.cpp:193`) skip that entire subtree's
   reconciliation (including the full `ImageWidgetState` rebuild cascade)
   on every tick, since only the ancestor `Transform` actually changes.

**Combined measured effect** (gallery Images tab, active rotation, `darwin-debug`
build): GPU 40% → 9%, UI (build-phase) 10ms → 0.8ms, `buildScope()`
specifically ~9.5ms → ~0.2ms. Full universal suite stayed green (439/439)
throughout every step. Verified no regressions by clicking through Lists,
Controls, Animations, and Gestures tabs after the final fix.

**Reusable diagnostic tools added** (all gated behind existing/new
`DebugFlags`, safe to leave permanently):
- `CW_TRACE_DIRTY=1` — prints `RenderObject::paint()`/`OffsetLayer`
  dirty-region reports, `Renderer::buildFrame()`'s per-frame dirty-rect
  summary, and `markNeedsLayout()`'s TRUE_ORIGIN-vs-propagated trace
  (`src/ui/render_object.cpp` — distinguishes "this node was actually
  marked dirty externally" from "this node is just hearing about a
  descendant," via a `s_layout_propagation_depth` counter).
- `CW_TRACE_RASTER=1` — prints both raster sub-phase timings (existing)
  and new build-phase sub-phase timings (`buildScope`/`layoutPass`/
  `generateDrawList`, `src/ui/renderer.cpp`).
- `Element::rebuild()` (`src/widgets/element.cpp`) prints per-element
  wall-clock cost and `typeid` under `CW_TRACE_DIRTY`, zero overhead when
  disabled (branches to the untimed path).

### Bug: `BackdropFilter` inside a scroll showed mismatched blur (2026-07-02, user-reported, fixed)

Regression from the dirty-region tracking work above — user noticed the
Images tab's BackdropFilter demo showed the blur sampling from the wrong
part of the scrolled background ("parts of the background don't match")
while scrolling.

**Root cause**: the dirty-region system assumed `offset` (as threaded
through `paint()`/`performPaint()`) is always a widget's true on-screen
position. False for anything painted inside `RenderSingleChildScrollView`/
`RenderListView`/`RenderGridView`/`RenderPageView` — all four apply their
scroll offset via `canvas.translate()` in `performPaint()`, never by
adjusting `offset` itself (confirmed via `RenderSingleChildScrollView::
performPaint()`, `src/ui/render_single_child_scroll_view.cpp:109-123`).
So `RenderBackdropFilter::performPaint()`'s reported bounds (used only for
the capture-skip *decision* — the actual sample/quad geometry was already
correctly transform-deferred to flush time, confirmed by tracing
`MetalDrawBackend::drawBackdropFilter()`, `src/macos/metal_draw_backend.mm:1356-1408`,
which applies `transform` to `cmd.bounds`' corners) stayed frozen at its
*logical*, unscrolled position, while the scroll view's own dirty report
(`OffsetLayer::maybeReplay()`'s `dirty` branch) correctly used its true,
never-moves screen position. Two different coordinate spaces being
compared for intersection meant the check could silently miss, skipping
the capture on scroll frames — correct current-position sampling geometry,
stale captured content.

**Fix**: added `projectedBounds(transform, local_rect)`
(`inc/campello_widgets/ui/dirty_region.hpp`) — projects a logical rect's
four corners through `Canvas::currentTransform()` (which *does* include
ambient scroll translates and Transform-widget matrices, composed via
`PushTransformCmd`s during recording) and returns their axis-aligned
bounding box. Applied at all three dirty-region report sites:
`RenderObject::paint()`, `OffsetLayer::maybeReplay()`, and
`RenderBackdropFilter::performPaint()` (only for the `noteBackdropFilter()`
gating call — the `beginBackdropFilter()` draw command keeps the original
logical `bounds`, since that one is correctly transformed later at flush
time; projecting it here too would double-transform).

New regression tests in `tests/universal/test_dirty_region.cpp`
(`ProjectedBounds.*`) — including one that reconstructs the exact
coordinate-mismatch scenario (a filter's logical bounds vs. a scroll
view's true viewport) and asserts they only intersect once projected.
Full suite 442/442 throughout. Verified fixed by the user directly in the
gallery.

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
- [x] Vulkan backend readback (`vkCmdCopyImageToBuffer`) for Android/Linux
- [ ] DirectX 12 backend readback (`CopyTextureRegion` into readback heap) for Windows
