# campello_widgets — Development Plan

Phases are ordered by dependency. Complete each phase before starting the next.
Items within a phase can be parallelised where noted.

---

## Phase 1 — Project Scaffolding

- [x] Set up CMakeLists.txt with C++20 standard
- [x] Add `campello_gpu` as a CMake dependency
- [x] Add `campello_input` as a CMake dependency
- [x] Define directory layout (`inc/`, `src/`, `tests/`, `examples/`, `dependencies/`)
- [ ] Set up CI pipeline (Windows, macOS, Linux builds)
- [ ] Add platform detection macros / abstractions

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

- [ ] Window / surface creation per platform (delegate to `campello_gpu`)
- [x] iOS: UIKit integration with safe area insets
- [x] Android: ANativeWindow integration with safe area insets
- [x] macOS: NSView / CAMetalLayer integration with safe area insets
- [x] Windows: HWND / DXGI integration
- [ ] Linux: XCB / Wayland integration
- [ ] Platform channel / FFI abstraction for native calls

---

## Phase 11 — Developer Experience

- [x] Debug overlay (FPS counter, paint size, repaint rainbow, debug banner)
- [ ] Hot-reload friendly design (documented rebuild contract)
- [x] Comprehensive unit tests for layout engine
- [x] Integration test harness (headless rendering) — `GpuVisualRenderer` (Metal, offscreen) with CPU fallback
- [ ] Example applications:
  - [x] Hello World
  - [x] Counter app (StatefulWidget demo)
  - [x] List view
  - [x] Animated transitions

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

## Backlog / Future

- Theme system (colours, typography, spacing tokens)
- Accessibility (semantic tree, screen reader support)
- Internationalisation (text direction, locale)
- [x] Rich text / inline spans
- [x] Dialog / overlay / modal system
- Drag-and-drop

---

## Real GPU Rasterization for Visual Fidelity Tests

**Resolved in v0.1.2.** `campello_gpu` v0.4.1 implemented `copyTextureToBuffer()` (Metal backend), closing
the GPU→CPU readback pipeline. `GpuVisualRenderer` (`src/testing/gpu_visual_renderer.mm`) now provides a
headless Metal renderer that renders a `DrawList` to an offscreen RGBA8 texture and exports PNG.
`VisualRenderer` is kept as a CPU fallback for CI environments without a GPU.

Remaining work for full cross-platform coverage:
- Vulkan backend readback (`vkCmdCopyImageToBuffer`) for Android/Linux
- DirectX 12 backend readback (`CopyTextureRegion` into readback heap) for Windows
