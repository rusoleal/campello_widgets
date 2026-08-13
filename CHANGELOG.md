# Changelog

All notable changes to campello_widgets will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.7.0] - 2026-08-13

### Added

- **Cross-platform `PlatformMenuBar` support for Windows/Linux.** Previously macOS-only (native `NSMenu`). Windows and Linux have no equivalent native menu-bar concept reachable without adopting a whole extra toolkit, so on those platforms `PlatformMenuDelegate` now reports `needsInWindowMenuBar() = true`, and a new `PlatformMenuBarView` widget renders the menu bar itself from the same `PlatformMenu`/`PlatformMenuItem` data, including keyboard accelerators. The horizontal top-level bar is a `ListView` (scrolls instead of clipping when there isn't enough width); each open dropdown's items are a `Row` of {label, `Expanded` spacer, shortcut caption} sized from actually-measured text. `FocusManager` gained a `globalKeyHandler()` getter so `PlatformMenuBarView` can chain onto any existing global key handler instead of clobbering it. The gallery example wires this up as a real demo: a "View" menu with Ctrl+1..9,0 shortcuts jumping to each tab.
- **`runApp()` overload accepting an explicit Linux `app_id`** (`inc/campello_widgets/linux/run_app.hpp`), plus a `defaultAppIdFromTitle()` fallback for callers that don't supply one. Sets `WM_CLASS` on X11 and `xdg_toplevel`'s `app_id` (via `libdecor_frame_set_app_id()`) on Wayland — what desktop shells use to match the running window to an installed `.desktop` file for the taskbar/alt-tab icon and name.
- Phase 12b: filled out `IDrawBackend` across Metal/Vulkan/D3D12 (`drawArc`, `drawPath`, `drawPoints`, `saveLayer`, `drawShaderMaskComposite`), sharing path flattening/tessellation via `src/gpu/path_tessellation.*`. Adds Vulkan line/shader_mask shaders and a DX12 `shader_mask.hlsl`. Refactors `GpuVisualRenderer` into a backend-agnostic core plus per-platform factories, and adds a Vulkan offscreen-readback path for headless GPU visual tests on Linux/Android.

### Fixed

- **[Linux] Cursor got stuck on whatever shape the window manager/compositor last set (typically a resize icon) once the pointer entered the window.** `setSystemCursor()` had no registered handler on Linux, so every `MouseRegion`/`TextField`/etc. cursor-shape change was a silent no-op. X11 now creates the standard cursor set via `XCreateFontCursor()` and wires `registerCursorHandler()` to `XDefineCursor()`. Wayland loads the system cursor theme via `wl_cursor_theme_load()` and wires it to `wl_pointer_set_cursor()` on a dedicated cursor surface, reclaiming the cursor immediately on `pointer_enter()` rather than waiting for the first move (libdecor manages its own cursor for the window decoration, and Wayland has no automatic reset crossing into the content surface). Needs the new `wayland-cursor` pkg-config dependency.
- **Two `ImageWidget`s pointing at the same image source, mounted in the same frame, triggered two independent decodes and two separate GPU textures for what should be one cached image** — found while chasing a Vulkan validation error under the gallery example; the redundant second `Texture` was what actually tripped it (see `campello_gpu`'s changelog for that half of the fix). `ImageLoader::loadAsync()` now checks an in-flight map before queuing a new decode task; a second caller for the same cache key gets the same `std::shared_future` the first caller holds. `loadAsync()`'s return type (and `ImageWidgetState::load_future_`) changed from `std::future` to `std::shared_future` since a plain `std::future` can't be shared across multiple waiters.
- **[Linux] `Renderer`/`VulkanDrawBackend` GPU resource caches (bind groups, textures, pipelines, samplers) could be destroyed on shutdown while the GPU was still executing the last submitted frame's command buffer** — `Device::submit()` is pipelined and doesn't block, so this reproduced 100% of the time under Vulkan validation layers on every clean shutdown. Fixed by having `Renderer::~Renderer()`/`VulkanDrawBackend::~VulkanDrawBackend()` call the existing `Device::waitForIdle()` before their own cache members tear down, mirroring how `Device::~Device()` already protects its own teardown.
- **Real `BoxFit`/clip rendering bug**: offscreen-composited children (`ClipRRect`/`ClipOval`/`ShaderMask`/`SaveLayer`) replayed nested clip commands with absolute canvas-space geometry, clipping content entirely outside the small offscreen child texture. Shared `translateChildCommands()` now corrects nested clip geometry for the synthetic offset. Verified against Flutter goldens for all 7 `BoxFit` modes.
- **Scroll/resize paint-cache performance**: `PictureLayer`'s "unsafe geometry" classification was overly broad, forcing a full re-record (and uncached GPU offscreen re-render) of any clipped/masked/shadowed content on every reposition — the common case during scroll and window resize. Narrowed to the two commands that actually need it; `OffsetLayer::maybeReplay()` now shifts clip-rect geometry by hand instead, unlocking GPU-side cache reuse. `RenderListView`/`GridView`/`SingleChildScrollView`/`TableView`/`TreeView` now self-boundary their own paint output via `OffsetLayer` so this applies to scrollable viewports directly, not just `RenderRepaintBoundary`.
- **[Linux/Wayland] Missing `VelocityTracker`-based wheel momentum handoff**, and a `frame_done()` callback bug starving the `poll()`-timeout tick.
- **Linux windows rendered at the wrong resolution on HiDPI displays** — neither the X11 nor the Wayland backend ever detected the display's scale factor: X11 hardcoded `device_pixel_ratio = 1.0f` and sized its window in raw (logical) pixels instead of physical ones; Wayland did the same and never called `wl_surface_set_buffer_scale()`. On a 2x display this produced a window rendered at half the intended physical resolution, visibly blurry or (under XWayland compositor scaling) undersized. Fixed in `src/linux/run_app.cpp` (new `getX11DisplayScale()`, reading `Xft.dpi` with a screen-mm fallback) and `src/linux/wayland_runner.cpp` (`wl_output`/`wl_surface.enter`-based scale discovery, `wl_surface_set_buffer_scale()`), both now sizing the window/swapchain in physical pixels. A second, related bug: even after physical sizing was correct, `Renderer::device_pixel_ratio_` — a separate member from `MediaQueryData`'s copy, and the one `buildFrame()` actually divides the physical viewport by to compute logical layout constraints — was never set on Linux, so layout still sized every fixed-size widget against the full physical viewport as if DPR were 1, shrinking all UI to roughly half its intended on-screen size. Fixed by calling `Renderer::setDevicePixelRatio()` after renderer construction in both backends. Verified on a live 2x-scale display (X11: exact physical-pixel window-size match via `xwininfo`; Wayland: `wl_surface.set_buffer_scale(2)` and correct swapchain size confirmed via `WAYLAND_DEBUG=1`; both: correct on-screen widget/text proportions).

- **Rotated/scaled images in the gallery's Images tab intermittently rendered another draw call's texture (e.g. glyph/text content) instead of the image** — `VulkanDrawBackend::drawTexturedQuad()` called `encoder.setBindGroup(0, bind_group)` *before* `encoder.setPipeline(...)`, the only place in the file to do so (every other draw method, e.g. `drawClipShapeComposite()`, correctly binds the pipeline first). The bind-group call was validated against whatever pipeline layout happened to still be bound from the *previous*, unrelated draw call in the pass (frequently a text/glyph draw) rather than the quad pipeline about to be selected — usually incompatible, which left the actual draw with no valid descriptor set 0 bound and made it sample whatever texture happened to still be resident in that binding slot. Confirmed via Vulkan validation layers (`VUID-vkCmdBindDescriptorSets-firstSet-00360`, `VUID-vkCmdDraw-None-08600`) and fixed by moving `setBindGroup()` after `setPipeline()` in both the axis-aligned and perspective/rotated code paths (`src/gpu/vulkan/vulkan_draw_backend.cpp`).

- **The gallery's Draw tab produced a broken, dotted stroke instead of a solid line, and content wiped by the Clear button could resurface on the next stroke** — traced to `campello_gpu`'s Vulkan offscreen-pass barrier (see `campello_gpu`'s own changelog for the fix); this repo's `dependencies/campello_gpu.cmake` already points at the local checkout, so no version bump was needed here. Verified with a solid, continuous stroke and a clean Clear-then-redraw cycle on both Linux (X11 and Wayland) and Android (Samsung Galaxy Tab S7 FE).

- **[Android] A stylus tap on a button inside a scrollable (e.g. the Animations tab's Swap/Next corner/Play buttons, all inside a `SingleChildScrollView`) was almost never recognized as a tap** — `gesture_constants.hpp`'s `isPrecisePointer()` grouped `stylus`/`invertedStylus`/`trackpad` together with `mouse` as "precise pointers," giving them a 1px pan-slop threshold instead of touch's 36px. A physical stylus pressing on glass inherently drifts a few pixels (the tip pivots as pressure is applied, unlike a frictionless mouse cursor decoupled from its click), so the ancestor scrollable's pan recognizer won the gesture arena over the button's tap on nearly every real stylus tap, silently consuming it as an imperceptible scroll. Fixed by restricting `isPrecisePointer()` to `mouse` only, matching Flutter's actual `computeHitSlop()`/`computePanSlop()` (`gestures/constants.dart`), which treats stylus like touch for this purpose despite the previous code's doc comment claiming otherwise. Verified on a Galaxy Tab S7 FE with an S Pen.

- **[Android] A hovering stylus (e.g. S Pen proximity sensing) never triggered `MouseRegion::on_enter`/`on_exit`** (sidebar nav-item hover highlighting worked with a desktop mouse but not the pen) — `handleAndroidInputEvent()` (`src/android/run_app.cpp`) had no case for `AMOTION_EVENT_ACTION_HOVER_ENTER`/`HOVER_MOVE`/`HOVER_EXIT`, the distinct action codes Android reports for stylus proximity (not touching), separate from `ACTION_MOVE`; they silently fell into the `default: break;` case. Now dispatches `HOVER_ENTER`/`HOVER_MOVE` as `PointerEventKind::move` with `pressure=0.0f` (matching the documented "0.0 for hover" convention already established for desktop mouse-move-without-press), reusing the same hover-diff mechanism in `PointerDispatcher` that already drives desktop mouse hover. `HOVER_EXIT` dispatches one synthetic off-screen move so the last-hovered item's `on_exit` fires correctly instead of leaving it stuck highlighted when the pen leaves proximity range entirely. Verified on a Galaxy Tab S7 FE with an S Pen.

### Known Issues

- **[Linux/Vulkan] Two known, not-yet-fixed issues in the pinned `campello_gpu` dependency** — see that repo's own changelog (v0.23.0) for detail. Neither is new in this release, both were just found while verifying it: (1) a swapchain acquire→first-write synchronization-validation hazard, not visibly corrupting a frame on hardware tested so far; (2) `Device::createShaderModule()`/pipeline creation crashes (`SIGSEGV`) on empty/invalid shader bytecode instead of returning `nullptr` gracefully — not reachable through any campello_widgets code path today (this library only ever compiles its own known-good precompiled shaders), but worth knowing about before adding a code path that loads shader bytecode from an external/untrusted source.

## [0.6.0] - 2026-08-05

### Added

- **`EmbeddedApp`** (`inc/campello_widgets/linux/embedded_app.hpp`, `src/linux/embedded_app.cpp`) — a headless entry point for hosting a widget tree without an owned window, surface, or event loop, for embedding this library inside a host that already owns its GPU device and its own render loop (the motivating case: a Wayland compositor drawing a dashboard/overlay on top of other content it composites itself). Takes an existing `Device` + root widget + initial size; `renderFrame(target, w, h)` renders into whatever `TextureView` the host chooses, not necessarily a swapchain image. `pointerDispatcher()`/`focusManager()` are exposed so the host forwards its own input events; `tick()`/`forceRefresh()` let the host keep animations advancing or force a redraw on frames it doesn't otherwise draw. No new capability was needed in `Renderer`/`VulkanDrawBackend` — both were already window-agnostic; this just adds the missing "don't create a window at all" entry point, generalizing the pattern macOS's `runApp(device, ...)` overload already documents for device sharing.

### Changed

- **`campello_gpu` dependency bumped `v0.21.0` → `v0.22.0`** (`dependencies/campello_gpu.cmake`) — brings in `Device::createTextureFromDmaBuf()`/`getSupportedDmaBufModifiers()`/`getDrmDeviceNode()` (Linux/Vulkan dma-buf import, the primitive `campello_native`'s compositor needs) and a portable-build fix (`<cstdint>` was missing in `begin_render_pass_descriptor.hpp`, latent on GCC 13, broke on GCC 16). See `campello_gpu`'s own `CHANGELOG.md` (v0.21.1/v0.22.0) for the full list, including an unrelated Metal ray-tracing bind-group crash fix.

### Fixed

- `docker/Dockerfile` was missing `libdecor-0-dev`, which the file's own stated purpose ("reproducing the Linux CI environment locally") requires — the real CI workflow already installs it. Drift between the two; found because it broke a from-scratch Docker build.

### Tests

- **`tests/platform/test_embedded_app.cpp`** — `EmbeddedApp` shipped with zero coverage originally; this closes that gap with 4 real integration tests (require `BUILD_INTEGRATION_TESTS`, a real GPU — `Device::createDefaultDevice(nullptr)` is headless, so no display needed, runs fine against CI's own lavapipe environment): first-frame-always-draws / idle-frame-draws-nothing (mirrors `Renderer::buildFrame()`'s `std::nullopt`-on-no-change contract), `forceRefresh()` overriding that, tick/input-forwarding don't crash, and — the one that actually matters most — a real pixel readback confirming a red `ColoredBox` root actually lands as red pixels in the caller-provided texture, not just "the call didn't crash." Verified passing directly (`docker/Dockerfile`'s CI-repro container): 4/4 pass, and the full universal suite (501 tests) still passes with the `campello_gpu` bump above (0 failures).

## [0.5.0] - 2026-07-30

### Added

- **New gallery "Draw" tab — freehand canvas widget** (`RenderDrawSurface`/`DrawSurface`, `inc/campello_widgets/ui/render_draw_surface.hpp`, `inc/campello_widgets/widgets/draw_surface.hpp`) — a persistent, incrementally-updated GPU texture rather than replaying the full stroke history every frame: only the segment drawn since the last paint is submitted each time, via a new `DrawSurfaceUpdateBeginCmd`/`EndCmd` draw-command bracket (`inc/campello_widgets/ui/draw_command.hpp`) and `Renderer::applyDrawSurfaceUpdate()`. `IDrawBackend::beginOffscreenPass()` gained a `preserve_content` parameter (LOAD instead of CLEAR) to support this, implemented on Metal/Vulkan/D3D12. Strokes are stamped `drawCircle` calls rather than `drawLine`, since Vulkan doesn't implement the latter. Stroke width responds to pointer pressure (mouse/finger report a constant 1.0, so it degrades gracefully without a stylus). Resizing the canvas blits the previous texture into the new one (`CommandEncoder::copyTextureToTexture`, via a new `blit_source` on the update command) rather than clearing it, so the drawing crops/extends like a real drawing app instead of vanishing. `IDrawBackend::createDedicatedOffscreenTexture()`/`createOffscreenTexture()` on all three backends now include `TextureUsage::copyDst` (previously only `copySrc`) so a texture can serve as a blit destination.
- **Stylus/pencil input** — `PointerEvent` gains `tilt`/`tilt_orientation` fields alongside the existing `pressure`; `PointerDeviceKind::stylus`/`invertedStylus` are now actually populated by both platform bridges instead of never being set. iOS (`src/ios/run_app.mm`) sources pressure from `UITouch.force`/`maximumPossibleForce`, device kind from `touch.type == UITouchTypePencil`, tilt from `altitudeAngle`, orientation from `azimuthAngleInView:`. Android (`src/android/run_app.cpp`) sources from `AMotionEvent_getPressure()`/`getToolType()` (`AMOTION_EVENT_TOOL_TYPE_STYLUS`/`_ERASER`/`_MOUSE`)/`getAxisValue(..., AXIS_TILT/_ORIENTATION, ...)`. New `isPrecisePointer()` (`gesture_constants.hpp`) extends the existing mouse/trackpad "precise pointer" gesture-slop treatment to stylus input too. Verified live on a Galaxy Tab S7 FE with an S Pen (stylus and finger both checked); iOS side is code-complete but not yet verified on real hardware.
- **iOS platform integration, verified on real device and Simulator** — `src/ios/run_app.mm` gains hardware-keyboard support (`UIKeyboardHIDUsage`→`KeyCode` mapping via `pressesBegan:/pressesEnded:/pressesCancelled:`, modifier mapping mirroring macOS's `keyDown:`), suppression of the on-screen keyboard/predictive-text bar (`-inputView`/`-inputAccessoryView`/`-conformsToProtocol:` overrides that return empty views / deny `UITextInput` conformance while nothing is focused), and safe-area-inset-aware touch coordinates for notched devices. Switched from on-demand (`enableSetNeedsDisplay`) to `MTKView`'s continuous `CADisplayLink`-paced rendering — the on-demand path was found (reproduced on both Simulator and device) to fully starve `hitTest:`/touch delivery once any continuous `AnimationController` is running. Also fixes viewport dimensions passed to the renderer (logical points → physical pixels, matching macOS; the old mismatch collapsed most scissor rects to empty). `build_metal_shaders.sh` now compiles three Metal shader variants (macOS / iOS device / iOS simulator — Metal bytecode is target-triple-specific) via `xcrun -sdk {macosx,iphoneos,iphonesimulator}`, all embedded into `src/shaders/metal_widgets.h` and selected at compile time via `TargetConditionals.h`. New `examples/gallery/ios/run.sh` builds+installs+launches on a connected physical device (`xcrun devicectl`) or falls back to Simulator (`xcrun simctl`); `examples/gallery/ios/CMakeLists.txt` gained a `POST_BUILD` step that copies and re-signs `campello_gpu`/`campello_image` dylibs into the app bundle's `Frameworks/` directory, required for on-device (sandboxed) launch.
- **Box shadows are now actually rendered** — `DrawShadowCmd` was previously a documented no-op ("planned for a later phase"). `Renderer::applyBoxShadow()` fills the shadow shape into a padded offscreen texture, runs the existing two-pass Gaussian blur, and composites it back, mirroring `applyClipShape()`'s structure and GPU-cached via a new `shadow_gpu_cache_`. New `Path::simpleRRectShape()` tracks whether a path is exactly one `addRect()`/`addRRect()` call so the shadow code can recover the correct corner radius without a geometric round-trip.
- **`RenderClipRRect` and shadow-bearing `RenderDecoratedBox` self-promote to repaint boundaries** (`isRepaintBoundary()` → true, own `OffsetLayer`), the same automatic-promotion pattern already applied to scrollables in 0.4.0 — no longer need a manual `RepaintBoundary` wrapper to avoid re-running an expensive offscreen composite/blur every frame under an animating ancestor. `RenderDecoratedBox` only pays for this when `decoration.box_shadow` is non-empty.
- **Real FPS counter in the performance overlay** — `build_sampler_`/`raster_sampler_` only ever answered "how expensive was this phase", not "how often does a frame actually reach the screen", so a frame could look cheap on both and still not be presented on time. New `present_fps_sampler_` (`Renderer`) measures the wall-clock cadence between successive `rasterFrame()` completions and shows it in the overlay label (`UI: … RASTER: … FPS: …`). Since this is an on-demand renderer (frames only happen when something requests one), the first frame after being idle for a while is naturally far apart from the last one — feeding that gap straight into the sampler would register as one nonsensical "instant fps" sample (e.g. ~0.3fps after a 3s idle gap) and drag the rolling average down for a while right as a fresh animation starts. New static `Renderer::recordPresentSample()` resets the sampler instead of recording across any gap wider than 200ms (comfortably above one vsync period even at 30Hz), so resuming from idle starts a clean window instead. Exposed as a dependency-free static (no `this`) so the exact idle-gap scenario is unit-testable with synthetic timestamps, without a real GPU device (`Renderer.PresentFpsSamplerResetsAcrossIdleGap`/`PresentFpsSamplerDoesNotResetForOrdinaryJank`, `tests/universal/test_renderer.cpp`).
- **Performance overlay's budget line now tracks the actual display refresh rate** instead of assuming 60Hz — found while testing the FPS counter above on a multi-monitor macOS setup (a 60Hz display and a 144Hz one): the overlay's bar-chart budget line stayed hardcoded at 16.67ms even though real presentation cadence (and the FPS counter itself) correctly adapted when the window was dragged onto the 144Hz display, since actual vsync pacing is handled by the OS compositor for whichever screen the window is currently on. New `Renderer::setDisplayRefreshHz()`/`displayRefreshHz()` (defaults to 60, clamped to `[1, 1000]`) feeds `paintUnifiedFrameChart()`'s `kTargetMs`/`kMaxMs`, so the budget line's vertical midpoint always represents the *current* display's actual frame budget. Wired up on macOS: `windowDidChangeScreen:` (new `NSWindowDelegate` method, `src/macos/run_app.mm`) updates both this and `MTKView.preferredFramesPerSecond` whenever the window moves to a different display; other platforms keep the 60Hz default until similarly wired.

### Changed

- **`campello_gpu` dependency bumped `v0.20.0` → `v0.21.0`** (`dependencies/campello_gpu.cmake`) — brings in that release's Metal command-buffer/encoder leak fix, the Vulkan swapchain-recreated-every-frame fix, device-local buffer memory preference, `kFramesInFlight` 2→3, `VK_PRESENT_MODE_MAILBOX_KHR` as the default present mode, and offscreen `VkRenderPass` caching — see `campello_gpu`'s own `CHANGELOG.md` for details. The Android gallery example's temporary `FETCHCONTENT_SOURCE_DIR_CAMPELLO_GPU` override (pointing at a local checkout to test these before they were tagged) has been removed now that the pin covers them.
- **Gallery sidebar nav items get a hover effect** — wrapped in `MouseRegion` (subtle background tint + pointer cursor on hover, skipped for the already-active tab).
- **[Vulkan] Vertex buffers pooled instead of allocated fresh on every draw call** — `drawFilledQuad()`/`drawTexturedQuad()`/`drawClipShapeComposite()` each called `Device::createBuffer()` (a real `VkBuffer` + `vkAllocateMemory`, not just a memcpy) on every single draw, purely to hold six vertices of per-draw geometry — the same problem the Metal backend already solved for its own equivalent buffers via `UniformBufferPool`. Ported the identical pattern to Vulkan (`VulkanDrawBackend::UniformBufferPool`, two instances — `colored_quad_vertex_pool_`/`quad_vertex_pool_`, one per fixed vertex-struct size): a small ring of buffer objects reused round-robin across `kGenerations=4` frames, each draw still getting fresh contents via the now-cheap `Buffer::upload()` instead of a fresh allocation. Found chasing an unrelated regression report ("we made buffer improvements but see no improvement") — turned out the specific change in question (`campello_gpu`'s new device-local-memory-preferring `createBuffer()`) was a *net* small regression on this Android/Adreno UMA device (device-local memory there is already host-visible, so the extra memory-type scanning was pure overhead, ~8-13% slower per draw call across categories) — but the *real* lever was this pooling gap Metal had already closed. Measured live on a Galaxy Tab S7 FE (Snapdragon 750G/Adreno 619), gallery Images tab: main-flush (CPU draw-list encoding) 14.50ms → 9.93ms (-31%), rect draw cost -82%, clip-shape -59%, image -59%, text -38%; overall raster TOTAL 23.30ms → 22.26ms and FPS 42.9 → 44.9 — beating the pre-regression baseline outright, since this also eliminates the *original* per-draw allocation cost that predated the regression, not just the regression itself.
- **`Renderer::applyBoxShadow()`'s Gaussian blur now runs at reduced resolution for softer shadows** — `blur.frag` is a naive per-tap loop (up to 25 texture fetches/pixel/pass, two passes), and its cost scales with the padded offscreen texture's pixel count. Since a blurred shadow is inherently low-frequency content, rendering the fill + both blur passes at half resolution (`sigma >= 1.5f` gates it — small/crisp shadows are left full-resolution, since the softening only becomes perceptually free once the blur itself is already wide enough to dominate over the resolution loss) and compositing back via the existing linear-filtered `drawImage()` upscale is visually indistinguishable while cutting the blur's pixel count 4x. Sigma is scaled down by the same factor passed to `blurTexture()`, keeping the blur's extent identical relative to the shadow's logical size. Root-caused via `CommandBuffer::getGPUExecutionTime()` (real GPU hardware timestamps, not CPU-side timing) on a Galaxy Tab S7 FE: box-shadow blur passes alone accounted for roughly half of the renderer's entire per-frame GPU execution time on a typical UI frame with ~5 shadows (~10-12ms → ~4.5-6ms measured with blur bypassed entirely as an A/B test; ~5-7ms with this downsampling in place, visually correct, essentially matching the no-blur floor).
- **Android and Linux share one Vulkan draw backend** — unified into `src/gpu/vulkan/vulkan_draw_backend.{hpp,cpp}`, replacing Android's separate 463-line copy (`src/android/vulkan_draw_backend.{hpp,cpp}`, deleted). Per-platform text rasterization is now injected via a new `ITextRasterizer` interface (`src/gpu/vulkan/text_rasterizer.hpp`), implemented once each in `src/android/android_text_rasterizer.cpp` / `src/linux/linux_text_rasterizer.cpp`. `MetalDrawBackend` also moved, `src/macos/` → `src/gpu/metal/`, for the same platform-agnostic-backend convention. `ios.cmake`/`macos.cmake`/`windows.cmake` each gained an explicit filter excluding the sibling `src/gpu/{vulkan,metal}/` directory (previously unnecessary since the old paths lived under already-excluded per-platform directories).
- **Vulkan: push constants replace per-draw uniform buffers/bind groups** for rect/rrect/circle/oval (`vkCmdPushConstants` instead of `vkAllocateDescriptorSets`+`vkUpdateDescriptorSets`+`createBuffer` per draw call) — was the dominant Vulkan-vs-Metal raster-time gap (~57 rect draws/frame on the gallery). Android additionally gains `drawCircle`/`drawOval`/`blurTexture`/`drawBackdropFilter` (previously no-ops on that backend) and now queries its swapchain pixel format from the device instead of hardcoding `bgra8unorm` (some devices only expose RGBA8; the mismatch corrupted clip-shape/shader-mask offscreen composites while leaving direct swapchain draws unaffected).
- **Platform-independent text rendering cache** — `D3DDrawBackend` (Windows) and `MetalDrawBackend` (macOS/iOS) each carried their own near-duplicate `text_texture_cache_`; `VulkanDrawBackend` (Linux) had none at all and re-rasterized every `DrawTextCmd` from scratch every frame. Hoisted the cache itself up to `Renderer` (`text_texture_cache_`, `TextTextureCacheEntry`, `kTextTextureCacheMaxAgeFrames = 120`), mirroring the existing `clip_shape_gpu_cache_`/`shader_mask_gpu_cache_` pattern — same eviction sweep, same per-frame counter. `IDrawBackend::drawText()` is replaced by two smaller virtuals: `rasterizeText()` (pure rasterization, no caching) and `drawTextTexture()` (draws an already-rasterized texture as a quad, returning whichever bind group it used so the cache can store it for next time). `TextSpanHash` — previously duplicated verbatim in the Windows and Metal backends — consolidated into a single implementation (`inc/campello_widgets/ui/text_span.hpp`, `src/ui/text_span.cpp`). Gives Linux/Vulkan real text caching for the first time. Verified live on Windows: `text` raster sub-phase dropped from avg 2.04ms/max 17.4ms to avg 0.068ms/max 0.16ms, no regressions in the full universal test suite (530/530).
  - Adapted alongside the three planned backends: `src/android/vulkan_draw_backend.{hpp,cpp}` (a separate implementation from Linux's, discovered via sanity sweep — required the same interface update to keep compiling) and `src/testing/gpu_visual_renderer.mm` (GPU visual-fidelity test renderer).
  - **Windows only was built, tested, and run live this session.** Metal (macOS/iOS, shared backend), Linux/Vulkan, and Android/Vulkan changes are code-parity transforms mirrored from the verified Windows pattern but not compiled or run anywhere in this session — no macOS/Linux/Android toolchain available in this environment.

### Fixed

- **A `DragTarget` briefly flashed green when a `Draggable` entered it, then reverted to grey while the drag was still hovering over it** — a repaint-boundary ancestor (e.g. a box-shadowed card) may replay a cached picture instead of calling `performPaint()` on the target when nothing there is individually dirty, which also skipped `DragManager::updateTargetBounds()`, so the drop-zone hit-test bounds silently drifted stale mid-drag. `RenderDragTarget` now registers a per-tick handler (`PointerDispatcher::addTickHandler()`) that forces `markNeedsPaint()` every tick while `DragManager::active()->isDragging()`, guaranteeing a real repaint (and fresh bounds) for the whole gesture.
- **Dragging anything inside an `Overlay`'s root `Stack` (e.g. a `Draggable`'s feedback widget following the cursor) unnecessarily detached and reattached every sibling in that `Stack` on every pointer move** — `StackElement::syncChildRenderObjects()` called `RenderStack::clearChildren()` unconditionally before re-inserting, and `RenderStack::insertChild()` always called `setParent(nullptr)`/`setParent(this)` even when the exact same child box already occupied that slot. At the root `Overlay`'s `Stack`, this meant the *entire app* got re-parented on every single pointer-move event during a drag (repeated `markNeedsLayout()` bubbling, plus any `attach()`/`detach()` side effects on affected boxes, e.g. `RenderGestureDetector` re-registering with `PointerDispatcher`). `RenderStack::insertChild()` now reuses a slot in place (only updating position fields, and only if they changed) when the same box is already there; new `RenderStack::truncateChildren()` handles removals without re-parenting the children that remain, and `StackElement` re-inserts in place instead of clearing first.
- **Scrolling a `GridView`/`ListView` could swap, repeat, or blank out cell content that used a `ClipRRect`/`ClipOval`/`ShaderMask`/`BoxShadow`** — those composite offscreen textures are cached by `Renderer::{clip_shape,shader_mask,shadow}_gpu_cache_`, keyed by an `OffsetLayer`'s own address, and kept for up to ~120 frames. But `createOffscreenTexture()` may hand back a texture from a size-keyed *rotating pool*, correct for a one-off composite fully re-recorded every time it's used, not for a cache entry relying on that exact physical texture's content staying valid for up to 120 frames — exactly the situation a virtualized grid/list creates by mounting/unmounting many same-sized clipped cells during scroll, silently overwriting another cell's still-cached texture. New `IDrawBackend::createDedicatedOffscreenTexture()` bypasses the pool for anything that's about to be cached long-term this way (implemented on Metal/D3D12 as a real non-pooled allocation; Vulkan never pooled in the first place, so it's already correct there). New `OffsetLayer::~OffsetLayer()` calls `Renderer::evictReplayCacheEntries()` so a destroyed layer's stale cache entries can't collide with an unrelated new layer that happens to reuse the same freed address.
- **`RenderImage::setTexture()` could permanently stop a `RenderObject` from ever repainting again** — it calls `markNeedsPaint()` internally, and calling that from *inside* `performPaint()` (after the base `RenderObject::paint()` wrapper had already cleared the dirty flag for that frame) re-dirties the node with nothing left to ever clear it again, since the node is never revisited once its `RepaintBoundary` ancestor's own flags go quiet. Hit by `RenderDrawSurface` (see the new Draw tab below), whose `performPaint()` used to call `setTexture()` on first allocating its backing texture — moved that allocation into `performLayout()` instead, which always runs before paint in the same frame, so any dirty flag it sets gets correctly consumed by that same frame's paint pass.
- **Gallery Images tab held at ~45fps instead of 60 on macOS, purely from one unrelated animation sharing a scroll view with a `BackdropFilter`** — every class that owns an `OffsetLayer` (`RenderRepaintBoundary`, `RenderClipRRect`, shadow-bearing `RenderDecoratedBox`, all five scrollables) called `OffsetLayer::maybeReplay()` with `needsPaint() || needsDescendantPaint()` folded into one `dirty` flag. Since a boundary whose *own* content never changed still gets `needsDescendantPaint()` set whenever some nested boundary further down does (see `markNeedsDescendantPaint()`), that boundary's forced real re-record was also reporting **its own entire bounds** as a dirty region every such frame — e.g. the gallery's `SingleChildScrollView` reporting its whole 823×720 viewport dirty every frame purely because a small, unrelated `RotatingTransformRow` 190px away was mid-animation. That spurious wide dirty rect defeated `anyRegionDirty()`'s capture-skip gating for any `BackdropFilter` sharing the same scroll view, forcing its full capture-and-blur pass to re-run on literally every frame regardless of how far away on screen the two actually were (confirmed via `CW_TRACE_DIRTY=1`: `needs_capture=1` on 159/159 frames). `OffsetLayer::maybeReplay()` now takes `own_dirty` and `descendant_dirty` as separate parameters — both still force the same real-record fallback, but only `own_dirty` triggers the `noteDirtyRegion()` report, since a genuinely-changed nested boundary (or ordinary dirty node) already reports its own precise bounds separately once the re-record reaches it. All 7 call sites updated. Verified live on macOS (Intel UHD 630): `needs_capture` dropped to 2/267 frames, average FPS 44.6 → 60.0, raster CPU cost 1.4ms → 0.7ms avg. New regression test drives this end-to-end through a real `Renderer` (`RenderObjectPaintPropagation.DistantDirtyDescendantDoesNotForceUnrelatedBackdropFilterCapture`, `tests/universal/test_render_object_paint_boundary_propagation.cpp`) — asserts `FramePackage::has_backdrop_filter` stays false when only a distant, unrelated descendant is dirty; confirmed it fails without the fix.
- **Nested repaint boundaries could freeze permanently dirty** — `markNeedsPaint()` bubbling stops at the first repaint boundary it hits, by design; but a boundary nested inside another boundary then never told the *outer* boundary it had a dirty descendant, so the outer boundary kept replaying its stale cached content forever (observed in practice with a `RenderDecoratedBox` shadow boundary nested outside a `RenderClipRRect` boundary). New weaker signal `markNeedsDescendantPaint()`/`needsDescendantPaint()` (`RenderObject`) continues past a boundary all the way to root; every boundary class (`RenderClipRRect`, `RenderRepaintBoundary`, shadow-bearing `RenderDecoratedBox`, and all five scrollables) now ORs it into their replay-vs-record decision.
- **Taps/drags/cursor placement landed offset by the safe-area inset** on any device with a non-zero one (notched iPhones/iPads, camera-housing MacBooks) — the paint pass is seeded with the safe-area offset so screen-space `offset` and the tree-local coordinates `PointerDispatcher` hit-tests against disagreed. New `RenderObject::activePaintOriginOffset()`/`setActivePaintOriginOffset()` (static, atomic) lets `Draggable`, `DragTarget`, `RenderGestureDetector`, `RenderSlider`, and `RenderTextField` subtract the inset before latching a position for later pointer math.
- **Clips established inside scrolled content used the wrong coordinate space** — `Canvas::clipRect()`/`clipRRect()`/`clipOval()` intersected the caller's local-space rect directly against `current_clip_` (absolute space) without transforming through `current_transform_` first; scroll views apply their offset via `canvas.translate()` rather than by adjusting a passed-in `offset`, so any clip nested inside scrolled content used a pre-scroll rect against a post-scroll clip. New `transformRectAABB()` helper projects all four corners (not just two, to handle rotation/skew) into an AABB before intersecting.
- **A slow-building drag could fail to ever start panning** — pan-slop across all five scrollables plus `RenderGestureDetector` was measured against the previous move event (`pan_last_pos_`, which advances every event) instead of cumulatively from pointer-down; a drag whose individual per-event deltas never exceeded the slop threshold, however far it traveled in total, never triggered. New `pan_down_pos_` (fixed at pointer-down) is used for the threshold check instead.
- **Rapid tapping could silently drop a tap** — `RenderGestureDetector::resolveTapOutcome()`'s double-tap-window check ran unconditionally even when no `on_double_tap` callback was registered, hitting an early return that swallowed the second tap's `on_tap()`. Now gated on `on_double_tap` being non-null first.
- **Android: black screen / Vulkan validation crashes under load** (`VUID-vkDestroySampler-sampler-01082`, `VUID-vkDestroyPipeline-pipeline-00765`) — `campello_gpu`'s Vulkan backend pipelines 2 frames deep without blocking `Device::submit()`, but the old "clear on next `setViewport()`" resource-lifetime logic assumed submission was synchronous and freed buffers/textures/views still in flight on the GPU. Fixed with triple-buffered per-frame resource retention (`frame_*_`/`prev_frame_*_`/`prev2_frame_*_`) in the merged Vulkan backend and `Renderer`'s backdrop-filter view handles.
- **Android: a dropped frame could hang all future frames** — if `beginRenderPass()` returned null, the (possibly empty) command buffer was previously discarded instead of submitted; the frame-ring's fence for that slot then never signaled, and every future frame reusing the slot blocked forever waiting on it. `Renderer::rasterFrame()` now always calls `device_->submit()` regardless.
- **Android: touch input could freeze while a continuous animation was running** — GPU submit is blocking work; running it on the same thread that pumps `android_native_app_glue`'s input queue and `AChoreographer` vsync starved touch delivery under sustained animation load. `src/android/run_app.cpp` now runs a dedicated `RasterThread`, mirroring the existing macOS/Linux split.
- **Android: touch coordinates were wrong on non-1x-density devices** — physical touch coordinates were never divided by DPR before hit-testing.
- **Android: the first frame (and sometimes all subsequent frames) never rendered** — Android's on-demand vsync loop needs an explicit initial request to prime it; added an explicit `FrameScheduler::scheduleFrame()` call on startup.
- **Android: JNI crash under the new `RasterThread` split** (`android_text_rasterizer.cpp`) — `FindClass()` returns a thread-local ref; text measurement/rasterization now run on the raster thread rather than the thread that constructed the rasterizer, so cached classes are now acquired via `NewGlobalRef`/released via `DeleteGlobalRef` instead of caching the original thread-local ref.
- Fixed a hardcoded developer-machine path in `examples/gallery/android/app/src/main/cpp/campello_widgets.cmake`'s `SOURCE_DIR` derivation — now computed from `CMAKE_CURRENT_LIST_DIR`.

## [0.4.0] - 2026-07-18

### Added

- **Automatic repaint-boundary promotion for scrollables** — `RenderSingleChildScrollView`, `RenderListView`, `RenderGridView`, and `RenderPageView` now each own a `PictureLayer`/`OffsetLayer` pair, mirroring Flutter's `RenderViewport.isRepaintBoundary`: clean content replays a cached draw-list slice instead of re-walking the subtree, with no app-level `RepaintBoundary` wrapping required. A pure reposition (offset changed, content didn't) replays via a delta `canvas.translate()` when the cached content has no clip/backdrop-filter/shader-mask geometry; otherwise falls back to a full re-record — proven safe with a dedicated test per scrollable class, since all four always clip to their own viewport in practice.
- **Dirty-region tracking gates `BackdropFilter`'s capture pass** — `Renderer::noteDirtyRegion()`/`dirtyRegionIntersects()` accumulate per-frame dirty rects; the full-viewport backdrop-capture-and-blur pre-pass in `rasterFrame()` now only runs when a filter's own (blur-margin-expanded) bounds actually intersect something that changed that frame, instead of unconditionally on every frame anything anywhere is dirty.
- **`PointerSignalResolver`** (`inc/campello_widgets/ui/pointer_signal_resolver.hpp`) — mirrors Flutter's; coordinates instantaneous scroll/wheel signals across the hit-test path the way `GestureArenaManager` already coordinates down/move/up gestures. A "dominant axis" tier lets a genuinely nested scrollable win its own axis, with a fallback tier so a standalone scrollable (no nested competitor) never silently drops an event just because a swipe wasn't perfectly axis-aligned.
- **Axis-aware pan-slop** for all scrollable types — arena resolution now measures only the scrollable's own axis of movement, not total Euclidean drag distance, matching Flutter's `VerticalDragGestureRecognizer`/`HorizontalDragGestureRecognizer`. Fixes a horizontal `ListView` nested inside a vertical page never being able to win the gesture arena regardless of drag direction.
- **iOS-style rubber-band overscroll** (`BouncingScrollPhysics::applyBoundaryConditions()`) — replaced flat 50% linear resistance (overscroll grows unboundedly with drag distance, just at half rate) with the real formula `f(x) = x·d·c / (d + c·x)`, which asymptotically caps displayed overscroll around 100px regardless of how far or fast the user drags.
- **`DebugFlags::printScrollTrace`** / `CW_TRACE_SCROLL=1` — per-scroll-event and per-tick trace for the scroll-physics render objects (source, offset, requested vs. applied delta), auto-flagging implausible position jumps. Diagnostic-only, safe to leave wired permanently.
- **`RenderDropdownMenuPositioner`** — dedicated `RenderObject` for `DropdownButton`'s popup menu placement, replacing the previous `Align`/`Stack`-based approach.
- Gallery: horizontal-scrolling BoxFit sample strip and a squared preview container (Images tab), a Lists tab (virtualized `ListView`/`GridView` demos).
- **Windows DirectX 12 draw backend, complete** — `D3DDrawBackend` goes from a partial stub (rect/quad/shape/line only) to feature-complete, mirroring `MetalDrawBackend`: adds `drawCircle`/`drawOval`/`drawRRect`/`drawLine` plus the previously-deferred `BackdropFilter` blur and `ClipRRect`/`ClipOval` composite pipelines (new `shaders/dx12/blur.hlsl`, `clip_shape.hlsl`). Introduces uniform/vertex ring-buffer pools, an offscreen-texture pool, and a texture-keyed bind-group cache so continuously-animating content doesn't create a fresh D3D12 heap allocation every frame. Adds `build_dx12_shaders.{bat,ps1}` + `src/shaders/dx12_widgets.h` (compiled HLSL bytecode embedded into a header, same pattern as the existing Vulkan build) and `examples/gallery/windows/run.bat` for local iteration.
- **Clip-shape/shader-mask GPU compositing cache** (`Renderer::clip_shape_gpu_cache_`/`shader_mask_gpu_cache_`) — a `RepaintBoundary`/`OffsetLayer` correctly skips re-walking the widget tree for unchanged content, but until now that gave zero savings on the *GPU* side for anything containing a `ClipRRect`/`ClipOval`/`ShaderMask`: every such bracket redid its full offscreen-capture-and-composite cycle every frame regardless of whether its content actually changed. New `CacheReplayBeginCmd`/`CacheReplayEndCmd` draw-command markers bracket an `OffsetLayer` identity replay (`inc/campello_widgets/ui/draw_command.hpp`); `Renderer::flushDrawList()` recognizes a clip-shape/shader-mask bracket inside one of these regions as guaranteed byte-for-byte unchanged and reuses its cached GPU composite instead of recapturing it, keyed by `(region_id, Nth bracket since the region began)` — safe with no content hashing needed, since a replay is only ever emitted when nothing changed. Correctly handles nested replay regions. `BackdropFilter` content is unaffected (already forced to re-record every frame for an unrelated reason, so it never enters this caching path). Verified live: dropped a representative scene's raster time from ~19.5ms average to ~8.8ms.

### Changed

- Overscroll spring-back is noticeably snappier (`kSpringCoeff` 12→20), closer to iOS `UIScrollView` bounce timing.
- Offscreen textures for clip-shape/shader-mask compositing are now pooled and reused across frames instead of freshly allocated on every draw call (Metal backend, mirrors the existing `UniformBufferPool` pattern) — GPU utilization for continuously-animating clipped content dropped ~50%→15-20%.
- **`campello_gpu` upgraded from v0.16.0 → v0.19.0** — picks up three releases of backend fixes:
  - **v0.17.0** — Wayland swapchain extent resolved from caller-supplied dimensions (compositor always reports `UINT32_MAX`); `campello_gpu_wayland_resize()` helper for between-frame resize; Android build fix for `LinuxSurfaceInfo` guard.
  - **v0.18.0** — `DeviceData::gpu_mutex` serializes all `VkCommandPool`/`VkQueue` access across threads; `swapchainImageAcquired` guard prevents double `vkAcquireNextImageKHR` per frame; `offscreenViewRef` prevents offscreen `TextureView` use-after-free; offscreen layout corrected to `SHADER_READ_ONLY_OPTIMAL`; dynamic rendering disabled for Intel hasvk GPUs (BSW/HSW/BYT/BDW/CHV); swapchain format prefers UNORM over sRGB; `VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT` removed from color render targets; vertex input bindings and attributes now correctly wired in `createRenderPipeline()`.
  - **v0.19.0** — CPU/GPU frame-pacing fix (`Device::submit()` no longer blocks the CPU until the GPU is fully idle after every frame; a 2-deep frame-in-flight ring lets the CPU run one frame ahead instead, fixing dramatic frame-time swings of ~9-13ms vs ~35-85ms for identical recorded work); DirectX 12 command allocator/list/query-heap pooling (createCommandEncoder() reclaims and resets the previous ring slot's objects instead of recreating them from the driver every frame, ~4ms/frame CPU-side overhead eliminated); DirectX 12 `rtvExtraHeap` grown from 64 to 1024 slots with bounds checking (previously silently corrupted D3D12 runtime memory once a widget minted more than 64 distinct offscreen render-target sizes in a session); two DirectX 12 resource-state-tracking bugs (`generateMipmaps()`, `Texture::upload()`) that assumed every texture starts in the `COMMON` state, wrong for render-target-usage textures. See campello_gpu's own `CHANGELOG.md` `[0.19.0]` entry for the full list.

### Fixed

- **`BackdropFilter` inside a scrollable went stale while scrolling** — `RenderRepaintBoundary`'s clean-replay path (identity offset, not dirty) skipped `RenderBackdropFilter::performPaint()` entirely, so its `noteBackdropFilter()` side effect — which gates the capture pass — never fired; the frosted panel kept sliding with scroll while showing a frozen capture. Backdrop-filter-containing content is now permanently uncacheable in `OffsetLayer`.
- **...then showed a mismatched blur once that fix landed** — the dirty-region system's reported bounds assumed a widget's `offset` parameter was always its true on-screen position; false for content painted inside a scroll view, which applies its offset via `canvas.translate()` and never changes `offset` itself. Added `projectedBounds()` (`inc/campello_widgets/ui/dirty_region.hpp`) to project logical bounds through the ambient canvas transform before comparing.
- **`Transform` content vanished for part of every rotation cycle** — a negative-scale ambient transform (the gallery's flip-effect demos) produced a negative destination rect fed straight into the clip-shape SDF shader, which saturates fully transparent for any negative half-extent. Fixed by normalizing the destination rect in `drawClipShapeComposite()` and threading a separate `flip` flag through to mirror UV sampling instead of mirroring geometry.
- **A chain of "always dirty regardless of change" bugs**, compounding into ~40% GPU / 10ms UI cost for 4 simple animated widgets: `markNeedsPaint()` bubbled past repaint boundaries unconditionally instead of stopping there; `RenderObjectElement::update()` called `markNeedsLayout()` unconditionally on every widget update, bypassing every widget's own equality-guarded `updateRenderObject()` override; `RenderFlex::insertChild()`/`clearChildren()` cleared and reinserted every child on every rebuild regardless of whether the child list actually changed; the gallery's own animated row reconstructed a fresh `ImageWidget` every tick instead of reusing one. Combined: GPU 40%→9%, UI build phase 10ms→0.8ms.
- **Nested horizontal `ListView` inside a vertical page never won the gesture arena**, and once fixed, a vertical scroll gesture starting inside that nested list did nothing instead of routing to the outer page — see the axis-aware pan-slop and `PointerSignalResolver` additions above.
- **Overscroll spring-back was numerically unstable and vibrated at the limits** — the previous velocity-based spring formula could overshoot the target with real residual velocity, bouncing off the opposite edge. Replaced with an unconditionally-stable exponential ease of the overscroll distance, with a settle threshold (decay asymptotically approaches but never reaches exactly zero) so the spring actually stops once close enough instead of re-triggering a frame forever.
- **Spring-back could freeze permanently mid-overscroll** — this platform's render loop only produces a frame on explicit request (no free-running vsync); the "still actively scrolling" gate returned early without ever requesting a follow-up frame, so once gated by the very last scroll event of a gesture, nothing ever asked for another frame again and a pending overscroll stayed stuck indefinitely instead of springing back.
- **Virtualized list/grid items mass-unmounted (visible blink) during overscroll** — the mounted-item range was computed from the raw, unclamped overscrolled offset, letting it keep changing throughout an entire bounce even though nothing new was actually being revealed at the edge; now pinned to the range visible exactly at the boundary for the whole overscroll.
- **Overscroll position could snap backward mid-drag** — `applyScrollDelta()` fed its own previous (already-resisted) output back into the boundary-resistance formula, compounding the resistance further on every call instead of reflecting the true drag distance from the boundary; a shrinking per-event delta could retract the displayed overscroll even while still dragging in the same direction. Now tracks a separate, unresisted raw offset as the single source of truth the resistance formula is always applied to fresh.
- **Trailing OS momentum-tail scroll events held spring-back's start delay to several seconds** — every incoming event, however small, refreshed the "still actively scrolling" gate; raised the significance threshold so only deltas ≥8px count, and sub-threshold trailing deltas are dropped entirely while overscrolled instead of applied (previously they still nudged the position while the spring was simultaneously easing it back, so it never quite settled exactly at the limit).
- **Linux CI: `libdecor-0-dev` missing from apt-get install** — `wayland_runner.cpp` unconditionally includes `<libdecor.h>` (the whole file is inside `#ifdef CAMPELLO_WIDGETS_HAS_WAYLAND`), so the package is required whenever Wayland support is compiled in. Also corrected `libfreetype6-dev` → `libfreetype-dev` (Ubuntu 24.04 package name).
- **`CAMPHELLO_WIDGETS_HAS_WAYLAND` typo** — `linux.cmake`, `src/linux/run_app.cpp`, and `src/linux/wayland_runner.cpp` all used `CAMPHELLO_` (missing 'L') instead of `CAMPELLO_`. The define and its guards are now consistent.
- **Image cache was never actually populated** — `ImageLoader::executeTask()` called `ImageCache::put()` on a background thread right after decoding, before any GPU texture existed; `ImageCache::put()` silently no-ops without a texture, so every reload of the same image (a widget rebuild, or scrolling it back into view) re-ran the full decode-from-bytes pipeline instead of hitting a cache. Moved the `put()` call to `ImageWidgetState::checkFuture()`, the one point in the pipeline where a `LoadedImage` has both its decoded pixels consumed and a real texture attached.
- **Degenerate/unbounded layout bounds could corrupt the D3D12 debug layer** — an unbounded ancestor constraint (e.g. an infinite-width container) reaching `RenderGridView`'s cross-axis division, or a similarly degenerate rect reaching `Renderer::applyClipShape()`, cast a NaN/Infinite float to `uint32_t` for GPU texture dimensions — undefined behavior that corrupted the D3D12 debug layer's descriptor tracking instead of failing gracefully. Both now clamp to zero and skip the offending work instead.
- **`forceRefresh()` didn't guarantee the next frame actually repaints** — relied entirely on `markNeedsPaint()`'s own dirty-transition bookkeeping, so toggling a debug flag while the tree was already clean produced no visible change until something else triggered a paint. Now explicitly schedules a frame itself.
- **Windows runtime DLL copy went stale on a dependency-only rebuild** — the post-build DLL copy was a `POST_BUILD` hook on the gallery exe target itself, which MSBuild could skip when only a dependency's DLL changed. Converted to an `OUTPUT`-based custom command that tracks the DLLs' own timestamps directly, with explicit target dependencies (`campello_widgets`/`campello_gpu`/`campello_image`) so a from-scratch multi-config build can't schedule the copy before the DLLs exist.
- **`examples/gallery/windows/run.bat`**: removed a `Tee-Object`-based stdout/stderr pipe to a log file on every run (real per-line overhead, and the log file was never actually consulted for a normal release run); fixed an exe-path assumption that only worked for a single-config (Ninja) build, breaking `run.bat release` against this repo's multi-config (Visual Studio) release build directory; fixed Unix-only (LF) line endings that corrupted `cmd.exe`'s parsing of the script's multi-line `if`/`else` blocks.
- Removed leftover unconditional debug output that had no `DebugFlags` gate: `std::cerr` tracing throughout the async image-loading pipeline (`image_loader.cpp`, `image_provider.cpp`, `image_widget.cpp`) and a stray `printf` on every `TextField` focus loss.

Full universal test suite: 530/530 passing.

## [0.3.7] - 2026-06-28

### Changed

- **`campello_gpu` upgraded from v0.13.2 → v0.16.0** — picks up four releases of backend fixes and additions:
  - **v0.13.3 (Metal)** — `Fence::wait()` no longer returns immediately on a freshly created fence (`MetalFenceData::signaled` defaulted to `true`, causing `Device::submit(cmdBuffer, fence)` + `wait()` to be a no-op); `Buffer::download()` now encodes a blit-encoder `synchronizeResource:` before `memcpy` on `Managed` storage-mode buffers, so GPU writes are visible to the CPU on systems that require it (was silently reading stale data).
  - **v0.14.0** — `ComputePipeline::getWorkgroupSize()` added (returns `threadExecutionWidth()` on Metal; `{1,1,1}` on other backends); fixed missing `<cstdint>` in `compute_pipeline.hpp` that broke GCC 13 / Ubuntu 24.04 builds.
  - **v0.15.0 (Vulkan/Linux)** — Vulkan 1.3 core dynamic-rendering entry points with `VK_KHR_dynamic_rendering` fallback; `VK_KHR_surface` no longer required for headless contexts; portability ICD support (`VK_KHR_portability_enumeration`); compute pipeline null-safety; various Linux crash fixes (no-attachment render passes, unbound pipeline draws, depth/stencil usage on color targets).
  - **v0.16.0 (Vulkan)** — Optional `CAMPELLO_GPU_VALIDATION` CMake flag wires up `VK_LAYER_KHRONOS_validation`; fixed swapchain `minImageCount=0` hang on drivers where `maxImageCount == 0` means "no limit"; fixed `waitStage` dangling pointer in `Device::submit()` (UB on every Vulkan submit with a swapchain); fixed `deviceData->surfaceFormat` always being `VK_FORMAT_UNDEFINED` (shadowed inner declaration caused every dynamic-rendering draw call to be silently rejected by the driver).

## [0.3.6] - 2026-06-21

### Added

- **UI/raster thread split on macOS** — `Renderer::renderFrame()` is now split into `buildFrame()` (UI thread: tick input/animation, rebuild, layout, paint-record into an immutable `FramePackage`) and `rasterFrame()` (GPU encode/submit/present, may run on a dedicated thread), per the evaluation recorded in `TODO.md` on 2026-06-19. `renderFrame()` remains as a synchronous back-compat wrapper, so iOS/Android/Windows/Linux are unaffected. New `RasterThread` (`inc/campello_widgets/ui/raster_thread.hpp` / `src/ui/raster_thread.cpp`) implements a depth-1 handoff — `submit()` blocks the UI thread only until the raster thread has *picked up* the previous package (not until it's finished processing it), so the UI thread can build frame N+1 while frame N is still being rastered, but never more than one frame ahead. Wired into `CampelloMTKDelegate` on macOS (`src/macos/run_app.mm`); `ThreadChecker::rasterInstance()` added as a second, independent thread-binding check alongside the existing UI-thread `instance()`. Verified with `sample`-captured per-thread stacks showing the main thread idling in AppKit's run loop while `RasterThread::workerLoop()` does the encode/submit/present work concurrently.
- **`FramePackage`** (`inc/campello_widgets/ui/frame_package.hpp`) — immutable per-frame snapshot (draw list, viewport, DPR, clear color, backdrop-filter state, target, retained drawable) produced by `buildFrame()` and consumed by `rasterFrame()`, so the raster phase never reads a live, UI-thread-mutable `Renderer` member.

### Changed

- **Enabled ARC for the macOS target** (`macos.cmake`), matching `ios.cmake`. Previously `.mm` files on macOS compiled under MRC by default, which meant `__bridge_retained`/`__bridge_transfer` cast qualifiers were silent no-ops there — found via the easy-to-miss `-Warc-bridge-casts-disallowed-in-nonarc` warning while extending a `CAMetalDrawable`'s lifetime past its call stack for the raster-thread handoff above. `src/macos/platform_menu_delegate.mm` is explicitly opted out of ARC (`-fno-objc-arc` source property) since it deliberately leaks menu objects forever to dodge an AppKit issue where the async keyboard-shortcut updater accesses menu items after the menu bar is replaced — ARC would auto-release those on destruction and reintroduce the crash that leak exists to prevent.
- **Performance overlay redesigned again** — replaced the two-lane layout introduced in 0.3.4 with a single unified frame chart matching Flutter DevTools' actual "Flutter frames chart" (confirmed via the real DevTools docs, not just the in-app `PerformanceOverlay` widget the previous design mimicked): each frame is a UI bar + raster bar pair followed by a blank segment of equal width (`Renderer::paintUnifiedFrameChart()`, `src/ui/renderer.cpp`), sharing one chart area and one budget reference line at the panel's vertical midpoint (full panel height now represents 2× the 60fps budget, i.e. a full-height bar is 30fps-equivalent cost, down from 3× previously). Shows the most recent 20 frames at a fixed 4px segment width rather than the full 64-frame sampler history spread thin (previously under 1px per bar and effectively invisible); the samplers still retain the full history for the averages shown in the label.

### Fixed

- **`RenderRepaintBoundary` replayed a stale cache after being repositioned without being repainted** — its cache bakes in absolute on-screen coordinates (including clip rects, which aren't transform-deferred at flush time) recorded at a specific offset, but nothing checked whether that offset was still current before a clean replay. A `Row`/`Column` repositioning a wrapped child (e.g. `campello_editor`'s Scene tab hierarchy/inspector panels on window resize) does so without calling `markNeedsPaint()` — correct in general, since every other `RenderObject` always repaints at the fresh offset regardless of dirty state. The boundary now remembers the offset it last painted/cached at and treats a change as dirty, forcing a fresh recording. New regression test `RenderRepaintBoundary.RepositionWithoutMarkNeedsPaintForcesReRecordingAtNewOffset` in `tests/universal/test_render_repaint_boundary.cpp`.

Full universal test suite: 487/487 passing (8 new tests this release: 4 in `test_raster_thread.cpp`, 3 in `test_renderer.cpp`, 1 in `test_render_repaint_boundary.cpp`).

## [0.3.5] - 2026-06-20

### Added

- **`RepaintBoundary`** — Flutter-equivalent paint-caching boundary (`inc/campello_widgets/widgets/repaint_boundary.hpp` / `src/widgets/repaint_boundary.cpp`, backed by `RenderRepaintBoundary` in `inc/campello_widgets/ui/render_repaint_boundary.hpp` / `src/ui/render_repaint_boundary.cpp`). Found while investigating why `campello_editor`'s UI time stayed ~2.0ms during camera-orbit dragging despite the actual app-specific work measuring only ~0.3-0.4ms: `RenderObject::paint()` called `performPaint()` unconditionally regardless of `needs_paint_` (unlike `layout()`, which already skips clean subtrees), and `markNeedsPaint()`'s upward propagation to the root meant *any* dirty widget anywhere forced the entire tree — menu bar, every dock panel, all of it — to re-walk and re-emit draw commands every frame. `RenderRepaintBoundary::paint()` overrides the (now-`virtual`) base method: when clean, it replays a cached `DrawList` slice instead of calling `performPaint()` at all, skipping the subtree walk entirely. The cache is recorded by painting the child into the *live* context at its real on-screen offset rather than a separate local context — `PushClipRectCmd` bakes an absolute rect at record time (unlike draw-command geometry, which is transform-deferred to flush time), so an earlier version that recorded locally and relocated via a wrapping `canvas.translate()` on replay left clip rects stuck at the fabricated local origin, clipping content at the wrong position (caught via user-reported visual regression — the viewport's grid rendering outside its bounds — fixed before release). New tests in `tests/universal/test_render_repaint_boundary.cpp`, including a dedicated regression test for the clip-position bug (394/394 passing overall, +5 new). Applied around `campello_editor`'s viewport content as the first real usage. **Caveat (resolved)**: on its own this didn't fix the editor's specific ~1.6ms gap — `Flex`/`ColoredBox`'s `updateRenderObject()` (and the framework's other widgets, consistently) mark render objects dirty unconditionally on every reconciliation pass, so a fully-rebuilt subtree (as `SceneEditorTab`'s three panels were, every camera-drag frame) stayed dirty regardless of any boundary wrapping it — confirmed no measurable UI-time change in the editor at first. Resolved entirely on the `campello_editor` side (no further `campello_widgets` change needed) by scoping the *rebuild*, not just the paint: extracting the viewport into its own nested `StatefulWidget`/`State` so camera movement only dirties that Element, leaving `RepaintBoundary` around hierarchy/inspector free to actually skip their repaint. Editor UI time during camera-orbit dropped 2.0ms → 1.0ms (-50%) once both pieces were in place together. See `TODO.md` for the full writeup.

### Fixed

- **`MetalDrawBackend` text rendering re-rasterized every text widget from scratch on every single frame** — `drawText()` ran the full CoreText layout/CPU-rasterize pipeline and allocated+uploaded a brand-new GPU texture per `DrawTextCmd`, every frame, even for static text that never changes. Added a per-`(text, font, size, color, weight, italic)` texture cache (`lookupOrCreateTextTexture()`/`evictStaleTextTextures()` in `src/macos/metal_draw_backend.hpp`/`.mm`) so unchanged text reuses last frame's GPU texture; entries unused for 120 frames are evicted so scrolled-away or rapidly-changing text doesn't grow the cache unboundedly.
- **`MetalDrawBackend::drawTexturedQuad()` had leftover debug `std::cerr` logging on every call** — two unconditional prints fired on every single text/image draw regardless of any debug flag, which turned out to be the larger of the two costs found while investigating raster performance (see `TODO.md`). Removed.
- Combined effect: `campello_widgets_hello`'s steady-state raster (GPU encode+submit) time dropped from **~3.85ms → ~2.15ms average** (-44%) in Release; `campello_editor` (far more static text) dropped from **~16.6ms → ~5.7ms** (-66%) on an empty scene.
- **`MetalDrawBackend` allocated a fresh GPU `BindGroup`/`Buffer` on every single draw call**, regardless of texture caching — `drawText()`/`drawImage()` rebuilt a `BindGroup` every call, and `drawFilledRect`/`drawShape`/`drawLine`/`drawTexturedQuad` each called `Device::createBuffer()` (a real GPU allocation) for a few floats of uniform data on every draw. Cached text now also caches its `BindGroup` alongside its texture (same cache entry, same eviction); added `UniformBufferPool` — a 4-generation ring of reusable `Buffer`s, refreshed via `upload()` instead of reallocated — wired into all four uniform-buffer call sites. `hello`'s steady-state raster dropped further, **~2.15ms → ~0.89ms** (-58%, **-77% from the original baseline**); per-draw averages now ~0.004–0.05ms for rects and ~0.002–0.05ms for text (was ~0.13–0.32ms pre-fix), putting `hello` in Flutter's "well under 1ms" range even with 100+ draw calls. `campello_editor` (far more draw calls per frame to amortize the savings across) dropped from **~5.7ms → ~1.7ms** (-70%, **-90% from the original ~16.6ms baseline**) raster on an empty scene, with UI+RASTER together (~4.3ms) now comfortably within the 16ms/60fps budget.
- **`RenderBox::setChild()` had leftover unconditional debug `std::cerr` logging on every call** — the UI-side counterpart of the raster bug above. `setChild()` is invoked from `SingleChildRenderObjectElement::performBuild()` on *every rebuild* of any single-child wrapper widget (`Padding`, `Center`, `Container`, `ConstrainedBox`, `Align`, `ClipRect`, `Opacity`, `DecoratedBox`, …) — not just at mount — so this fired continuously during any rebuild-triggering interaction (e.g. dragging an object in the editor). Removed. Also removed the same pattern from `ImageWidgetState::build()`'s steady-state path, which logged twice per rebuild for any mounted `Image` widget.

## [0.3.4] - 2026-06-20

### Added

- **`GestureArenaManager`** — Flutter-equivalent gesture arbitration (`inc/campello_widgets/ui/gesture_arena_manager.hpp` / `src/ui/gesture_arena_manager.cpp`). Recognizers `add()` themselves to a per-pointer arena at `down` and call `resolve(GestureDisposition::accepted|rejected)` instead of unilaterally mutating shared state; the arena picks at most one winner — either eagerly (first explicit accept, once the arena closes) or by default via `sweep()` on pointer-up (first member added wins). Wired into `PointerDispatcher`, which now owns a `GestureArenaManager` and calls `close()`/`sweep()` around `down`/`up`/`cancel` dispatch.
  - Every pointer-handling render object — `RenderDraggable<T>`, `RenderTreeView`, `RenderGestureDetector`, `RenderListView`, `RenderGridView`, `RenderPageView`, `RenderSingleChildScrollView`, and `RenderSlider` — now implements `GestureArenaMember` and competes through the arena instead of independently flipping `dragging_`/`panning_`/`pressed_` flags on raw `PointerDispatcher` events. Fixes the bug where dragging a `Draggable`-wrapped row inside a `TreeView` produced no visual feedback because `RenderTreeView`'s smaller tap slop silently won the race every time.
    - `RenderSlider` claims its arena immediately on `down` (no slop) so it reliably preempts an ancestor scrollable's pan-to-scroll claim.
    - `RenderGestureDetector` self-rejects once a move exceeds slop *unless* it actually has `on_pan_update`/`on_pan_end` wired up — a plain tap/long-press detector (the common "button" case) has nothing to do with movement and gives up its claim instead of blocking an ancestor scrollable from ever winning, mirroring Flutter's `TapGestureRecognizer`. It also claims explicitly when its long-press timer fires, so a competing pan/scroll can no longer steal the gesture afterwards.

### Fixed

- **`DragManager` dangling global pointer** — `DraggableState::dispose()` only called `endDrag(false)`, never clearing `DragManager::active()` when its own lazily-created `DragManager` (`own_manager_`) was the active one. Any `Draggable` whose row got unmounted (e.g. by `TreeView`/`ListView` row virtualization) while it owned the global manager left a dangling pointer; the next drag anywhere in the app called into freed memory (heap-use-after-free, confirmed with AddressSanitizer). `dispose()` now clears the global pointer first when it's the owner.
- **`Element::updateChild()` missing identity short-circuit** — passing the *same* `WidgetRef` instance back into `updateChild()` still called `child->update()` unconditionally, cascading a rebuild through every untouched descendant (mirrors Flutter's `if (child.widget == newWidget) return child;` check, which was missing). This was silently wasteful almost everywhere, but actively destructive for `Overlay`: inserting one new `OverlayEntry` rebuilds `OverlayState`'s `Stack`, which reuses the same entry objects for every other sibling — without the identity check, that cascaded all the way down through e.g. `TreeViewElement::update()`, which unconditionally unmounts and remounts every row, destroying any row mid-gesture.
- **`Stack` only detected a directly-nested `Positioned`** — `StackElement::syncChildRenderObjects()` used `dynamic_cast<Positioned*>` on the top-level widget in `Stack.children`, which is blind to a `Positioned` produced several `StatefulWidget`/`StatelessWidget` layers down (e.g. `OverlayEntry` → `ValueListenableBuilder` → `Positioned`, the shape of `Draggable`'s drag-feedback overlay). Now walks `firstChildElement()` down to the nearest `RenderObjectElement` looking for a `Positioned` anywhere along the way.
- **`RenderStack` forced a tight constraint on partially-specified `Positioned` children** — a child positioned with only `left`/`top` (no `width`/`right`) was tightly constrained to fill all remaining space instead of sizing to its own content, because `BoxConstraints::tight()` was used unconditionally for positioned children. Now only tightens an axis whose size is actually determined (explicit `width`/`height`, or both opposing edges); otherwise the child gets loose constraints up to the remaining space and sizes itself intrinsically.
- **`Positioned` updates never reached a `Stack` after the initial mount** — `Positioned`'s fields are only read when its ancestor `Stack`'s own children list is reconciled; a rebuild that changes only a deeply-nested `Positioned` (e.g. a `ValueListenableBuilder` rebuilding to follow a dragged pointer) left the on-screen offset frozen at whatever it was when first mounted, since nothing told the `Stack` to re-sync. Added `PositionedElement` (mirrors Flutter's `ParentDataElement`): every mount *and* update now re-notifies the nearest `RenderObjectElement` ancestor via the existing `onDescendantRenderObjectChanged()` bubble, so `Stack` re-reads the latest `Positioned` and re-applies the offset regardless of how many non-RenderObject elements sit in between.
- **`GestureDetector.on_pan_down`** — new callback fired immediately on pointer-down (before the pan slop gate), passing the box-local position — mirrors Flutter's `GestureDetector.onPanDown(DragDownDetails)`. Lets consumers hit-test against rendered content (e.g. picking a 3D gizmo handle) at the exact moment a drag begins.
  - `PointerEvent` gained a `local_position` field — the hit point in the recipient render box's own coordinate space, filled in per-target by `PointerDispatcher`.
  - `PointerDispatcher` now retains full `HitTestEntry` (target + local position) for a captured pointer instead of bare `RenderBox*`, and derives `move`/`up` local positions from the down-time anchor plus the global delta (valid since ancestor offsets are pure translations).
  - `RenderGestureDetector` exposes `on_pan_down`; `GestureDetector` threads it through `createRenderObject`/`updateRenderObject` alongside the existing gesture callbacks.

### Changed

- **Performance overlay redesigned to match Flutter's model** — previously, `Renderer`'s single graph plotted the wall-clock gap between successive `renderFrame()` calls (request cadence), not how long any actual work took; two frames requested close together (for any reason) showed as a misleadingly tiny bar regardless of real cost, and the "FPS" text was the average of those gaps rather than a meaningful frame rate. Now shows two lanes like Flutter's `PerformanceOverlay`/DevTools frame chart: **RASTER** (GPU command encode + submit time) on top, **UI** (widget rebuild + layout + paint-command recording time) on bottom — each bar is the actual measured duration of that phase for that frame, bracketed with `std::chrono::steady_clock` timestamps in `renderFrame()`, independent of how often a frame was requested. Backed by the new `FrameTimeSampler::recordDuration()` in campello_gpu.
- **campello_gpu** upgraded from `v0.13.1` → `v0.13.2` (carries the `FrameTimeSampler::recordDuration()` addition the performance overlay above depends on; the dependency wrapper had been pinned to a local checkout while this tag didn't yet exist, now reverted to the standard `GIT_REPOSITORY`/`GIT_TAG` form)

### Fixed

- **Duplicate per-frame ticking** — every platform shim (macOS, iOS, Linux X11, Linux Wayland, Windows) called `PointerDispatcher::tick()`/`TickerScheduler::tick()` itself immediately before calling `Renderer::renderFrame()`, which already ticks both internally. The result was two ticks per actually-drawn frame, which could schedule a second, redundant `setNeedsDisplay`-equivalent redraw request right after the first — visible as a back-to-back pair of frames (one near-0ms apart, one at the normal vsync interval) whenever continuous pointer input (e.g. dragging) kept marking the tree dirty. Removed the duplicate external tick call from each platform's `renderFrame()`-adjacent call site; `Renderer::renderFrame()` remains the single owner of ticking. The Wayland idle-timeout tick (which fires only when `renderFrame()` is *not* called that loop iteration, to keep tickers alive while otherwise idle) was left untouched since it isn't a duplicate.

## [0.3.3] - 2026-04-29

### Added

- **macOS PlatformMenu function-key support** — `PlatformMenuDelegate` now recognises all AppKit special keys in shortcut strings:
  - **Function keys**: F1 – F20
  - **Navigation**: PageUp, PageDown, Home, End
  - **Editing**: Insert, Delete, Backspace, ForwardDelete, Space
  - **Other**: Help, Clear
  - Shortcuts like `"Cmd+F1"`, `"Ctrl+F5"`, `"Shift+PageUp"` now produce the correct `NSMenuItem.keyEquivalent` and modifier mask instead of falling back to the first character of the key name.
- **`initializeMacOSPlatformMenuDelegate()`** now declared in `inc/campello_widgets/macos/run_app.hpp` so integration tests and custom entry points can install the delegate manually.

### Fixed

- **macOS top-level menu item titles** — `NSMenuItem`s added to the menu bar now have their `title` set from the submenu title, so iterating `-[NSMenu itemArray]` and checking `title` correctly finds user-defined menus (e.g. `"File"`, `"Edit"`).
- **Integration test build** — `tests/CMakeLists.txt` now:
  - Globs `.mm` (Objective-C++) files on macOS for the platform integration test target (was only collecting `.cpp`, so `test_macos_platform_menu.mm` was never compiled).
  - Disables Unity Build for the integration test target when `.mm` sources are present, preventing "expected unqualified-id" errors from combining Objective-C++ headers into C++ unity batches.

## [0.3.2] - 2026-04-28

### Changed

- **campello_gpu** upgraded from `v0.13.0` → `v0.13.1`

### Fixed

- **`debug_assert.hpp` Windows compatibility** — replaced `std::raise(SIGTRAP)` with platform-aware `CW_DEBUG_BREAK()` macro: `__debugbreak()` on Windows (`_WIN32`), `std::raise(SIGTRAP)` elsewhere. Fixes build failure on MSVC where `<csignal>`/`SIGTRAP` is not available for inline debug breaks.

## [0.3.1] - 2026-04-28

### Changed

- **campello_gpu** upgraded from `v0.12.0` → `v0.13.0`
- **campello_image** upgraded from `v0.4.0` → `v0.5.0`

## [0.3.0] - 2026-04-26

### Added

- **Linux X11 platform runner** — full X11 window with Vulkan swapchain via `campello_gpu`:
  - `runApp()` creates an X11 window, initialises the Vulkan device through `LinuxSurfaceInfo` (X11 display + window handle), and runs the event loop
  - Pointer events (motion, button press/release) dispatched through `PointerDispatcher`
  - Keyboard events translated via `XkbKeycodeToKeysym` and routed through `FocusManager`
  - `FrameScheduler` callback wired so `setState()` and animations trigger platform redraws
- **Linux IBus IME integration** — D-Bus based input method editor for composed characters (accents, CJK):
  - `IbusIme` class connects to the IBus daemon over session D-Bus
  - `processKeyEvent()` forwards key events to IBus; consumed keys are translated into `commit_string` and `update_preedit` signals
  - `TextInputManager::setOnInputTargetChanged()` automatically focuses/defocuses IBus when a `TextField` gains/loses focus
  - Cursor rectangle reported to IBus in screen coordinates for popup positioning
- **Vulkan draw backend** (`VulkanDrawBackend`) — implements `IDrawBackend` for Linux:
  - Two SPIR-V pipelines (solid rect + textured quad) with premultiplied-alpha blending
  - Procedural geometry (no vertex buffers); per-draw uniform buffers for transform + paint data
  - `drawRect`, `drawImage`, `drawText`, and `measureText` fully implemented
  - Scissor caching to minimise `setScissorRect` calls
  - `setViewport()` must be called per-frame before `Renderer::renderFrame()`
  - Embedded SPIR-V shaders auto-generated by `build_vulkan_shaders.sh`
- **Linux text rasterizer** (`LinuxTextRasterizer`) — FreeType + HarfBuzz + fontconfig:
  - `fontconfig` discovers the system sans-serif font path
  - `hb_shape()` performs Unicode bidi shaping with HarfBuzz
  - `FT_Load_Glyph(..., FT_LOAD_RENDER)` renders each glyph to a grayscale bitmap
  - Glyphs are composited into a premultiplied BGRA8 CPU buffer, then uploaded to a transient GPU texture
  - `measure()` returns accurate bounding boxes from glyph extents
- **Linux Wayland platform runner** — dual-backend with runtime X11/Wayland detection:
  - Checks `WAYLAND_DISPLAY` at startup; if set, uses the Wayland backend, otherwise falls back to X11
  - Wayland implementation: `wl_display_connect`, registry global discovery (`wl_compositor`, `wl_seat`, `xdg_wm_base`), `wl_surface` → `xdg_surface` → `xdg_toplevel`
  - Pointer input via `wl_pointer` listener (`wl_fixed_t` to logical pixels)
  - Keyboard input via `wl_keyboard` + **xkbcommon** (`xkb_state_key_get_one_sym`, `xkb_state_key_get_utf32`, modifier masks)
  - Vsync-aligned rendering via `wl_surface_frame` callbacks
  - Reuses IBus IME on Wayland (D-Bus is display-server agnostic)
  - Optional CMake integration: `pkg_check_modules(WAYLAND_DEPS wayland-client xkbcommon)`; defines `CAMPHELLO_WIDGETS_HAS_WAYLAND` when available
  - Protocol bindings generated from upstream XML via `generate_wayland_protocols.py` (minimal `wayland-scanner` replacement)
- **`IDrawBackend::setViewport()`** added to the interface as a virtual no-op so that `Renderer::drawBackend()` can call it polymorphically across all platforms
- **CI matrix expansion** — `.github/workflows/ci.yml` now covers:
  - **Desktop**: Debug + Release for macOS (arm64 native + x86_64 cross-compile), Linux (x86_64), and Windows (x86_64)
  - **Tests**: executed on Debug and Release for Linux and Windows; macOS Debug only (to save CI time)
  - **iOS**: Debug + Release cross-compilation for arm64
  - **Android**: Debug + Release for `arm64-v8a` and `x86_64`
  - **Caching**: per-platform, per-arch dependency source caches keyed on `dependencies/*.cmake` hashes
  - **Linux deps**: installs all required build packages (`libvulkan-dev`, `libx11-dev`, `libdbus-1-dev`, `libfreetype6-dev`, `libharfbuzz-dev`, `libfontconfig1-dev`, `libwayland-dev`, `libxkbcommon-dev`)
- **Design System abstraction** — complete theming layer decoupling visual style from widget logic:
  - `DesignTokens` — value types for colors (`ColorScheme`), typography, shapes, spacing, motion, and elevation
  - `DesignSystem` — abstract interface with 19 `buildXxx(Config)` methods covering Button, Switch, Checkbox, Radio, Slider, TextField, Card, ProgressIndicator, Tooltip, ListTile, Divider, AppBar, NavigationBar, Dialog, SnackBar, PopupMenuButton, DropdownButton, PrimaryActionButton, and TabBar
  - `Theme` — `InheritedWidget` that propagates `std::shared_ptr<const DesignSystem>` down the tree; `Theme::of()` returns a static fallback `CampelloDesignSystem` when no theme is present
  - `CampelloDesignSystem` — concrete implementation with a distinct warm-teal visual identity (not Material, not Cupertino); `light()` and `dark()` factory presets
  - Adaptive thin-wrapper widgets — `Button`, `Card`, `Divider`, `ListTile`, `AppBar`, `NavigationBar`, and `PrimaryActionButton` now delegate their `build()` to the active design system
  - Canonical configured widgets — `Switch`, `Checkbox`, `Radio`, `Slider`, `TextField`, `CircularProgressIndicator`, `LinearProgressIndicator`, `Tooltip`, `PopupMenuButton`, `DropdownButton`, `TabBar`, `Dialog`, and `SnackBar` are constructed and themed by `CampelloDesignSystem`
  - macOS showcase example updated with a "Theme" tab demonstrating adaptive widgets and a live dark-mode toggle

### Changed

- **Dependencies** — `campello_gpu` upgraded from v0.11.1 to v0.12.0 (adds mipmap generation and per-mip `copyTextureToTexture`)
- `linux.cmake` — reworked Linux build configuration:
  - Added `pkg_check_modules` for X11, D-Bus, FreeType2, HarfBuzz, and fontconfig (all required)
  - Added optional `wayland-client` + `xkbcommon` detection
  - Added `src/linux/protocols` to private include directories

## [0.2.4] - 2026-04-13

### Added

- **`FrameScheduler`** — on-demand frame scheduler mirroring Flutter's `SchedulerBinding.scheduleFrame()`. The platform registers a callback once at startup; any dirty-tree event (`setState()`, `markNeedsPaint()`, new animation ticker) calls `scheduleFrame()`, which requests exactly one platform redraw. The display system goes completely idle when nothing changes, eliminating continuous rendering overhead.
- **Vsync-aligned platform render loops** — all four platform entry points now use idle-friendly, vsync-driven frame production instead of polling loops:
  - **macOS** — `CampelloMTKView` runs with `paused=YES` and `enableSetNeedsDisplay=YES`; `FrameScheduler` calls `[setNeedsDisplay:YES]`
  - **iOS** — `CampelloMTKView` runs with `paused=YES` and `enableSetNeedsDisplay=YES`; `FrameScheduler` calls `[setNeedsDisplay:YES]`; `viewDidLayoutSubviews` requests a frame on rotation or resize
  - **Windows** — dedicated vsync thread calls `DwmFlush()` then `InvalidateRect` so `WM_PAINT` fires at the DWM composition boundary; main thread uses blocking `GetMessage` instead of `PeekMessage` busy-wait; `dwmapi` added to linked libraries
  - **Android** — `AChoreographer_postFrameCallback` (API 24+) delivers vsync to the main `ALooper`; `ALooper_pollOnce(-1)` blocks with zero idle CPU instead of spinning
- **iOS drawable scheduling** — iOS `runApp` now ties presentation to GPU completion via `campello_gpu::Device::scheduleNextPresent`, matching the macOS tearing fix.
- **Unified macOS Showcase Example** (`examples/macos_showcase/`) — replaces the need to launch individual macOS demos separately. A single `DefaultTabController` + `TabBar` + `TabBarView` app hosts all major feature demos in one window:
  - Counter, ListView, Animations, Gestures, TextField, KeyboardListener, TableView, TreeView, and ImageWidget
  - Includes a working `PlatformMenuBar` with File / Edit / View / Window / Help menus
  - Build and run via `run_macos_showcase.sh` (Debug) and `run_macos_showcase_release.sh` (Release)
- **iOS IME support** — `CampelloMTKView` now conforms to the `UITextInput` protocol, enabling full software-keyboard composition on iPhone and iPad:
  - Implements all required `UITextInput` methods (`setMarkedText:selectedRange:`, `unmarkText`, `selectedTextRange`, `markedTextRange`, `insertText:`, `deleteBackward`, etc.)
  - Added `CampelloTextPosition` and `CampelloTextRange` helpers that wrap byte offsets into `TextEditingController`
  - `TextInputManager` now fires an `onInputTargetChanged` callback so the platform can show/hide the software keyboard via `becomeFirstResponder` / `resignFirstResponder`
  - Dead-key filtering for hardware keyboards (e.g. Bluetooth keyboard accents)
- **Windows IME support** — integrated Windows Input Method Manager (IMM32) for composing complex characters:
  - Handles `WM_IME_STARTCOMPOSITION`, `WM_IME_COMPOSITION`, and `WM_IME_ENDCOMPOSITION`
  - Reads composition (`GCS_COMPSTR`) and result (`GCS_RESULTSTR`) strings via `ImmGetCompositionStringW`
  - Converts UTF-16 IME strings to UTF-8 and routes them through `TextInputManager` to `TextEditingController`
  - Suppresses `WM_CHAR` while composing to prevent duplicate insertion
  - `imm32` added to Windows link libraries

### Changed

- `RenderObject::markNeedsPaint()` — when called on the root render object (no parent), it now calls `FrameScheduler::scheduleFrame()` so the platform automatically requests a frame, matching Flutter's `PipelineOwner` behaviour.
- `TickerScheduler` — automatically re-arms the next frame via `FrameScheduler::scheduleFrame()` while tickers are active, and requests the first frame on new subscriptions so animations start immediately.
- `StatefulWidget::setState()` — now calls `FrameScheduler::scheduleFrame()` after scheduling the build, ensuring state changes trigger a platform redraw.

### Fixed

- **macOS Metal tearing** — changed `CampelloMTKView` to schedule drawable presentation through `MTLCommandBuffer presentDrawable:` (via `_device->scheduleNextPresent`) instead of calling `[drawable present]` separately on the CPU. This ties presentation to GPU completion and display vsync, eliminating "present before render" tearing artefacts.
- **Debug logging cleanup** — removed verbose `std::cerr` traces from `RenderImage`, `RawImage`, and `RenderObject` that were left over from image-loading development.

## [0.2.3] - 2026-04-12

### Changed

- **Dependencies** — `campello_gpu` upgraded from v0.5.0 to v0.8.0
- **Dependency guard policy** — all dependency cmake wrappers now skip `FetchContent` when the target is already defined by a parent project (`if(TARGET <name>) return()`), preventing duplicate target errors when `campello_widgets` is consumed as a subdirectory: `campello_gpu.cmake`, `campello_image.cmake`, `vector_math.cmake`, `campello_input.cmake`

### Fixed

- **CI Build Issues** — fixed platform-specific build failures:
  - iOS: updated cmake configuration and run_app.mm for compatibility
  - Android: fixed build configuration issues
  - Windows: updated campello_input dependency handling

## [0.2.2] - 2026-04-08

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

- **Windows build failure** — `<windows.h>` defines `min(a,b)` / `max(a,b)` as macros, which expanded in `Slider`'s constructor initializer list (`min(min_val), max(max_val)`) producing C2059/C2612 syntax errors and cascading C3668 `override` failures across `Button`, `Navigator`, `Divider`, `Card`, `ListTile`, `FloatingActionButton`, `SnackBar`, and a C1004 unexpected-EOF on the whole unity batch. Fixed by adding `NOMINMAX` to `campello_widgets`' compile definitions in `windows.cmake`.
- **Linux, Android, and iOS CI build failures** — `campello_input` v0.2.1 has broken platform cmake/source files on all three platforms: `linux.cmake` never calls `add_library` (causing the upstream `install()`/`get_target_property()` calls to error during configure); `android.cmake` requires the AGDK `game-activity` package which is not installed on standard CI runners; `touch_apple.mm` references `UITraitCollection.maximumNumberOfTouches` (a non-existent property) and `CHHapticPatternPlayer` (missing import), causing compile errors on the iOS SDK. Since `campello_widgets`' Linux, Android, and iOS code does not reference any `campello_input` symbols, the `campello_input.cmake` wrapper now creates an `INTERFACE` stub target on all three platforms instead of calling `add_subdirectory`, satisfying the link dependency without requiring a working `campello_input` build.
- **Unity Build Compilation Errors** — fixed symbol conflicts when unity build is enabled:
  - `campello_gpu` — disabled unity build due to naming conflicts between `campello_gpu::Device`/`Buffer` and Apple's `MTL::Device`/`MTL::Buffer`
  - `campello_input` — disabled unity build due to Objective-C++ (`.mm`) files incompatible with C++ unity batches
  - `campello_widgets` — disabled unity build due to Objective-C++ platform files
  - `libwebp` (via `campello_image`) — disabled unity build for `sharpyuv`, `webpencode`, `webpdecode`, `webpdspdecode`, `webputilsdecode` targets due to static function name conflicts (`clip()`, `GetPSNR()`, `Shift()`)
  - **Test files** — extracted shared helper functions (`flutterGoldenExists`, `getFlutterGoldenPath`, `getCppOutputPath`, `goldenFileExists`, `loadGolden`) and constants (`kFidelityWidth`, `kFidelityHeight`) into `tests/universal/visual_fidelity_helpers.hpp` to prevent redefinition errors when test files are combined in unity builds
- **PlatformMenuItemLabel::create overload ambiguity** — removed ambiguous `create(std::string label, bool checked, std::function<void()> on_selected)` overload that caused `const char*` (string literal) to preferentially convert to `bool` rather than `std::string` during overload resolution, breaking calls like `create("Open", "Cmd+O", callback)`

### Changed

- **Dependencies** — `campello_gpu` upgraded from v0.5.0 to v0.7.0; added `campello_image` v0.3.1 for image loading capabilities

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
