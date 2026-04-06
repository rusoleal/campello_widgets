# Changelog

All notable changes to campello_widgets will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.2.2] - 2026-04-06

### Added

- **Unity Build Support** — new `ENABLE_UNITY_BUILD` CMake option (default ON) for faster compilation; combines source files into batches of 16 to reduce compilation time
- **IME (Input Method Editor) Support on macOS** — full implementation for entering complex characters:
  - Accented characters: `´` + `e` → `é`, `` ` `` + `a` → `à`, `~` + `n` → `ñ`
  - CJK (Chinese/Japanese/Korean) input methods
  - Emoji picker support (Ctrl+Cmd+Space)
  - Visual feedback: composing text displayed with underline
  - `TextEditingController` extended with `isComposing()`, `composingStart()`, `composingEnd()`, `beginComposing()`, `updateComposingText()`, `commitComposing()`, `cancelComposing()`
  - `TextInputManager` — bridge between platform IME and widget system
  - macOS `CampelloMTKView` implements `NSTextInputClient` protocol with full method suite (`setMarkedText:`, `unmarkText`, `insertText:`, `hasMarkedText`, `markedRange`, `selectedRange`, `firstRectForCharacterRange:`)
- **Image Loading and Caching** — integrated `campello_image` dependency (v0.3.1) for cross-platform image loading:
  - `ImageProvider` — abstract image source interface
  - `ImageCache` — LRU cache for decoded images with configurable size limits
  - `ImageLoader` — asynchronous image loading with priority queue
  - `NetworkImage` — widget for displaying images from URLs with loading states
  - `ImageWidget` — widget for displaying loaded images with fit modes (cover, contain, fill)
  - `HTTPClient` — cross-platform HTTP client (macOS/iOS via `NSURLSession`, Windows via WinHTTP, Linux via libcurl, Android via JNI)
  - Support for JPEG, PNG, BMP, TGA, GIF, and WebP formats
- **New Examples**:
  - `macos_textfield` — TextField demo with IME composition showcase
  - `macos_image` — image loading demo with NetworkImage and local assets
  - `macos_gestures` — gesture detection demo with pan, scale, and tap gestures

### Fixed

- **Unity Build Compilation Errors** — fixed symbol conflicts when unity build is enabled:
  - `campello_gpu` — disabled unity build due to naming conflicts between `campello_gpu::Device`/`Buffer` and Apple's `MTL::Device`/`MTL::Buffer`
  - `campello_input` — disabled unity build due to Objective-C++ (`.mm`) files incompatible with C++ unity batches
  - `campello_widgets` — disabled unity build due to Objective-C++ platform files
  - `libwebp` (via `campello_image`) — disabled unity build for `sharpyuv`, `webpencode`, `webpdecode`, `webpdspdecode`, `webputilsdecode` targets due to static function name conflicts (`clip()`, `GetPSNR()`, `Shift()`)

### Changed

- **Dependencies** — added `campello_image` v0.3.1 for image loading capabilities

## [0.2.1] - 2026-04-04

### Fixed

- **macOS GPU scissor clipping** — the Metal draw backend was silently ignoring the `clip` parameter (`/*clip*/`) in every draw function (`drawRect`, `drawCircle`, `drawOval`, `drawRRect`, `drawLine`, `drawText`, `drawImage`, `drawBackdropFilter`), meaning no widget that relied on clipping produced correct visual output on macOS. All clipping widgets are now correctly scissored on the GPU:
  - `ClipRect`, `ClipRRect`, `ClipOval`, `ClipPath`
  - `ListView`, `GridView`, `SingleChildScrollView`, `PageView`
  - `TableView`, `TreeView`
  - `TextField` (cursor/content clipping)
  - `AnimatedSize` and any other widget using `canvas.clipRect()`
- **Metal crash on empty clip rect** — when a scrollable widget scrolled a child fully out of the viewport, the clip intersection produced a zero-sized `Rect`, which was passed to `MTLRenderCommandEncoder::setScissorRect` with `width=0`/`height=0`. Metal requires both dimensions ≥ 1; passing zero caused undefined behaviour and a crash on the GPU completion queue (`MTLIOAccelPooledResourceRelease`). Draw calls are now skipped entirely when the scissor rect is degenerate after viewport clamping.
- `MetalDrawBackend::setDevicePixelRatio(float)` added so the backend converts logical-point clip rects to physical pixels correctly on Retina displays

## [0.2.0] - 2026-04-03

### Added

- **Two-Dimensional Scrollables** — new widgets for complex data visualization:
  - `TableView` — scrollable table with rows and columns supporting:
    - Bidirectional scrolling (horizontal + vertical)
    - Pinned rows and columns that remain fixed during scroll
    - Lazy cell virtualization (only visible cells mounted)
    - Configurable row heights and column widths via `TableSpan`
    - Scroll wheel navigation with natural macOS direction
    - Aggressive clipping for optimal performance
  - `TreeView` — hierarchical tree display with:
    - Two-dimensional scrolling (vertical through rows, horizontal for deep nesting)
    - Expandable/collapsible nodes via `TreeController`
    - Lazy row building (only visible rows mounted)
    - Configurable indentation and row height
    - Animation support for expand/collapse transitions
  - `TreeNode` — immutable tree node structure for TreeView
  - `TreeController` — manages expansion state independently from node tree
  - `TableSpan` — configuration for row/column extents and pinning
- macOS TableView example (`examples/macos_table_view/`) — spreadsheet-like demo with 1000×26 cells, pinned header row and column
- macOS TreeView example (`examples/macos_tree_view/`) — file explorer-like demo with expandable folders
- Launch scripts: `run_macos_table_view.sh`, `run_macos_table_view_release.sh`, `run_macos_tree_view.sh`, `run_macos_tree_view_release.sh`

## [0.1.7] - 2026-04-03

### Added

- **Comprehensive Constructor Support** — 50+ widgets now have full constructors supporting the `mw<>()` pattern:
  - Layout: `ConstrainedBox`, `DecoratedBox`, `AspectRatio`, `FractionallySizedBox`, `ClipRRect`, `ClipOval`, `ClipPath`, `SingleChildScrollView`, `IntrinsicWidth`, `IntrinsicHeight`, `Wrap`
  - Interactive: `Button`, `GestureDetector`, `ListTile`, `MouseRegion`, `Tooltip`
  - Forms: `Checkbox`, `Switch`, `Slider`, `TextField`
  - Display: `Divider`, `CircularProgressIndicator`, `LinearProgressIndicator`
  - Effects: `ShaderMask`, `FractionalTranslation`
  - Animated: `AnimatedContainer`, `AnimatedOpacity`, `AnimatedAlign`, `AnimatedPositioned`, `AnimatedSize`, `AnimatedSwitcher`, `AnimatedBuilder`
  - Transitions: `FadeTransition`, `ScaleTransition`, `SlideTransition`, `RotationTransition`
  - Navigation: `PageView`, `DefaultTabController`, `TabBar`, `TabBarView`
  - Menus: `DropdownButton`, `PopupMenuButton`
  - Builders: `LayoutBuilder`, `FutureBuilder`, `StreamBuilder`, `ValueListenableBuilder`
  - Drag & Drop: `Draggable`, `DragTarget`
- `Container` full constructor `(w, h, color, padding, alignment, child)` for `mw<>()` convenience
- `Positioned` full constructor `(l, t, r, b, w, h, child)` for `mw<>()` convenience
- `Expanded` constructors: `Expanded()`, `Expanded(child)`, `Expanded(flex, child)`
- `Text` default constructor for `mw<>()` convenience
- macOS PlatformMenu test example (`examples/macos_menu_test/`) — demonstrates standard menu bar with File, Edit, View, Format, Window, Help menus
- macOS PlatformMenu integration tests (`tests/platform/test_macos_platform_menu.mm`) — comprehensive test suite for native menu bar functionality

### Fixed

- **macOS PlatformMenu crash** — fixed use-after-free when AppKit's async `_NSMenuShortcutUpdater` accesses menu items after the menu bar is replaced; all menu objects are now intentionally retained to prevent crashes

## [0.1.6] - 2026-04-02

### Added

- `KeyboardListener` widget — observes keyboard events via a `FocusNode` without consuming them; provides `on_key_event` callback for `KeyEvent` data
- macOS keyboard example (`examples/macos_keyboard/`) — interactive demo showing key presses, event kinds (down/up/repeat), modifiers, and typed text accumulation

### Changed

- **Breaking**: renamed `make<T>()` helper to `mw<T>()` (shorter alias for `std::make_shared<T>`); all examples updated

## [0.1.5] - 2026-04-01

### Added

- **Phase 14 — Logical Pixels**: all layout, input, and rendering now operate in device-independent logical pixels; DPR is applied only at the GPU boundary
  - `Renderer::setDevicePixelRatio(float)` / `device_pixel_ratio` field; `layoutPass()` divides viewport dimensions by DPR before building `BoxConstraints`
  - `RenderObject::activeDevicePixelRatio()` / `setActiveDevicePixelRatio(float)` static — set by `Renderer` around layout/paint passes so render objects can scale rasterised assets
  - `RenderText` and `RenderParagraph` multiply `font_size` by DPR at paint time for physical-resolution text
  - Platform adapters wired: `backingScaleFactor` (macOS), `contentScaleFactor` (iOS), `GetDpiForWindow/96` (Windows); DPR updated on display-change events
  - Pointer coordinates converted to logical pixels in all platform adapters (removed `* backingScaleFactor` / `* contentScaleFactor` multiplications); scroll deltas adjusted on Windows
  - Safe area insets stored in `Renderer::view_insets_` are now in logical pixels (removed `* scale` from macOS and iOS adapters)
- `MediaQueryData` struct (`logical_size`, `device_pixel_ratio`, `padding`, `view_insets`, `physicalSize()`) and `MediaQuery` `InheritedWidget` injected above the root widget by `Renderer`; `MediaQuery::of(BuildContext&)` static accessor
- `SystemMouseCursor` enum (`arrow`, `pointer`, `text`, `forbidden`, `resize_ns`, `resize_ew`); `registerCursorHandler()` / `setSystemCursor()` / `resetSystemCursor()` global API; macOS platform adapter wires `NSCursor` shapes
- `MouseRegion` extended with `cursor` field (`SystemMouseCursor`) — sets the system cursor on enter and resets it on exit
- `Card` — `StatelessWidget` wrapping `DecoratedBox` with configurable elevation shadow, border radius, and clip behaviour
- `ListTile` — `StatelessWidget` with `leading`, `title`, `subtitle`, and `trailing` slots; tap callback via `GestureDetector`
- `FloatingActionButton` — circular `StatelessWidget` with icon, background colour, elevation shadow, and `on_pressed` callback
- `SnackBar` — `Overlay`-based bottom notification bar with auto-dismiss driven by `AnimationController`; `showSnackBar()` / `hideSnackBar()` free functions
- `PopupMenuButton` — `StatefulWidget` that opens an `Overlay` popup menu above/below the anchor; `ModalBarrier` for tap-outside dismissal; typed `PopupMenuItem<T>` entries
- `DropdownButton<T>` — template `StatefulWidget` (header-only) that opens an `Overlay` dropdown with typed items; selected-value display and `on_changed` callback
- `DefaultTabController` + `TabScope` (`InheritedWidget`) + `TabBar` + `TabBarView` — complete tab navigation system with animated indicator and coordinated scrolling
- `PageView` + `PageController` + `RenderPageView` — horizontally swipeable pages with snap physics and programmatic `animateToPage()` / `jumpToPage()`
- 14 new unit tests in `test_logical_pixels.cpp` covering `RenderObject::activeDevicePixelRatio`, `MediaQueryData` equality / `physicalSize`, `MediaQuery` widget `updateShouldNotify`, pointer-event logical coordinates, and `EdgeInsets` helpers

### Changed

- All four macOS examples updated to use logical-pixel dimensions after the DPR switch
- Phase 14 (Logical Pixels) marked complete in `TODO.md`

## [0.1.4] - 2026-03-30

### Added

- `TextField` widget + `TextEditingController`: full single-line text input with cursor, selection, placeholder, obscure-text mode, focus integration, and callbacks (`on_changed`, `on_submitted`)
- `Draggable<T>` widget: type-safe drag source with feedback overlay (position-tracked via `ValueNotifier<Offset>`), `child_when_dragging`, and `on_drag_started`/`on_drag_ended` callbacks
- `DragTarget<T>` widget: type-safe drop zone with `on_will_accept`, `on_accept`, and hover-aware `builder`
- `DragManager`: global singleton coordinating drag sessions; handles target registration, enter/exit callbacks, and type-checked acceptance
- 65 new unit tests across three suites: `TextEditingController` (25), `RenderTextField` (19), `DragManager` (13), and `Draggable`/`DragTarget` widget integration (8)

### Changed

- `campello_gpu` dependency upgraded from v0.4.1 to v0.5.0 (official GitHub tag)
- Phase 13 (Advanced Widgets) marked complete in `TODO.md`
- Hot-reload marked as not planned in `TODO.md`

## [0.1.3] - 2026-03-29

### Added

- `run_fidelity_tests.bat`: Windows batch script replicating `run_fidelity_tests.sh`; supports `--skip-flutter`, `--skip-build`, `--visual`, `--json`, `--all`, `--test <name>`, and `--verbose` flags

### Changed

- Testing headers (`fidelity.hpp`, `gpu_visual_renderer.hpp`, `visual_fidelity.hpp`) moved from `inc/campello_widgets/testing/` to `src/testing/` — they are internal test infrastructure, not part of the public API; `tests/CMakeLists.txt` updated to add `src/testing` as a private include path

### Fixed

- MSVC build (`curves.hpp`): replaced `_USE_MATH_DEFINES` + `M_PI` with `std::numbers::pi` (`<numbers>`) to avoid include-order sensitivity
- MSVC build (`fidelity.cpp`): guarded `<cxxabi.h>` behind `#ifndef _MSC_VER`; added MSVC-compatible `demangleTypeName` using `typeid().name()` with keyword-prefix stripping
- MSVC build (`d3d_draw_backend.cpp`): corrected `Matrix4` field access (`data` not `m`); fixed `setPipeline` call to pass `shared_ptr` by value rather than dereferencing it
- Windows shared library (`windows.cmake`): added `WINDOWS_EXPORT_ALL_SYMBOLS ON` so MSVC generates the `.lib` import library required by the test linker
- Windows test discovery (`tests/CMakeLists.txt`): switched `gtest_discover_tests` to `DISCOVERY_MODE PRE_TEST` and added a post-build step to copy `campello_widgets.dll` and `campello_gpu.dll` next to the test executable, preventing `0xc0000135` load failures at discovery time

## [0.1.2] - 2026-03-28

### Added

- `Transform` widget and `RenderTransform` RenderBox: apply a `Matrix4` transform (rotate, scale, translate) to a child widget; pivot controlled by `Alignment`; layout-transparent (transform affects painting only)
- Factory helpers on `Transform`: `Transform::rotate()`, `Transform::scale()`, `Transform::translate()`; static matrix builders `rotation()`, `scaling()`, `translation()` mirrored on both `Transform` and `RenderTransform`
- `GpuVisualRenderer`: headless Metal-backed offscreen renderer for visual fidelity tests; renders a `DrawList` to an RGBA8 texture and exports PNG; falls back gracefully when no GPU is available (CI); stub implementation for non-Metal platforms
- Visual fidelity test infrastructure: `test_visual_fidelity.cpp` and `test_fidelity.cpp` extended with GPU-rendered golden comparisons; `flutter_fidelity_tester` Flutter app generates reference goldens

## [0.1.1] - 2026-03-28

### Changed

- Updated `.gitignore` to exclude fidelity test autogenerated files:
  - Flutter JSON golden files (`tests/goldens/*_flutter.json`)
  - Visual fidelity output directories (`tests/visual_fidelity/flutter_goldens/`, `cpp_output/`, `diffs/`)
  - Flutter tool cache (`flutter_fidelity_tester/.dart_tool/`, `build/`)
  - `flutter_fidelity_tester/pubspec.lock`

## [0.1.0] - 2026-03-22

### Added

- Project scaffolding: CMake build system (C++20), platform dispatchers for macOS, iOS, Android, Windows, and Linux; `FetchContent` wrappers for `campello_gpu` (v0.3.7), `campello_input`, `vector_math`, and GoogleTest
- Core widget infrastructure: `Widget`, `WidgetRef`, `BuildContext`, `StatelessWidget`, `StatefulWidget`/`State<T>`/`StateBase`, `Element`, `StatelessElement`, `StatefulElement`, `RenderObjectElement`, `SingleChildRenderObjectElement`, `MultiChildRenderObjectElement`; full widget-tree reconciliation
- Layout system: `BoxConstraints`, `Size`, `Offset`, `EdgeInsets`, `RenderObject`, `RenderBox`; constraints-down / sizes-up layout protocol
- Rendering pipeline: `PaintContext`, `Canvas`, `DrawCommand` queue, dirty-region tracking, layer compositing, clip/transform stacks, frame loop via `Renderer`; premultiplied-alpha blend; `Canvas::setOpacity()` bakes opacity multiplicatively into draw commands
- Basic render widgets: `RawRectangle`, `RawText`, `RawImage`, `RawCustomPaint` / `CustomPainter`
- Composited widgets: `SizedBox`, `Padding`, `Align`, `Center`, `Container`, `Row`, `Column`, `Stack`/`Positioned`, `Text`, `Image`, `ColoredBox`, `Scaffold`; `Flex`/`Expanded`/`Flexible` with `MainAxisAlignment` and `CrossAxisAlignment`
- Opacity compositing: `RenderOpacity`, `Opacity`, `AnimatedOpacity`
- Input handling: `PointerEvent`, `PointerDispatcher` (hit-test on down, pointer capture, scroll), `HitTestResult`/`HitTestEntry` on `RenderBox`/`RenderFlex`/`RenderStack`; `GestureDetector` with tap, double-tap, long-press, pan, and scroll recognizers; `FocusNode`, `FocusManager` (tab traversal, key routing), `Focus` widget, `RenderFocus`, `KeyEvent`/`KeyCode`
- Platform input wiring: macOS `CampelloMTKView` (mouse, scroll, keyboard); iOS UIKit touch via `UITouch*` identity map; Android `GameActivity` touch via pointer ID
- Animation system: `TickerScheduler`, `AnimationController`, `Tween<T>` (float, double, `Color`, `Offset`, `Size`), `CurvedAnimation`, `Curves`, `AnimatedBuilder`, `AnimatedContainer`, `AnimatedOpacity`
- Scrolling: `ScrollController`, `SingleChildScrollView`, `ListView` (virtualised), `GridView`, scroll physics (momentum, bounce, clamped)
- Unit tests: `BoxConstraints`, `RenderAlign`, `RenderFlex`, `RenderListView`, `RenderPadding`, `RenderSizedBox`
- Example applications: Hello World, Counter (StatefulWidget), ListView, Animated transitions (macOS)
- Build and run scripts for macOS (Debug and Release); `test.sh` for universal and integration test runs
