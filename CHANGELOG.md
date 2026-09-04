# Changelog

All notable changes to campello_widgets will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.8.2] - 2026-09-04

### Fixed

- **Disabled buttons rendered nearly illegible across `campello_ui`, `campello_material`, and
  `campello_cupertino`** — each `DesignSystem`'s `buildButton()` faded a disabled button's
  background/foreground colors to 40% opacity via `withOpacity(bg/fg, 0.4f)`, then separately
  wrapped the *entire* button in an outer `Opacity{0.4f}` for the same disabled state — the two
  compounded to an effective ~16% opacity (0.4 × 0.4) instead of the intended single 40%, most
  visibly on `ButtonPriority::danger` (white text on red faded to ~16% read as nearly invisible
  white-on-pale-pink). Caught via a screenshot of a disabled "Stop" button in a consuming app.
  Fixed by removing the redundant manual pre-fade in all three and keeping the single outer
  `Opacity` wrap as the sole source of disabled dimming. `campello_fluent`'s `buildButton()` never
  had this pattern and was unaffected. Verified against all four `*_tests` binaries (855 passing,
  69 pre-existing GPU-visual skips, 0 failures) — the existing `BuildDisabledButtonReturnsWidget`-
  style tests only assert the widget is returned, not exact opacity math, so this class of bug
  wasn't caught by them; not adding a stricter regression test in this pass since none of the
  three affected files have an existing pattern for asserting composited/effective opacity through
  a nested `Opacity` widget.

## [0.8.1] - 2026-09-04

### Fixed

- **`AnimationController::notifyListeners()` reentrancy use-after-free** — a real crash (SEH access violation), reproduced and root-caused via a debugger. `notifyListeners()` already snapshots its listener list before iterating so a listener can safely remove itself/another during notification, but that snapshot only protects against container corruption, not against a listener's *captured state* going stale mid-batch: `StatefulElement::setState()` rebuilds synchronously, so an earlier listener in the same batch (e.g. `NavigatorState`'s own, which prunes a just-finished pop transition's route on every `setState()`) could synchronously unmount a widget whose `State` was a later, not-yet-invoked listener in that same snapshot (e.g. that route's `SlideTransitionState`) — its `dispose()` correctly removed itself from the *live* list first, but the snapshot still held a copy of its now-dangling closure. Fixed by re-checking each snapshot entry against the live list immediately before invoking it.
- **[Windows] `RenderObject`'s `inline static` atomics weren't shared across the DLL boundary** — `WINDOWS_EXPORT_ALL_SYMBOLS` only auto-exports function symbols, not data symbols (the same class of bug already worked around for `DebugFlags`). The DLL and any executable linking against it each got their own private copy of `s_active_backend_`/`s_active_dpr_`/`s_active_paint_origin_x_`/`s_active_paint_origin_y_`, so setting the active paint-origin offset from outside the DLL was silently invisible to `RenderBox::paint()` running inside it. Fixed with the same `dllexport`/`dllimport` pattern, applied to the individual static members.
- **[Windows/DirectX 12] Root-signature mismatch broke every pipeline with a second bind group** (`quad`, `blur`, `clip_shape`, `shader_mask`, `icon` — anything binding a texture+sampler as a separate bind group) — `CreateGraphicsPipelineState` failed validation on every single frame after a `campello_gpu` bump changed how bind-group indices map to DirectX register spaces (bind group *N* → HLSL `spaceN`), but these shaders declared their second bind group's registers with no explicit space (defaulting to `space0`). Fixed by adding explicit `space1` (Shader Model 5.1, which `space` syntax requires) to the affected `.hlsl` files. Also regenerates `dx12_widgets.h` with the `rect_aa`/`vertices`/`arc`/`icon` shader pairs, whose `.hlsl` sources existed from recent Canvas/Icon work but had never actually been compiled on a Windows machine.
- **Flaky velocity/momentum tests** (`GestureDetectorFixture`, `ScrollViewFixture`, `RenderListView`, `RenderViewport`) — root-caused to `VelocityTracker` timestamps coming from `std::chrono::steady_clock::now()` called deep inside each recognizer's own event-handling code, so unrelated processing overhead (much higher in unoptimized Debug builds under a loaded CI runner) could age fast-moving samples out of the tracker's 100ms window before release. `PointerEvent` gains a `timestamp_ms` field, stamped once at `PointerDispatcher::handlePointerEvent()`'s single choke point; every direct `VelocityTracker` feed site across 9 files now reads that instead of calling `now()` itself, and the affected tests use explicit deterministic timestamps instead of `sleep_for()`.
- **[Windows] Unity Build redefinition of `utf8ToUtf16`/`utf16ToUtf8`** — `clipboard.cpp` declared its own anonymous-namespace copies with the same signature as existing ones in `d3d_draw_backend.cpp`/`run_app.cpp`; harmless per-file, but Unity Build merges every file's anonymous namespace into one, producing a hard redefinition error. Renamed clipboard.cpp's copies.
- **[Windows] `M_PI` undeclared in `test_rs_transform.cpp`** — `<gtest/gtest.h>`'s own transitive `<cmath>` include beat this file's to `_USE_MATH_DEFINES`. Replaced with an explicit literal constant, matching this codebase's existing convention elsewhere (`scale_gesture_recognizer.cpp`, `d3d_draw_backend.cpp`) for the same reason.
- **[Android] Build failure: `AChoreographer_postFrameCallback64` needs API 29+** — CI's `android-28` floor is deliberate (the Vulkan 1.1 minimum `campello_gpu` requires), but that symbol is compile-time gated to API 29. Resolved via `dlsym` instead of a direct call, falling back to the API-24 `AChoreographer_postFrameCallback()` when unavailable.
- **[Linux] CI only installed Vulkan headers, not a runnable implementation** — GitHub-hosted Linux runners have no real GPU and no headless-rendering fallback without one (unlike macOS/Metal or Windows/WARP), so every `VisualFidelity.*` test that actually creates a `campello_gpu::Device` failed outright. Installs `mesa-vulkan-drivers` (lavapipe) and points `VK_ICD_FILENAMES` at it before running tests.
- **[Windows/GDK] CI never actually propagated `GameDKCoreLatest`** — the GDK installer sets it as a machine-level environment variable, but the workflow step's own already-running PowerShell process never picks up another process's environment change; the step detected this and printed a warning but never fixed it, so the Configure step reliably failed. Now reads it back from the registry (`Machine`, falling back to `User` scope) and forwards it via `$GITHUB_ENV`.

### Changed

- Bumped the `campello_gpu` dependency to v0.24.1 (adds a missing `operator|`/`&`/`|=` for `ShaderStage`, a bitmask `enum class` whose own doc comment already promised combinable-flag usage that had never actually compiled).

## [0.8.0] - 2026-09-03

### Added

- **`FocusScope` — Flutter-parity focus containment and restoration**
  (`inc/campello_widgets/widgets/focus_scope.hpp`) — wrap a modal's content
  in one so Tab/Shift+Tab and directional (arrow-key) focus movement can
  no longer walk out of it into the rest of the app, and so closing it
  automatically restores focus to whatever was focused immediately before
  it opened. A thin `Focus` subclass (`Focus` itself gained a `scope` bool
  prop) that auto-creates and flags its own `FocusNode` — no new
  RenderObject/Element machinery. `FocusNode` gained `isScope()`,
  `previouslyFocusedOutside()` (scope bookkeeping, `FocusManager`-only),
  and a `traversal_policy` slot; `FocusManager` gained
  `nearestEnclosingScope()` and a private `traversalCandidates()` filter
  used identically by `moveFocusForward()`/`moveFocusBackward()`/
  `moveFocusDirectional()`. `showDialog()` (`src/widgets/dialog.cpp`) now
  wraps its content in a `FocusScope` — the single choke point behind all
  of campello_editor's in-app dialogs — with `auto_focus = true` and
  Escape wired to the same dismiss path `ModalBarrier`'s tap-dismiss
  already used (independent of `barrier_dismissible`, which only governs
  click-outside). Verified against the real subtree-teardown order rather
  than assumed: a scope's own `unregisterNode()` call is guaranteed to run
  before any focused descendant's, since a removed subtree's `RenderObject`
  graph stays fully linked until a single `shared_ptr` release at the tree
  boundary triggers a normal top-down member-destructor cascade — not
  Element-level `unmount()`, which is bottom-up but never touches
  `RenderObject` parent/child links itself. Covered by 22 new unit tests
  in `tests/universal/test_focus_manager.cpp` (registration/traversal,
  ancestor key-event bubbling, scope containment, scope open/close
  restore including nested scopes and dangling-reference cleanup,
  traversal policies, `Focus`/`FocusScope` widget wiring) — 607/607 tests
  green.

- **`FocusTraversalPolicy`** (`inc/campello_widgets/ui/focus_traversal_policy.hpp`)
  — pluggable Tab/Shift+Tab ordering per scope, mirroring Flutter's real
  `FocusTraversalPolicy`. `OrderedTraversalPolicy` (registration order) is
  `FocusManager`'s global default — matches every Tab flow already working
  before `FocusScope` existed, so nothing changes unless a scope opts in.
  `ReadingOrderTraversalPolicy` sorts by each node's real last-painted
  `bounds()` (top-to-bottom, then left-to-right), matching Flutter's own
  default; available via `scope_node->traversal_policy = std::make_shared<
  ReadingOrderTraversalPolicy>()` but not applied anywhere by default yet.

- **`FocusNode::skipTraversal()`/`canRequestFocus()`** — the remaining two
  pieces of Flutter-parity `FocusNode` API: `skipTraversal` excludes a
  node from Tab/directional traversal candidate lists without preventing
  programmatic `requestFocus()`; `canRequestFocus` (checked once, in
  `FocusManager::requestFocus()`, covering both traversal-driven and
  programmatic calls) makes `requestFocus()` an outright no-op. Exposed as
  primitives; not yet wired into any specific campello_editor call site
  (no current concrete need identified).

- **`CupertinoDesignSystem::liquidGlass()`** — the first wiring of the
  Liquid Glass rendering primitive into an actual `DesignSystem`. New
  `CupertinoMaterial` enum (`classic`/`liquidGlass`) selects the style,
  same class/library as before (no new widget types, no new sibling
  library) via a `material` parameter on the existing constructor
  (defaults to `classic`, so all existing call sites are unaffected) and
  a `liquidGlass(bool dark = false)` factory parallel to `light()`/
  `dark()`. Wired into `buildCard()` (elevated priority only — `filled`/
  `outlined` stay classic, matching real HIG's flat grouped-list surfaces)
  and `buildPrimaryActionButton()` so far; `Dialog`/`BottomSheet`/
  `NavigationBar`/`AppBar` need either a core widget change (`Dialog`
  paints its own background with no filter hook) or asymmetric
  corner-radius shader support (edge-flush surfaces need rounding on only
  some corners) — both out of scope for this pass, see `TODO.md`.
  `examples/gallery`'s design-system switcher gained a 4th "Glass" segment
  and a new Card + PrimaryActionButton demo row to exercise it.

- **`ImageFilter::liquidGlass()`** — a first-party Apple Liquid Glass
  rendering primitive on the Metal backend, applied via the existing
  `BackdropFilter` widget. Refracts, saturates, tints, and adds a specular
  rim highlight to the (pre-blurred) content behind it, shaped to a
  rounded rect via a signed-distance-field (SDF) evaluated per pixel —
  corners fall out of the SDF formula for free, no shape-specific
  branching. `ImageFilter` gained a `kind` field (`gaussianBlur` default /
  `liquidGlass`) rather than becoming a `std::variant`, specifically so
  backends without a real `liquidGlass` implementation (everything but
  Metal, for now) degrade gracefully to plain frosted blur instead of
  failing to compile. New Metal shader pipeline in
  `shaders/metal/widgets.metal` (`liquidGlassVertex`/`liquidGlassFragment`,
  reusing `shapeFragment`'s existing rounded-rect SDF math). Demoed
  side-by-side with the existing frosted-glass example in
  `examples/gallery`'s Clipping & FX tab. See `TODO.md` for the full
  design rationale (why not a separate `campello_glass` library, why not
  generic custom-shader loading) and known v1 simplifications (fixed
  light direction, Metal-only, no `DesignSystem`/`campello_cupertino`
  integration yet).

- **`examples/gallery`'s sidebar and all 9 remaining tabs now follow
  `Theme::of(ctx)`** — previously only the Controls tab was theme-reactive;
  switching dark mode or the UI/MD3/iOS design-system switcher had no
  visible effect anywhere else, including the sidebar hosting the switcher
  itself. Backgrounds, cards, captions, and body text across Layout, Text
  & Input, Lists, Animations, Gestures, Clipping & FX, Keyboard, Images,
  and Draw now read theme color roles instead of hardcoded literals; the
  now-unused `kContent` constant was removed. The illustrative accent
  palette (`kBlue`/`kGreen`/`kOrange`/... used for demo variety in avatars,
  chips, transition boxes) is deliberately unchanged, as is `DrawSection`'s
  canvas background (stays paper-white regardless of theme, like a real
  drawing app). Fixed a latent dangling-reference risk while doing this:
  several `ListView`/`GridView`/`DragTarget`/`LayoutBuilder` callbacks in
  the gallery are invoked by the framework on later frames after the
  `build()` that constructed them returns — these now capture the theme's
  `ColorScheme` by value instead of by reference.

- **`DesignSystem` gains 5 new components (wave 3): `ExpansionTile`,
  `ToggleButtons`, `Banner`, `NavigationRail`, `DataTable`** — 35
  pure-virtual builders total, implemented across all 4 concrete systems
  (`NullDesignSystem`, `campello_ui`, `campello_material`, `campello_cupertino`).
  `NavigationRail` reuses `MaterialDesignSystem::buildNavigationBar`'s
  `secondary_container` pill-indicator convention for its selected item;
  `campello_cupertino` documents where HIG has no direct equivalent
  (`NavigationRail`'s closest relative is the split-view sidebar,
  `DataTable`'s is a grouped `UITableView`) and falls back pragmatically.
  Wired into `examples/gallery`'s `ControlsSection` with live state
  (expand/collapse, per-item toggle selection, active rail tab, dismissible
  banner), reactive to the sidebar's UI/MD3/iOS design-system switcher.
  Contract test suite extended with 5 new default-config assertions, 2 new
  disabled-state assertions, and 4 new structural-content tests. 690/690
  tests green (up from 674).

- **`ColorScheme` gains `tertiary`/`on_tertiary` and 4 container role pairs**
  (`primary_container`, `secondary_container`, `tertiary_container`,
  `error_container`, each with an `on_` pair) — 10 new fields, additive and
  backward-compatible. Closes part of the parity gap against real MD3
  (which has ~30 color roles to our previous 23). `campello_material` uses
  the real, publicly documented MD3 baseline palette values (seed
  `#6750A4`); `campello_ui` gets a coral/terracotta tertiary; `campello_cupertino`
  uses `systemPink` as its tertiary (HIG's usual third accent) with literal
  tint values instead of runtime `withOpacity()`; `NullDesignSystem` gets
  flat neutral grays.

### Fixed

- **`FocusManager` never bubbled an unconsumed key event to an ancestor
  `FocusNode`** — `FocusNode::on_key`'s own doc comment already promised
  "return false to let it propagate" (matching Flutter's real `FocusNode`
  behavior), but `FocusManager::handleKeyEvent()` only ever tried the
  single currently-focused leaf node; there was no ancestor chain to walk
  at all (`FocusNode` had no `parent()`). Concretely: a `KeyboardListener`
  wrapping a focused child (e.g. a search bar around a `TextField`) never
  saw a key the child's own `on_key` didn't consume — found via a real
  Escape-doesn't-close-the-search-bar bug in campello_editor. Fixed with
  new `FocusNode::parent()` + `FocusManager::dispatchToFocusChain()`,
  used identically by the Tab, arrow-key, and general-key-routing paths.
  `parent()` resolves **lazily**, on demand, by walking the owning
  `RenderObject`'s `parent()` chain (via a new `RenderObject::
  ownedFocusNode()` virtual, overridden by `RenderFocus`/
  `RenderGestureDetector`/`RenderTextField`/campello_editor's
  `RenderRichTextField`) rather than being cached at `attach()` time — an
  earlier version of this fix cached it eagerly and was itself wrong: a
  render object's `attach()` fires as soon as *it* gets a parent, which
  happens bottom-up while a fresh subtree assembles (e.g.
  `RenderMouseRegion`, which `TextField`/`RichTextField` both build,
  adopts its child and fires the child's `attach()` — and thus the walk —
  *before* `RenderMouseRegion` itself is attached further up), so the walk
  would see a truncated chain and wrongly conclude there was no ancestor.
  Computing it lazily instead — only ever consulted from
  `handleKeyEvent()`, i.e. in response to a real key event, which can only
  happen once the visible tree has long since finished mounting —
  sidesteps the ordering problem entirely.

- **`TextField`/`RichTextField`'s `FocusNode` was never actually linked
  into the ancestor chain above**, even after the bubbling fix — both
  manage their `FocusNode` at the widget-`State` level (registering
  directly with `FocusManager`), completely bypassing the render-tree
  `attach()`/`detach()` lifecycle the bubbling fix relies on to link
  `parent()`. `RenderTextField` (`render_text_field.cpp`) and
  campello_editor's `RenderRichTextField` both gained the same
  `focus_node` field + `ownedFocusNode()` override + `attach()`/`detach()`
  wiring `RenderFocus` already had, threaded through their proxy widgets
  from the owning `State`. Found immediately after the bubbling fix above
  via trace logging (a click-to-place-cursor debug session): the
  ancestor-chain walk stopped dead at the very first hop.

- **Liquid Glass card showed stale/reflected content from elsewhere on the
  page while scrolling** — `buildCard()`'s liquid-glass branch was the
  first place in this codebase wrapping `BackdropFilter` inside
  `ClipRRect`, and `ClipRRect` is itself an independent `OffsetLayer`
  cache boundary. This framework already has a specific safeguard against
  stale `BackdropFilter` caching during scroll
  (`PictureLayer::hasBackdropFilter()` — see its doc comment for the
  matching "blur sampling a stale, frozen backdrop" symptom it was built
  to prevent), but it had only ever been exercised against a *direct*
  `BackdropFilter`, not one nested inside a second, separate caching
  boundary. Fixed by removing the `ClipRRect` — it was never load-bearing:
  `ImageFilter::liquidGlass()`'s shader already self-shapes to the rounded
  rect via its own SDF+alpha, so the extra clip was only guarding against
  child content overflowing the corners, a minor cosmetic edge case not
  worth the untested nested-caching interaction. User-diagnosed (correctly
  identified it as stale/misindexed backdrop sampling) via scroll testing.

- **`buildPrimaryActionButton()` (FAB) had backwards `Align`/`SizedBox`
  nesting in all three concrete `DesignSystem`s** (`campello_ui`,
  `campello_material`, `campello_cupertino`) — a pre-existing bug, not new
  this session, just never previously exercised in a real layout (only
  unit-tested for non-null return). `Align(center, SizedBox::square(56,
  content))` sizes the *outer* `Align` to fill whatever bounded space its
  parent gives it and only positions the already-56×56 box within that —
  it doesn't pin the button to 56×56 itself, so inside a `Row`'s flex
  layout the FAB could expand to claim most of the available space,
  starving sibling `Expanded` children. Found via the gallery's new Card +
  PrimaryActionButton demo row, which rendered as a squished card sliver
  next to a giant stretched FAB pill in *every* design-system kind. Fixed
  by swapping the nesting — `SizedBox::square(56, Align(center, content))`
  — in all three systems identically.

- **Gallery's UI/MD3/iOS theme switcher was unreadable in dark mode** —
  `buildThemeFooter()`'s segment labels ("UI"/"MD3"/"iOS") were built with
  `Text("UI")` and no explicit color, defaulting to plain black.
  `buildSegmentedButton()` doesn't recolor caller-supplied labels, so this
  was invisible-to-low-contrast against the dark unselected track
  (`colors.surface_variant`) once dark mode made that background genuinely
  dark. Each design system also fills its *selected* segment with a
  different role (Campello: `primary`, Material: `secondary_container`,
  Cupertino: `surface` as a floating pill), so no single fixed color would
  have contrasted correctly against all three anyway — the fix picks the
  matching `on_X` role per the active design-system kind and per
  selected/unselected state.

- **Raster-thread `SIGBUS` in `campello_gpu::Buffer::upload()` under heavy
  widget churn** — `MetalDrawBackend::UniformBufferPool::acquire()`
  (`src/gpu/metal/metal_draw_backend.mm`) reused a ring-slot GPU buffer via
  `upload()` on every cache hit without checking it was actually large
  enough for the new request. `rect_vertex_pool_` is shared between
  `drawFilledQuad()`'s fixed 6-vertex draws and `drawFilledVertices()`'s
  variable-length arc/path vertex arrays, so a slot sized for a small draw
  could get overflowed by a later larger one — an out-of-bounds GPU buffer
  write. Now reallocates the slot when `buffers[idx]->getLength() < size`
  instead of blindly reusing an undersized buffer. Previously logged as a
  known, deferred issue in `TODO.md`; root-caused and fixed this session.

- **Three MD3 builders were approximating a container color with
  `withOpacity(primary, 0.15–0.18)` because no container role existed** —
  now use the real MD3-specified role: `MaterialDesignSystem::buildPrimaryActionButton`
  (FAB) now uses `primaryContainer` instead of a solid `primary` fill (a
  real spec detail — Material 3's default FAB is *not* primary-colored);
  `buildChip`'s selected state and `buildSegmentedButton`'s selected
  segment now use `secondaryContainer`/`onSecondaryContainer`;
  `buildNavigationBar`'s active-item indicator pill likewise. `campello_ui`
  and `campello_cupertino`'s `buildChip`/`buildIconButton` selected states
  now read `primary_container` instead of computing an inline tint per
  call site.

- **Design system decoupling** (TODO.md Phase 16) — following Flutter 3.47's
  split of Material/Cupertino into standalone packages, `campello_widgets`
  core now ships with zero baked-in visual style. Three new sibling
  libraries each implement the existing `DesignSystem` abstract interface
  (`inc/campello_widgets/ui/design_system.hpp`), linking against
  `campello_widgets` and never the reverse:
  - **`campello_ui`** — the existing bespoke "warm teal" `CampelloDesignSystem`
    (unchanged visually), relocated out of core; `campello_editor`'s design.
  - **`campello_material`** — new `MaterialDesignSystem`, MD3-authentic:
    baseline tonal palette (seed `#6750A4`), MD3 type scale, MD3 shape scale
    (4/8/12/16/28dp), stadium-shaped filled buttons, a 16dp-radius FAB
    (MD3's real departure from Material 2's circular FAB), and a
    connected-button-group `SegmentedButton` with a stadium outer border.
  - **`campello_cupertino`** — new `CupertinoDesignSystem`, HIG-authentic:
    iOS system colors, Dynamic Type sizes, filled/tinted/plain/destructive
    button styles at a 14pt corner, `UISwitch`'s real green active track
    with an always-white thumb, a `UIAlertController`-style narrow
    (270pt) dialog with centered text and divided action buttons, and
    `UISegmentedControl`'s real sliding-white-pill `SegmentedButton`.
  - `NullDesignSystem` (new, in core) — a flat, unstyled fallback; it's
    `Theme::of()`'s no-`Theme`-found default now that core can no longer
    depend on `campello_ui`.
- **`DesignSystem` interface expanded from 19 to 30 components**, in two
  waves, implemented across all four concrete systems in lockstep (no
  partial implementations): `Chip`, `SegmentedButton`, `BottomSheet`,
  `Badge`, `IconButton` (wave 1); `Stepper`, `RatingIndicator`,
  `ActionSheet`, `SearchField`, `DatePicker`, `TimePicker` (wave 2 — date/
  time pickers are tappable trigger fields, not calendar/wheel widgets,
  matching how `Dialog`/`BottomSheet`/`PopupMenuButton` already separate
  chrome from presentation).
- **`examples/macos_showcase`'s Theme tab** rebuilt into a live design-system
  gallery: a 3-way switcher (`campello_ui`/`campello_material`/
  `campello_cupertino`, built with the new `SegmentedButton`) plus the
  existing light/dark toggle, exercising all 30 `DesignSystem` builders via
  `Theme::of(ctx)`.
- **Parameterized abstract-contract test suite**
  (`tests/design_system_contract/`) — the same assertions (all 30 builders
  return non-null, token scales are internally consistent, `on_X` colors
  differ from their base `X`) run against all four concrete `DesignSystem`
  implementations through the abstract base pointer.

### Known Issues

- **`SIGSEGV` in Metal's `presentDrawable:` completion handler under heavy
  widget churn** — found via the gallery's tab-switch + design-system-switch
  stress test, distinct from the `UniformBufferPool` `SIGBUS` fixed above
  (different subsystem, different stack: Metal itself calling
  `presentWithOptions:` on an already-deallocated `CAMetalDrawable` from its
  own internal completion queue). Investigated: the drawable's retain/
  release accounting across `run_app.mm` and `campello_gpu`'s
  `Device::scheduleNextPresent()`/`submit()` balances correctly by
  inspection, matching Apple's documented `presentDrawable:` contract — no
  proven code defect found (unlike the `UniformBufferPool` bug, which had a
  self-documented broken invariant). `RasterThread`'s depth-1 pipelining
  (UI thread can request a new `currentDrawable` while the previous frame
  is still presenting) is a plausible contributing factor under rapid
  churn, not a confirmed cause. Reproduced once on a 2018 Intel Mac mini
  (UHD 630); has not reproduced since. Postponed at the user's direction —
  pinning it down needs live Metal tooling (`NSZombieEnabled`/
  `MTL_DEBUG_LAYER`, see `TODO.md`), not further static analysis.

### Fixed

- **`InheritedElement::notifyDependents()` could call `markNeedsBuild()` on
  an already-destroyed `Element`** — a heap-use-after-free, found by the
  design-system switcher above (the first scenario where an `InheritedWidget`
  changes value in the same rebuild pass that also unmounts structurally
  different widgets elsewhere in the tree; toggling light/dark alone never
  unmounted anything, so this was unreachable before). `dependents_` was
  only ever cleared when the `InheritedElement` itself unmounted, never when
  an individual dependent did. Not reproducible under a plain debugger
  (classic use-after-free timing sensitivity); reproduced deterministically
  under AddressSanitizer. Fixed in `src/widgets/inherited_element.cpp` by
  pruning stale entries via the framework's existing `Element::isAlive()`
  liveness registry before calling `markNeedsBuild()`.

### Known Issues

- **Raster-thread `SIGBUS` in `campello_gpu::Buffer::upload()` under heavy
  widget churn** — found via the same stress test as the fix above, but
  confirmed distinct from it (still reproduces after that fix). Needs a
  specific sequence (switch tabs away and back, then switch design system
  repeatedly) to reproduce; lives in `campello_gpu`'s buffer pool /
  raster-thread synchronization, not in `campello_widgets`' Element tree.
  See `TODO.md`'s `## Backlog / Future` for the full repro steps and stack
  trace; not yet fixed.

### Added

- **Liquid Glass rollout completed for `CupertinoDesignSystem::liquidGlass()`**
  — `PopupMenuButton`, `DropdownButton`, `Dialog`, `ActionSheet`, and
  `Tooltip` all now render as frosted glass panels in Glass mode, joining
  `Card`/`PrimaryActionButton` from the initial wiring. `PopupMenuButton`/
  `DropdownButton`/`Dialog`/`Tooltip` each gained a `backdrop_filter`
  (`std::optional<ImageFilter>`) field on the core widget itself (they own
  their own overlay/dismiss lifecycle, unlike `Card`, so the design system
  can't hand-roll the composition the way it does there); `ActionSheet`
  needed no core change, composing entirely inside
  `CupertinoDesignSystem::buildActionSheet()`. All five use the same
  shadow-`DecoratedBox`-wrapping-`BackdropFilter` composition established
  by `Card` — no `ClipRRect`, now a *confirmed* (not just suspected)
  incompatibility, see Fixed below. Every glass-wired surface has a new
  demo in `examples/gallery`'s Controls tab. See `TODO.md`'s Liquid Glass
  entry for the full per-widget history.
- **`TextStyle::tight_vertical_bounds`** — opt-in (default off, no-op on
  multi-line text), sizes/positions a single-line `Text` by its tight
  glyph-ink bounds instead of the full typographic
  `ascent + descent + leading` box, so centering a short UI label (button
  text, ...) centers on the visible ink rather than font-metric padding
  the string doesn't use. Backed by a new
  `IDrawBackend::measureTextInkBounds()` (default: untightened box,
  graceful fallback for backends without a native tight-bounds query;
  implemented for Metal via CoreText's `CTLineGetBoundsWithOptions
  (kCTLineBoundsUseGlyphPathBounds)`). Enabled in `examples/gallery`'s
  `ts()` text-style helper.

### Fixed

- **`OffsetLayer::record()` never evicted its own stale GPU replay-cache
  entries on a second full record** — `Renderer::evictReplayCacheEntries()`
  was only ever called from `OffsetLayer`'s destructor (guarding against a
  *different* instance reusing a freed address), not when the *same*
  instance records fresh content after an earlier record. Any node whose
  `OffsetLayer` records more than once in its lifetime (e.g. a `Card`
  settling from an intermediate layout into its final position) could
  leave a stale cached shadow/clip-shape/shader-mask/save-layer GPU
  composite — keyed only by `(pointer, encounter-order index)`, with no
  way to detect the underlying picture changed — sitting around for a
  future identity-replay to reuse unquestioningly. Visible as a hard-
  edged, unblurred shadow frozen at an early frame's position while the
  card itself repainted correctly. Fixed by calling
  `evictReplayCacheEntries(this)` unconditionally at the start of
  `record()`, not just in the destructor.
- **`ClipRRect` wrapping a `BackdropFilter` reads content from the wrong
  part of the screen** — confirmed, not just avoided: `Renderer::
  applyClipShape()` renders a `ClipRRect`'s subtree into a separate,
  small, locally-offset offscreen texture via a nested `flushDrawList()`
  pass; a `BackdropFilter` inside that subtree samples the single
  full-window backdrop capture using UVs computed for that local viewport,
  not the real on-screen position, so it shows whatever's near the
  window's origin instead of what's actually behind it. This is also the
  real explanation for the earlier "reflecting toggle buttons while
  scrolling" bug (previously attributed, incompletely, to an untested
  caching interaction) — `buildCard()`'s existing lack of `ClipRRect` was
  already the correct fix, for the correct reason, confirmed by reading
  `applyClipShape()` directly. Documented as permanent in
  `buildCard()`'s doc comment so it isn't re-attempted blind.
- **`RenderGestureDetector::globalOffset()` ignored ambient scroll
  transforms** — used by `DropdownButton`/`PopupMenuButton` to anchor
  their overlay menu to the trigger button's on-screen position, computed
  purely from the paint-time logical `offset` (pre-scroll tree position);
  a scroll view's delta is applied separately, only as an ambient
  `Canvas::translate()` at paint time. Any trigger inside a
  `SingleChildScrollView` reported its pre-scroll position, so the menu
  opened progressively more mis-positioned the further the list had
  scrolled. Fixed by projecting through `Canvas::currentTransform()`
  (`projectedBounds()`), matching the same pattern already used by
  `RenderBackdropFilter`/`OffsetLayer` for this exact class of bug.
- **`PopupMenuButton`'s menu always opened at the screen's top-right**,
  never actually anchored to its trigger (hardcoded
  `Align(Alignment::topRight())`) — unlike `DropdownButton`, which was
  already anchor-aware. Gave it the same `GlobalKey` + positioner
  mechanism `DropdownButton` uses.
- **`Dialog::border_radius`/`elevation` were dead fields** — `Dialog::
  build()` was a bare `Container` with only `background_color` applied; the
  other two fields, set by every design system, were silently ignored (the
  code even said so in a comment). Now composes a real `DecoratedBox` with
  rounded corners and a shadow.
- **`CupertinoDesignSystem::buildDialog()`'s 1–2-action row could render as
  a near-full-window blank sheet with no visible buttons** — three
  compounding, pre-existing bugs, only ever exercised once `Dialog`'s
  dead-field gap above was fixed: (1) `Align(Alignment::center(), ...)`
  without `height_factor` fills all available height rather than
  shrink-wrapping to its child — fine under a bounded parent, but
  `showDialog()`'s `Center` only loosens the incoming constraints rather
  than bounding them, so the dialog's title/content `Align`s claimed
  nearly the whole window height before the actions row was ever reached;
  (2) the actions row's `cross_axis_alignment::stretch` (needed so the
  vertical divider between two side-by-side actions spans the row) then
  inherited that same huge loose budget as its own reported height; (3)
  the first fix for (2) — a fixed 44pt row height, with each action
  `Center`-wrapped for horizontal alignment — manufactured artificial
  slack space around the (naturally shorter) buttons, and a small residual
  text-baseline offset became visible specifically *within that slack*,
  initially misdiagnosed as a text-rendering problem (see the ink-bounds
  entry above, a real improvement but not what fixed this). Correctly
  root-caused by comparing against `MaterialDesignSystem`'s action row,
  which has never shown this — no `stretch`, no artificial height, so no
  slack for anything to be visible within. Final fix: dropped `stretch`
  and the fixed height entirely, `cross_axis_alignment::center` instead
  (row sizes to its tallest child), each action `Center`-wrapped with
  `height_factor = 1.0` (horizontal-only centering, without reinheriting
  bug 2's loose bound), and an explicit fixed height on the divider (which
  has no natural height of its own once `stretch` isn't sizing it).
  `CupertinoDesignSystem::buildActionSheet()` had the identical
  `Align`-without-`height_factor` hazard in its title/action labels;
  fixed proactively before it could manifest there too.

### Added

- **Video playback**, landing platform-by-platform: macOS first (`VideoPlayerController`/`VideoPlayer`/`RenderVideoPlayer`, modeled on `ScrollController`/`AnimationController`'s existing shapes — CPU-decode via `AVPlayer`+`AVPlayerItemVideoOutput` (BGRA), each frame copied into a persistent `campello_gpu::Texture` via the same `createDedicatedOffscreenTexture()` primitive `RenderDrawSurface` already used; not zero-copy, `campello_gpu` has no "wrap an external texture" API yet). End-of-playback is detected via `AVPlayerItemDidPlayToEndTimeNotification` (previously nothing noticed playback had ended, so the Play/Pause button stuck on "Pause" and the app redrew every tick indefinitely). Then ported to iOS (the AVFoundation backend is identical on both platforms — moved `src/macos/video_player_controller.mm` to `src/avfoundation/`, added the needed frameworks to `ios.cmake`), and to Android via a fully native `AMediaExtractor`/`AMediaCodec` decode path with no JNI or custom `Activity` — live testing on real hardware found the Android emulator's software decoder intermittently corrupts frames regardless of output mechanism, and Qualcomm hardware decoders hand back UBWC-tiled buffers `AImageReader` can't read as linear YUV even with CPU-read usage requested, so the final implementation uses `MediaCodec` byte-buffer decode with no output `Surface` at all, verified clean at full framerate on a real device. The gallery's Video tab (`BoxFit::contain`, letterboxed — an earlier `cover` default cropped landscape clips on portrait phone screens) overlays Play/Pause + position controls via `ds->buildCard()`, exercising Liquid Glass refraction against real moving video content rather than only a static test pattern. Also got the iOS gallery flavor building and launching for the first time end-to-end (linked the split-out `campello_ui`/`campello_material`/`campello_cupertino` libraries and `AVFoundation`/`CoreMedia`/`CoreVideo`, bundled the sample clip as a real app resource resolved via `NSBundle`, and fixed a real Unity Build bug where `main.mm` was silently compiled as plain C++ instead of Objective-C++ until it needed real Objective-C syntax) — verified installed and running on a booted iPhone 17 Pro Simulator via `xcrun simctl`.

- **Cross-platform visual fidelity testing framework**, replacing the old `GpuVisualRenderer` with a leaner offscreen-draw-backend capture path (`src/testing/offscreen_draw_backend*`) shared by the universal test suite and a new standalone harness (`tests/visual_fidelity/themed_component_harness.cpp`). Captures real platform chrome to diff against instead of hand-mocked references: real `UIAlertController`/`UITabBar`/`UISplitViewController` sidebar per iOS version (`ios_fidelity_reference/`, distinguishing classic pre-iOS-26 chrome from Liquid Glass), and a new Jetpack Compose reference app capturing real Material 3 Expressive components on an Android 16 emulator (`android_fidelity_reference/`), diffed against new `MaterialDesignSystem::expressiveLight()`/`expressiveDark()` presets. Also lands the first `campello_fluent` `DesignSystem` implementation (solid-material phase; Mica/Acrylic deferred to a rendering-pipeline follow-up) and a research proposal for extending `DesignTokens`/`DesignSystem` to fully cover Fluent 2 and M3 Expressive without breaking changes (`memory/theme_abstraction_redefinition_proposal.md`). Coverage was then extended real-capture-builder by real-capture-builder across both platforms until essentially every `DesignSystem` component had a validated real-device reference (iOS: dialog, actionSheet, searchField, navigationBar, navigationRail via a real iPad sidebar; Android: dialog, tabBar, dropdownButton, toggleButtons, popupMenuButton, navigationBar, badge, iconButton, navigationRail, listTile, appBar, primaryActionButton, bottomSheet, stepper, ratingIndicator, actionSheet, searchField, datePicker, timePicker, expansionTile, banner, dataTable) — see Fixed below for the real bugs this surfaced. Both reference backdrops were also unified: Android's captures switched from a flat `colorScheme.background` fill to the same colorful `liquid_glass_background.png` iOS already used, since a uniform backdrop hides translucency/scrim/blur blending bugs that only show up against a non-flat background.

- **`DrawTintedImageCmd`/`drawTintedImage()`** — a new Canvas primitive across all three GPU backends (Metal, Vulkan, D3D12 — D3D12 mechanically mirrored but unverified, no Windows toolchain available at the time) that recolors a "template" texture to an arbitrary runtime tint by sampling only its alpha channel, the same mechanism as iOS's `UIImage.withRenderingMode(.alwaysTemplate)` and Android's icon tinting — one monochrome icon asset serves any theme color with no new GPU work per color. Backs a new **`Icon` widget** (wraps `RawImage`, which gained an optional `color` tint field), closing a long-standing gap: `campello_widgets` previously had no real icon rendering at all, only a "★" placeholder glyph in the fidelity test harness. Core has no opinion on which icon set a glyph comes from (SF Symbols and Material Symbols differ visually and by license) — `DesignSystem` implementations own their own semantic name → texture resolution. Real assets were sourced for all 9 icon names the current Cupertino/Material builders need (house, magnifyingglass, person, bell, heart, star, chevron.left, gear, plus): SF Symbols via a small AppKit command-line capture tool, Material Symbols via the Android fidelity capture pipeline.

- **Windows platform integration completed: DPI awareness, Media Foundation video playback, and a Fluent 2 visual fidelity pipeline.** DPI: the process wasn't declared per-monitor DPI aware, so Windows applied bitmap-upscaling virtualization instead of native-resolution rendering on 2x displays — fixed via `SetProcessDpiAwarenessContext(..._PER_MONITOR_AWARE_V2)`; fixing that then exposed a 2x mouse-coordinate offset (`WM_MOUSEMOVE`/`WM_*BUTTON*`/`WM_MOUSEWHEEL` were passing raw physical pixels straight through), fixed with a `windowDpr()` helper dividing every pointer event's x/y. Video: `VideoPlayerController`'s Windows backend (previously entirely unimplemented — the gallery's play button did nothing) now decodes via `IMFSourceReader`, with a one-sample-lookahead pacing model and a bounded per-tick catch-up loop (max 8 frames/tick) to stay in sync with wall-clock playback time; frames are packed RGB32→BGRA8 via `IMF2DBuffer2::Lock2DSize` with signed-pitch-aware row copying. `campello_fluent` (`campello_fluent/`) is wired into the root build for the first time (its own `CMakeLists.txt` was never actually reached by `add_subdirectory()` before) and every remaining placeholder builder (23 of 30) is implemented — `appBar`, `navigationBar`, `dialog`, `snackBar`, `popupMenuButton`, `dropdownButton`, `tabBar`, `chip`, `segmentedButton`, `bottomSheet`, `badge`, `iconButton`, `stepper`, `ratingIndicator`, `actionSheet`, `searchField`, `datePicker`/`timePicker`, `expansionTile`, `toggleButtons`, `banner`, `navigationRail`, `dataTable` — verified against a native WinUI 3 reference app, fixing several real layout bugs along the way (`Align`/`Center` with no `height_factor` inflating `buildBottomSheet()`/`buildExpansionTile()` to fill the canvas; `buildTextField()` painting a duplicate inner chrome box and skipping the disabled-state fade; `buildCard()` ignoring `CardPriority` entirely; a dark-theme elevated-surface color darker than the page background, backwards from Fluent 2's elevation convention). `tests/visual_fidelity/` extends the existing iOS/Android pipeline to Windows as a third platform (`compare_windows_cpp.py`, a `windows_fidelity_reference/` WinUI 3 batch-exporter), plus a new `surfaceText()` harness helper (no `DefaultTextStyle`/ambient-`Theme` propagation exists in this codebase, so builders drawing arbitrary passed-in content over their own surface — `Card`/`DataTable`/`ListTile`/`Dialog`/`ActionSheet`/`BottomSheet`/`ExpansionTile`/`Banner` — were rendering nearly-invisible black-on-dark text under any dark theme). Final measured diff: `fluent_light`/`fluent_dark` (57 cases each) dropped from double digits to 6.26%/13.71% highest real diff, most cases under 5%.

- **`Clipboard::setText()`/`getText()`** — cross-platform, real implementations on macOS (`NSPasteboard`), Windows (Win32 clipboard), Android (JNI to `ClipboardManager`), and Linux (X11 `CLIPBOARD` selection ownership via a dedicated background thread, plus event-loop-integrated Wayland `wl_data_device_manager` support, runtime-dispatched via a new `isRunningUnderWayland()` flag); iOS stubbed pending `UIPasteboard` support. Wired to Cmd+C/Cmd+X/Cmd+V, blocked on `obscure_text` fields the same way native password fields never expose their contents. Landed alongside **`TextEditingController` undo/redo** — a bounded (100-entry), time-coalesced (500ms) snapshot stack (consecutive typing merges into one undo step, matching Flutter's `EditableText` edit grouping; snapshots capture text + selection together so undo restores cursor position too), wired to Cmd+Z/Cmd+Shift+Z. Both features add a `TextEditingController::focused()` registry (synced by `TextField`'s `FocusNode` focus-change callback) so app-level Edit-menu commands — OS-level menu key equivalents that never reach a focused widget's own key handler — can route to an in-progress text edit before falling back to document-level behavior.

- **Hover cursor support for interactive widgets** — `MouseRegion`/`SystemMouseCursor` existed as a fully cross-platform primitive already, but no widget actually used it. Wired a pointer cursor into `Button`, `PrimaryActionButton`, `ListTile` (all delegate to `Theme::of(ctx)->buildXxx()`, so wrapping the delegated result once covers Material/Cupertino/Fluent/UI uniformly), `Checkbox`, `Radio`, `Switch`, `Slider`, `TabBar`, `DropdownButton`, and `PopupMenuButton` — only shown while actually interactive (has a callback / enabled), matching each widget's existing disabled-state handling — plus a text cursor (I-beam) for `TextField`.

- **Press/hover/focus feedback for Material, Cupertino, and Fluent buttons** — new `GestureDetector` press-state and keyboard-focus plumbing (`on_press_change`, `focus_node`/`autofocus`/`focusable`, Space/Enter activation, Tab-driven `FocusHighlightMode` to suppress focus rings on mouse clicks), used to replace each theme's bare `GestureDetector`+`Opacity` with real interactive chrome: Material gets an InkWell-style state-layer overlay + ripple (`MaterialInkResponse`), Fluent gets a Reveal Highlight border-glow+tint (`FluentRevealResponse`), Cupertino gets a focus ring (`CupertinoFocusRing`) matching `CupertinoButton`'s real focused-state border. `ButtonConfig` gains `focus_node`/`autofocus` for programmatic focus control.

- **D-pad/TV spatial focus navigation** — `FocusManager` supports directional (arrow-key) focus movement, not just Tab/Shift+Tab linear order: jumps to the nearest focusable node in the pressed direction using each node's real on-screen bounds (populated every paint by `RenderFocus`/`RenderGestureDetector`), weighting cross-axis misalignment so a same-row/column candidate beats an off-axis one that's technically closer; unlike Tab, it doesn't wrap. A focused control's own `on_key` handler still gets first refusal (TextField cursor movement, Slider value changes) before the fallback. Android's D-pad center/OK button now maps to Enter. The gallery's sidebar nav items are now focusable with a keyboard/D-pad highlight, previously plain tap-only.

- **GDK (Xbox/Gaming.Desktop.x64) app-lifecycle backend** (`src/gdk/run_app.cpp`) — derived from the Win32 backend: `XGameRuntimeInitialize`/`Uninitialize` bootstrap, PLM suspend/resume via `RegisterAppStateChangeNotification` (matched against a real Microsoft sample), a continuous Present-paced render loop replacing the desktop backend's `DwmFlush`-driven vsync thread. New `CAMPELLO_GDK_GAMING_DESKTOP` CMake option and `gdk.cmake` make this repo independently GDK-buildable (own `GameDKCoreLatest`/`GRDKLatest` discovery, sets `WINAPI_FAMILY=WINAPI_FAMILY_GAMES` directly); every other platform's `CMakeLists.txt` now excludes `src/gdk/` from its `GLOB_RECURSE`. New `windows-gdk` CI job installs the public Microsoft GDK via `winget` for a build-only compile check (no Xbox partner/NDA access needed for CI). Input still routes through Win32 messages rather than `campello_input`'s GDK GameInput layer — left as a follow-up.

- **`Renderer` gains a `clear_target` constructor flag** (default `true`) — switches the main pass's color attachment between `LoadOp::clear`/`LoadOp::load`, so a widget tree can be drawn on top of content another renderer already wrote into the same texture this frame (sprites, HUD, any 2D overlay) instead of wiping it. Also adds a public `createDrawBackend()` factory — `MetalDrawBackend` was previously only reachable through a private header, so a standalone `Renderer` consumer (no App/Window bootstrap) had no way to attach a working `IDrawBackend` and `rasterFrame()` silently no-op'd.

- **`Offstage` widget** — keeps a hidden child's `Element`/`State` mounted (like `Opacity(0, child)` does) but skips paint and layout entirely: `performLayout()` reports zero size without laying the child out, `paint()` never visits the child's subtree. Matches Flutter's `Offstage`. Fixes a real correctness bug found via campello_editor (an inactive tab's content bleeding through the active one): `RenderOpacity` always paints its child at alpha 0 every frame regardless, and a descendant `RenderRepaintBoundary` that decides to replay its cache instead of repainting can bypass the ambient opacity multiplier entirely, painting stale full-alpha content through what should be invisible.

- **`vector_math` dependency bumped to v0.6.0** — v0.5.0 (additive: AVX2 batch-transform paths, runtime CPU-feature helpers, ARM NEON platform guard) plus v0.6.0's fix for `Matrix4::lookAt()`'s right-axis computation (reversed operand order from the standard right-handed convention); flagged upstream as breaking for camera-orientation callers, a no-op change here since `campello_widgets` never calls `lookAt()`.

- **GPU-based stroke caps/joins/miter-limit geometry, and real GPU-side rendering of them** — `Paint.stroke_cap`/`stroke_join`/`stroke_miter_limit` mirror Flutter's `StrokeCap`/`StrokeJoin`/`strokeMiterLimit`. Since this library targets potentially very dense scenes, cap/join expansion runs on the GPU rather than via CPU tessellation: the CPU side does only O(1) work per segment/join, via new shared, backend-agnostic `src/gpu/stroke_geometry.{hpp,cpp}` (`buildStrokeGeometry()`), decomposing a polyline into segments, round-cap/round-join circles, and bevel/miter join wedges — independently unit-tested (12 cases: exact miter-point math, the miter-limit-to-bevel fallback, closed-shape join/cap counts). The GPU rendering side (Metal, Vulkan, D3D12) reuses existing primitives almost entirely: a round cap/join is a filled circle via the existing SDF circle pipeline, a bevel/miter join is 1–2 flat triangles via the existing flat-triangle-batch pipeline. The one genuinely new piece: the line pipeline's fragment shader upgraded from a flat passthrough to an antialiased rotated-box SDF, which also gives every stroked line/rect/path real antialiasing for the first time — `drawRect()`'s stroke branch now routes through the same shared polyline path as `drawLine()`, incidentally fixing it not rendering correctly under rotation (previously 4 always-axis-aligned overlapping rects), and `PointMode::polygon` (`Canvas.drawPoints`) now gets real joins at every vertex including the closing corner instead of independent unjoined segments. Metal implemented, compiled, and visually verified via a gallery demo (every cap/join/miter-limit-fallback/rotation case confirmed correct and antialiased); Vulkan/D3D mirrored from the verified Metal implementation, diff-reviewed line-by-line in lieu of a local compiler for either platform. Gallery gained STROKE CAPS / STROKE JOINS / STROKE MITER LIMIT + ROTATION showcase rows.

- **`Paint.filterQuality` / `Paint.invertColors`** — the two cheapest gaps against Flutter's `Paint` API. `invert_colors` flips a solid color once at record time (`Canvas::resolvePaint()`), correct and free for analytic shape fills/strokes since inverting a flat color commutes with rasterization; doesn't apply to `drawImage()`/`drawTintedImage()`/`saveLayer()`, which would need real per-pixel GPU support. `filter_quality` selects nearest-vs-bilinear sampling, threaded through as a direct parameter on `drawImage()`/`drawTintedImage()` (the only draws that currently sample a texture) rather than via `Paint`. GPU backends wire `FilterQuality::none` to a dedicated nearest-neighbor sampler alongside the existing linear default.

- **`Paint.shader`: general gradient fills for any `Canvas` draw call** — mirrors (a subset of) Flutter's `Paint.shader`. Previously gradients only worked through `BoxDecoration`'s own `beginShaderMask()`/`endShaderMask()` wrapper; a `RawCustomPaint` user had no way to do the equivalent of `canvas.drawCircle(c, r, paintWithGradient)` directly. `Paint` gains an `std::optional<Shader>` field; `Canvas::drawRect`/`drawCircle`/`drawOval`/`drawRRect`/`drawPath`/`drawArc`/`drawLine` now check it and, when set, wrap the draw in a `beginShaderMask()`/`endShaderMask()` scope automatically — the exact technique `BoxDecoration`'s own gradient fill/border already use, so no backend changes were needed. A stroked shape's capture region is grown by `stroke_width/2` so the outer half of the stroke isn't clipped, with the shader's `Offset` coordinates shifted by the same amount to stay anchored to the shape's unstroked bounds. Not yet supported by `drawDRRect()` or `drawPoints()` (documented gap, falls back to color). Gallery gained a PAINT.SHADER showcase (gradient circle/star-path/stroked-rounded-rect via direct `Canvas` calls) — screenshot-verified: gradients render correctly, stroked ring's outer edge not clipped.

- **`drawPath()` fill antialiasing** — the last shape primitive without antialiasing (circle/oval/rrect already use an SDF fragment shader, every stroked primitive is now SDF- or geometry-antialiased, but fills were still ear-clip triangulated into flat, jagged-silhouette triangles). New shared, backend-agnostic `src/gpu/path_fill_aa.{hpp,cpp}` (`buildFillAASkirt()`) computes a thin outward-only "feather" band of triangles along a filled contour's boundary, with per-vertex alpha fading 1.0→0.0 a fraction of a pixel out — the GPU rasterizer's own triangle interpolation does the actual antialiasing, no SDF or new fragment-shader math needed, and since the skirt sits entirely outside the opaque interior (zero overlap) it's safe for semi-transparent paints and non-`srcOver` blend modes too. Independently unit-tested (7 cases: degenerate contours, closing-duplicate tolerance, exact 90°-corner miter math, winding-invariance of the extrusion direction). GPU rendering adds one new minimal pipeline per backend ("rect, but with a per-vertex alpha" — Metal's `rectAAVertex`/`rectAAFragment`, Vulkan's `rect_aa.vert/frag`, D3D's `rect_aa.hlsl`) rather than growing the existing flat-fill vertex types used unchanged by ~7 other call sites; the fill's interior triangulation stays one draw call, the skirt renders in a second, preserving the "two draw calls regardless of point/contour count" dense-scene property established by the stroke work above. Metal: implemented, compiled, and screenshot-verified (smooth, non-jagged silhouettes). Vulkan: shader-compiled via `glslangValidator`, then actually built and run end-to-end on a physical Android device (NDK 27, all 4 ABIs) — the first real, non-diff-review-only verification of this session's Vulkan work, screenshots confirming pixel-correct rendering matching the macOS/Metal reference. D3D: HLSL written and diff-reviewed; introduces new shader entry points not present in the committed header, so the D3D backend needs `build_dx12_shaders.bat` re-run on a Windows machine before it compiles. Gallery gained a DRAWPATH FILL ANTIALIASING showcase (a many-angled star and a shallow diagonal edge).

- **`HardwareKeyboard`: live modifier-key state, Flutter-parity** — mirrors Flutter's `HardwareKeyboard.instance`, a global "what modifier keys are currently held" query checked from inside a pointer/gesture handler to detect Cmd+Click and similar combinations (`PointerEvent` deliberately still carries no modifier field, matching Flutter's own `PointerEvent`). `HardwareKeyboard::current()` is kept live by `FocusManager::handleKeyEvent()`; scoped to the coarse `KeyModifiers` bitmask (shift/ctrl/alt/meta), not per-key left/right state, since discrete modifier `KeyCode`s are unreliably populated across platforms (macOS never emits one for a bare modifier press; Windows never maps Cmd/Win to anything but `KeyCode::unknown`) — matches the surface Flutter's own `isShiftPressed`/etc. getters expose. Required a real platform fix to actually work: AppKit has no `keyDown:`/`keyUp:` for a *bare* modifier press, only `flagsChanged:`, which this codebase had no handler for at all — added to macOS's `CampelloMTKView`. Windows/GDK have the analogous gap (`WM_KEYDOWN` deliberately excludes `VK_SHIFT`/`VK_CONTROL`/`VK_MENU`) — fixed the same way, diff-reviewed only (no Windows machine available). Gallery gained a HARDWARE KEYBOARD demo in the Gestures section, manually verified (plain clicks and bare-Cmd-held-then-click both detected correctly).

- **`Paint.colorFilter` and `Paint.maskFilter` (blur)** — two more Canvas/Paint parity gaps. `colorFilter`, scoped to Flutter's `ColorFilter.mode(color, blendMode)` (not the general 4×5 matrix), turns out to be entirely CPU-resolvable: for any non-shader, non-image draw the Porter-Duff destination is just `paint.color`, a single known value at record time, resolved via a new `blendColors()` helper implementing the standard Porter-Duff Fa/Fb formulas for all 13 existing `BlendMode` values — zero GPU/shader changes, exact everywhere in a shape's opaque interior, an approximation only at antialiased edge pixels for a subset of blend modes. `maskFilter`, scoped to `MaskFilter.blur(BlurStyle.normal, sigma)` on `drawRect()`/`drawCircle()`/`drawOval()`/`drawRRect()`, is a close structural twin of the existing box-shadow path (`Renderer::applyBoxShadow()`): new `Renderer::applyMaskFilterBlur()` mirrors its padding/downsample/offscreen-pass/blur/composite sequence almost verbatim, including the same GPU replay-cache mechanism, generalized via a `draw_shape` callback instead of always filling an RRect with a shadow color. Gallery gained PAINT.COLOR_FILTER (srcIn icon-tint style, modulate) and PAINT.MASK_BLUR_SIGMA showcases — screenshot-verified.

- **`Canvas.drawImageNine()` — 9-patch/nine-slice image drawing** — pure CPU geometry on top of the existing `drawImage()`: splits a texture into 9 patches around a source-pixel `center` rect, corners kept at native size and clamped to half the destination extent so opposing corners never overlap. Geometry math lives in its own `src/ui/nine_patch_geometry.hpp/.cpp`, unit-testable without a GPU-backed `Texture`. Gallery gained a drawImageNine section in the Images tab, screenshot-verified (naive stretch vs. corners-preserved vs. a narrow-dst corner-clamp case).

- **`Canvas.drawVertices()`, `Canvas.drawAtlas()`, and `Path.fillType()`, with a follow-up MSAA antialiasing pass for the fill** — three Canvas/Path parity gaps: `drawVertices()` is an arbitrary triangle mesh with per-vertex color blended with `Paint.color` via `BlendMode`, resolved once per vertex on the CPU (`buildTriangleListVertices()`), with a new per-vertex-color GPU pipeline in all three backends (Metal/Vulkan verified on real hardware; D3D structurally complete, not compiled). `drawAtlas()` is batched sprite drawing via `RSTransform`, implemented as a `save()`/`transform()`/`drawImage()`/`restore()` loop per sprite — pure CPU geometry, no new GPU/shader code (per-sprite tinting/`cullRect` out of scope, would need a real instanced pipeline). `Path.fillType()` (nonZero/evenOdd) fixes the ear-clip-then-concatenate fill every `DrawPathCmd` used regardless of fill type, which previously double-filled overlapping/nested contours (e.g. a donut's outer+inner circle) instead of subtracting a hole — multi-contour fills now route through a real stencil-then-cover GPU technique (`Renderer::applyPathFillWinding()`: write each contour's triangles into a stencil buffer, incrementWrap/decrementWrap per face for nonZero or invert for evenOdd, then cover gated by a stencil test), replay-cached like the shadow/mask-blur caches (without it, unpooled texture churn caused intermittent single-frame corruption on at least one Vulkan/Android driver during continuous scroll repaint). This work exposed two real bugs in `campello_gpu`'s own stencil support (Metal never wired `stencilFront`/`stencilBack`; Vulkan's `StencilOp` enum didn't numerically match `VkStencilOp`), fixed upstream separately. A same-session follow-up then antialiased the stencil-then-cover fill itself — previously hard-edged, unlike every other fill in this codebase — by rendering the write/cover passes into a 4×-multisampled color+stencil pair on both Metal and Vulkan and resolving to the single-sample texture actually composited, using `campello_gpu`'s new `RenderPipelineDescriptor.sampleCount` + `ColorAttachment.resolveTarget`. All verified via gallery demos, screenshot-checked on macOS/Metal; drawVertices and fillType additionally verified on a physical Android/Vulkan device, including a scroll-stress pass showing no crash or corruption. 585/585 tests passing throughout.

- **19 bounded Core Widgets parity gaps** (everything scoped as "bounded" — not one of the three structural gaps Slivers/Semantics/Notification-bubbling), implemented in four waves after matching each item to the closest existing pattern: **Wave 1** (trivial, zero new infra) — `Spacer`, `AnimatedPadding`, `DefaultTextStyle`, `TweenAnimationBuilder<T>`, `Visibility`, `IgnorePointer`, `AbsorbPointer`, `Listener`. **Wave 2** (composition of existing patterns) — `FittedBox` (reuses `RenderImage`'s `BoxFit` scale math via a canvas transform), `OverflowBox`/`UnconstrainedBox` (shared `RenderConstraintsTransformBox`, mirroring Flutter's own internal unification). **Wave 3** (small self-contained new infra) — `Baseline` (new `RenderBox::computeDistanceToActualBaseline()` virtual), `Flow` (new `FlowDelegate` abstract base + per-child paint-time transforms), `CustomMultiChildLayout` (new `MultiChildLayoutDelegate` + `LayoutId`), `IndexedStack` (small `RenderStack` accessor addition, subclasses `Stack` directly), `Shortcuts` (new `KeyCombo` struct on the existing key-bubbling mechanism), `Scrollbar` (pure composition, `LayoutBuilder` + `Stack` + `Positioned` thumb, no new `RenderObject`). **Wave 4** — `SpringSimulation` (closed-form damped-sinusoid curve approximation) and `ReorderableListView` (uniform item-height assumption, `Transform`-following drag). Deliberately deferred as needing real new subsystem capability: `Hero`, `RefreshIndicator`, `SelectableText` (all since landed — see their own entries). 51 new tests across 4 files; two real bugs caught by the tests (a `std::clamp` precondition violation in the scrollbar geometry math, and a wrong assumption in one of the baseline tests). 658/658 tests passing (up from 607/607).

- **`Canvas.getDestinationClipBounds()` and `getLocalClipBounds()`** — clip bounds were already tracked internally in absolute/destination space, so `getDestinationClipBounds()` is a thin accessor over the existing `currentClip()`; `getLocalClipBounds()` maps it back to the caller's local space by inverting the current transform, reusing the existing corner-transform AABB helper.

- **`GestureDetector` rewritten onto independent arena-competing recognizers** — replaces `RenderGestureDetector`'s single monolithic state machine with Flutter's real architecture: `TapGestureRecognizer`, `LongPressGestureRecognizer`, `DragGestureRecognizer` (Pan/Horizontal/Vertical), `ScaleGestureRecognizer`, and `ForcePressGestureRecognizer` each independently join the gesture arena and resolve on their own, instead of one class hard-coding every gesture's disambiguation logic together. New surface on `GestureDetector`: granular tap family (`on_tap_down`/`up`/`cancel`, secondary/tertiary tap — `button` field added to `PointerEvent`, wired through macOS/Windows/GDK/Linux X11/Wayland/Android, iOS excluded since touch has no button concept); granular long-press family (`on_long_press_down`/`cancel`/`start`/`move_update`/`end`); axis-locked `on_horizontal_drag_*`/`on_vertical_drag_*` alongside pan, all three now carrying real fling velocity via `VelocityTracker` on release; `on_scale_start`/`update`/`end` (pinch/rotate, degrading to plain pan for a single pointer); `on_force_press_start`/`peak`/`update`/`end` (gated to `PointerDeviceKind::stylus`); `HitTestBehavior` (opaque/translucent/deferToChild) and `dragStartBehavior`. `RenderStack::hitTestChildren()` now continues past a translucent hit instead of stopping, so a translucent `GestureDetector` doesn't block whatever's stacked behind it. Known gap: macOS trackpad two-finger pinch/rotate isn't forwarded as concurrent multi-pointer events yet, so `ScaleGestureRecognizer` isn't reachable via a MacBook trackpad today (touch/multi-touch sources work). Windows/GDK/Linux/Android button-plumbing edits were written carefully but not build-verified in this session (no cross-compilation toolchain available); only macOS and the core recognizer logic were compiled and test-verified. 926 tests passing, 0 regressions from the prior 923-test baseline.

- **`Path.combine()` boolean ops via vendored Clipper2** — union/intersect/difference/reverseDifference/xor between two `Path`s, matching Flutter's `Path.combine(PathOperation, Path, Path)`. General polygon boolean ops (self-intersection, degenerate/touching edges, holes via winding rules) is a hard problem to get robust from scratch, so this vendors Clipper2 (Boost License 1.0) via `FetchContent`, the same pattern already used for `vector_math`/`campello_gpu`, rather than hand-rolling a Weiler-Atherton/Greiner-Hormann clipper. Both inputs are flattened via the existing GPU tessellator's `buildPathContours()` before the boolean op runs, converted to Clipper2's `PathsD`, and the result converted back via `moveTo`/`lineTo`/`close()` — operates on a polygon approximation of any curves, consistent with this codebase's existing curve-approximate `getBounds()`/`contains()`. The combined result's fill type is always winding, since Clipper2 encodes holes via opposite ring winding direction, which the existing GPU fill-winding pipeline already interprets correctly under the nonzero rule; mixed-fill-type inputs are interpreted under `path1`'s rule. 939 tests passing, 0 regressions from the prior 926-test baseline. Visually confirmed via the gallery's new Path Ops section that difference/xorOp results render as real holes, not solid fills.

- **`RefreshIndicator`, themed across all 4 design systems** — pull-to-refresh, matching Flutter's Material-only `RefreshIndicator`, implemented via a new `DesignSystem::buildRefreshIndicator()` method (`RefreshIndicatorConfig{pull_progress, refreshing}`) rather than a core-only widget, since `campello_widgets` core carries zero concrete visual style. Reuses `CircularProgressIndicator`'s existing determinate/indeterminate modes for the pull-reveal and spin phases. Backed by a new `ScrollController` overscroll-listener channel wired into `RenderSingleChildScrollView`/`RenderListView`'s already-tracked raw offset — only these two scrollables are wired (`GridView`/`PageView`/`TreeView` aren't, so a `RefreshIndicator` wrapping one of those just never activates rather than crashing). Release-vs-still-dragging is inferred from the spring-back's monotonic decrease, since this codebase has no `ScrollNotification` bubbling system to synthesize a real pointer-up signal from. Incidentally fixes `campello_fluent`'s standing gaps, surfaced while giving it a `buildRefreshIndicator()` too: it was a real, built-by-default 4th sibling design system undocumented in `CLAUDE.md`, absent from the gallery's runtime theme switcher, and with zero test coverage — all three fixed here. 962 tests passing, 0 regressions.

- **`Canvas::drawArc()` antialiased via a dedicated SDF pie shader** — all three GPU backends previously CPU-tessellated `drawArc()` into a triangle fan/strip submitted through the flat, non-AA rect pipeline, the one primitive without antialiasing and most visible in the `RefreshIndicator`/`CircularProgressIndicator` spinner. New dedicated `arc_pipeline_` per backend using a per-pixel analytic distance field (radial distance to the outer circle boundary combined with two half-plane clip-plane tests for the angular cut) — confirmed against Skia's actual arc renderer (`GrOvalOpFactory.cpp`'s `CircleOp`) before implementing: Flutter's real `CircularProgressIndicator` hits the exact same code path, and Skia's GPU implementation uses the same per-pixel distance-field approach, not geometry-based AA. Metal is pixel-verified (caught and fixed a real stroke-convention mismatch this way); Vulkan's shader compiles cleanly via `glslangValidator` but is unverified at runtime (no Vulkan device in that environment); D3D12 written by close analogy to the proven Metal/Vulkan math, fully unverified (no Windows toolchain reachable). 962 tests passing, 0 regressions.

- **Sliver scrolling protocol** (`RenderSliver`/`SliverConstraints`/`SliverGeometry`/`RenderViewport`), a 5-stage migration toward Flutter's real sliver architecture instead of maintaining a parallel scrolling system. `RenderViewport` coordinates child layout with a scroll-offset-correction reflow loop; `RenderSliverToBoxAdapter` bridges any existing `RenderBox` into a sliver; `RenderSliverFixedExtentList` virtualizes multiple same-height children by index (mirrors `RenderListView`'s sparse index-keyed storage and visible-range math, re-based onto a sliver's own local `scroll_offset`/`remaining_paint_extent`); `RenderSliverPersistentHeader` (pinned variant, SliverAppBar-style) required extending `RenderViewport` itself — a per-child `layout_offset` clamp (gated behind `SliverGeometry::max_scroll_obstruction_extent > 0`) freezes a pinned header's position at the accumulated floor of earlier pinned siblings, plus a clip pass for body content scrolling under a fully-collapsed header and a second paint pass so pinned headers always draw on top — both changes are inert no-ops for every pre-existing sliver, confirmed by re-running the full `RenderViewport` suite unchanged as a regression gate. Bridged into the Widget/Element tree via `CustomScrollView`/`SliverToBoxAdapter`/`SliverFixedExtentList`/`SliverPersistentHeader` widgets, following `ListView`'s own precedent of subclassing `RenderObjectWidget` directly (`MultiChildRenderObjectWidget`'s child-insertion hooks are hard-typed to `RenderBox`, which no `RenderSliver` is) — `CustomScrollViewElement`'s reconciliation is copied in shape from `MultiChildRenderObjectElement`, simplified to positional matching and synced via `RenderViewport`'s own `insertChild()`/`truncateChildren()`. New "Slivers" gallery tab (a pinned 60–160px header over a 60-item `SliverFixedExtentList`), visually verified in the running app. 1020/1020 tests passing.

- **`NestedScrollView`** — a pinned/collapsing header sharing one drag gesture with an independently-scrolling body list, built on the sliver protocol above in 4 stages. `RenderSliverFillRemaining` claims whatever paint budget is left in the viewport for one child (the future body region), laid out tight to the leftover extent rather than its own natural size. `RenderSliverOverlapAbsorber`/`RenderSliverOverlapInjector` form the overlap channel between the outer viewport's header and the inner viewport's body: the absorber wraps the outer header sliver(s), forwarding their geometry unchanged while recording how much they permanently obstruct into a shared `SliverOverlapAbsorberHandle`; the injector is a pure spacer sliver reserving a matching gap as the inner viewport's first sliver. A new `applyExternalScrollDelta()` entry point on `RenderListView`/`RenderSingleChildScrollView`/`RenderViewport` lets a coordinator apply a delta through a view's own physics without going through its pointer-driven gesture state, returning the actually-applied (post-physics) amount so any remainder can be redistributed elsewhere — each is a 3-line wrapper around that class's existing, unchanged private `applyScrollDelta()`. `NestedScrollCoordinator::applyUserOffset()` is the coordinator itself: splits a user-driven delta between an outer and inner scrollable (outer-priority collapsing, inner-priority expanding, leftover flowing to whichever didn't go first), translated from Flutter's real `_NestedScrollCoordinator.applyUserOffset()` into this codebase's own sign convention, wired in via a new optional `external_delta_redirect` hook on each scrollable's pointer-move/scroll handling. Bridged into the Widget/Element tree via a new `RenderNestedScrollView` (owns two `RenderViewport`s — outer above, inner filling the remainder — wired together via a `NestedScrollCoordinator` in its constructor) and a `NestedScrollView` widget, whose outer viewport height is set automatically from `header->min_extent` so its scrollable range lands exactly on the header's real collapse range with no separate configuration knob. New "Nested" gallery tab (a pinned 60–200px "Profile" header over a 40-item body list). Verified end-to-end with two real `RenderViewport`s driven through real `onPointerEvent()` drags in both directions, not just unit-level pieces. Along the way: `ScrollController::jumpTo()` unconditionally hard-clamps to `[min,max]` regardless of physics, so a controller-attached `scrollOffset()` can never surface rubber-banded overscroll — pre-existing, documented behavior rather than a fix, since the `raw_offset_`/`notifyOverscroll()` channel exists for exactly this reason. 1069/1069 tests passing.

- **`FocusTraversalGroup`** — Tab-order grouping orthogonal to `FocusScope`. `FocusNode` gains `isTraversalGroup()`, independent of `isScope()`: a group node never gates Tab-escape or restores focus on unmount, unlike a scope — confirmed genuinely orthogonal in Flutter's own source (`FocusTraversalGroup` creates a plain `FocusNode`, not a `FocusScopeNode`, with no restore logic in its `dispose()`). `FocusManager::sortWithGroups()` replaces the direct `policy->order(candidates)` call in `moveFocusForward()`/`moveFocusBackward()`: buckets candidates by their nearest enclosing group, sorts each bucket with that group's own `traversal_policy` (falling back to the ambient one), then flattens depth-first so a group's members stay contiguous — ports Flutter's real `_findGroups`/`_sortAllDescendants` shape. Collapses to exactly prior behavior whenever no group exists, by construction, verified by running the full pre-existing `FocusManager` suite unchanged as a regression gate. 1076/1076 tests passing.

- **`Hero` — Flutter-parity shared-element transitions**, a 5-stage slice from foundation through a real, working flight. `RenderBox::computeGlobalRect()` consolidates the global-coordinate recipe `RenderFocus`/`RenderGestureDetector` each independently implemented (paint-origin inset subtraction + ambient-canvas-transform projection) into one shared helper; `Tween<T>` gained a `Rect` specialization. `Element::visitAllDescendants()` is a generic recursive descendant walker (built entirely on `Element::visitChildren()`'s existing per-type overrides — turned out to need no new tree infrastructure at all); `Hero` itself started as a pure-passthrough `StatelessWidget` with no transition logic, matched by tag via `Hero::collectHeroesFor()` — a fresh subtree walk per call, mirroring Flutter's real `Hero._allHeroesFor()` rather than a persistent registry. `NavigatorObserver` (`didPush`/`didPop`/`didReplace`/`didChangeTop`, every method a no-op default) is notified from small additive blocks in `push()`/`pop()`/`pushReplacement()` — confirmed, contrary to the original scoping assumption, that a Hero flight never touches a route's own transition at all (it's a separate overlay layered on top), so `NavigatorState::build()`'s `SlideTransition` construction stays completely untouched; `pop()`'s notification fires immediately, before the exit animation completes, matching Flutter's real timing (needed so a flight can run in parallel with the exit transition). `HeroController` builds a tag-matched manifest of `Hero` pairs across a route transition via `NavigatorState::elementForRoute()` (walks from the Navigator's own `StatefulElement` through `NavigatorScope` to the `Stack`, matching each layer's `ObjectKey(route)`), isolating the walk per route so two Heroes sharing a tag across different routes can't collide into one entry. The flight itself: a new `PostFrameCallbacks` primitive (one-shot callbacks run once a frame's build+layout+paint pass finishes, pumped once per frame from `Renderer::buildFrame()`) defers both manifest-building and rect capture until the destination route has actually been built and painted — real, necessary infrastructure this codebase didn't have (`FrameScheduler` is a persistent "please redraw" latch, not a one-shot queue). `Hero` was upgraded from `StatelessWidget` to a `SingleChildRenderObjectWidget` wrapping a new `RenderHero` (captures its own on-screen rect during paint via `computeGlobalRect()`, and can hide its child while a flight is in progress) — simpler than the originally-planned `StatefulWidget`-based placeholder swap. `HeroController::runFlights()` builds a `Tween<Rect>`-driven `Overlay` shuttle for each matched pair, reusing the route transition's own `AnimationController` directly (rather than a separate one) so the flight's duration automatically matches the route transition, and re-reads both endpoints' rects on every animation tick (not just once at flight start) so the shuttle correctly tracks a still-sliding destination route and converges on its true resting position rather than wherever it happened to be at the instant the flight began. New "Hero" gallery tab (a grid of color tiles flying into a detail view via a shared tag). 1106/1106 tests passing at the initiative's completion.

### Changed

- **`BoxGradient` fills, gradient borders, and Container/LimitedBox parity** — `BoxGradient` (linear/radial/sweep) is now paintable via `BoxDecoration::gradient` and `BoxBorder::gradientBorder()`, rendered through the existing Shader/PaintContext pipeline with an inset stroke path so gradient borders aren't clipped by the offscreen shader-mask capture texture; `RenderDecoratedBox` gained a shared `needsOffsetLayerFor()` helper so `isRepaintBoundary()` and `paint()` agree on when an offscreen layer is actually needed. Also closes several Container/Flutter parity gaps found while working on gradients: `foreground_decoration` support, correct `effectiveConstraints()` clamping, border-width padding merge, a new `LimitedBox` widget (backing `Container`'s empty-box fill behavior), and a null-child `RenderColoredBox`/`RenderDecoratedBox` layout fix so an unbounded axis never reports infinite size — which surfaced a real bug in `RenderConstrainedBox`: naive per-field `min()`/`max()` constraint intersection could produce invalid (`min > max`) constraints when `additional_constraints` had an infinite bound, silently resolving to infinity, fixed by using the existing `BoxConstraints::enforce()` instead. Gallery gained GRADIENT FILLS / GRADIENT BORDERS showcase rows.

- **`campello_gpu` dependency bumped `v0.23.1` → `v0.24.0`** (`dependencies/campello_gpu.cmake`) — fixes real silent-wrong-output bugs directly relevant to this project's own `Path.fillType()` MSAA stencil-then-cover antialiasing: Metal never applied `stencilFront`/`stencilBack` to `MTLDepthStencilDescriptor` (any pipeline requesting stencil ops silently got Metal's always-pass default); Vulkan's `StencilOp` enum order didn't match `VkStencilOp`'s real spec values, silently applying the wrong stencil operation; Vulkan's MSAA `resolveTarget` was only wired for the `VK_KHR_dynamic_rendering` path, silently producing a blank, never-resolved result on devices without that extension; Metal's `createTexture()` forced `StorageModeManaged` on MSAA textures, which aborts on Intel integrated GPUs; Windows/DirectX MSAA support was entirely unimplemented until this release. Also picks up `v0.23.2`'s fixes (a Metal `presentDrawable` data race, scissor-rect rounding, several Windows/DirectX bind-group/descriptor-heap bugs). See `campello_gpu`'s own `CHANGELOG.md` for the full list.

- **`campello_image` dependency bumped `v0.5.0` → `v0.5.1`** (`dependencies/campello_image.cmake`) — upgrades the vendored `basis_universal` texture transcoder from `1.16.4` to `v2_50`, fixing a transcoder-initialization bug specific to transcoder-only usage (this library's exact usage pattern). This project doesn't touch the Basis Universal texture-transcoding path itself (only standard raster decode for the `Image` widget), so low-risk. Also fixes `campello_image`'s own `tests/CMakeLists.txt` subdirectory silently being pulled into this project's own `ctest` run (its `BUILD_TESTS` option is a no-op once this project's own same-named cache variable is already set) — both an unintended extra 33 tests folded into this project's own test count, and, since `FetchContent_Declare` is first-wins, a silent override of this project's own `googletest` pin regardless of what it said. Fixed by mirroring `campello_gpu.cmake`'s existing guard (force `BUILD_TESTS OFF` around `campello_image`'s `FetchContent_MakeAvailable()` call, restore after).

- **`googletest` dependency bumped `v1.14.0` → `v1.18.0`** (`dependencies/googletest.cmake`) — the previous pin had been entirely ineffective (see the `campello_image` fix above); now genuinely takes effect. No breaking changes relevant to this project (CMake minimum bumped to 3.16, C++ minimum to C++17 — this project is already C++20).

### Fixed

- **Liquid glass tint opacity was masking its own blur/refraction almost entirely** — root-caused a report that the effect looked stronger in isolation than in real dialogs: `blur_sigma`/`refraction_strength` turned out to have near-zero visible effect (confirmed via controlled 10x parameter swings producing <2/255 pixel difference), because `dialog`/`confirmationDialog`'s backdrop tint opacity was fixed at 0.60 regardless of brightness — enough tint weight to wash out most of the blurred backdrop underneath it. Recalibrated per brightness against real captures by sampling the card-interior brightness plateau and solving for the tint weight that reproduces it (light: 0.75, dark: 0.54 — they diverge because `dialog_bg`'s luminance sits on opposite sides of the blurred backdrop's luminance between themes). The shader effect itself was also pushed stronger (wider refraction/rim bands, more chromatic aberration and saturation boost, `specular_intensity` to 1.0), verified not to regress real-device fidelity.

- **A recurring "foreground-discard" bug — an icon's own tint color was computed but never applied** — found and fixed independently in seven different builders across both design systems while validating the new `Icon` widget against real captures: `CupertinoDesignSystem::buildNavigationBar()` (tab bar items), `MaterialDesignSystem::buildIconButton()`, `buildNavigationRail()`, `buildNavigationBar()`, both systems' `buildAppBar()` (leading/action icons) and `buildPrimaryActionButton()` (FAB glyph), and `buildListTile()` (leading icon) — real `UITabBar`/M3 components all tint their icons per selection/role, and none of these builders did. Each fixed to the platform's real convention (Material: `onSurface`/`onSurfaceVariant`/`onPrimaryContainer` roles; Cupertino: `tintColor` accent for nav items, a secondary-label equivalent for list rows).

- **`Align` without `width_factor`/`height_factor` silently expands to fill whatever space its parent gives it instead of shrink-wrapping to its child** — a framework-level footgun (`Align`'s own default sizing behavior, not a design-system bug) found and independently fixed in five different places once real-device layouts finally handed these widgets a generous space budget: `MaterialDesignSystem`'s and `CupertinoDesignSystem`'s `buildBottomSheet()` drag-handle wrapper (ballooned to fill the whole sheet under Android's `Stack(fit=expand)`), `CupertinoDesignSystem::buildNavigationRail()` (only exposed under the iPad sidebar's full-height canvas), and `MaterialDesignSystem::buildButton()`'s label centering (only `height_factor` was set, so a standalone button expanded to whatever width it was handed instead of sizing to its label).

- **A cluster of real Material 3 rendering bugs found by diffing against real Jetpack Compose captures**: `buildDialog()` used a plain `surface` fill instead of M3's `surfaceContainerHigh` tonal role and the caller's default text style instead of M3's real `headlineSmall`/`bodyMedium` typography, with action buttons missing the 40dp minimum touch target and expanding to fill their row instead of shrink-wrapping (dialog diff: ~15.5% → ~1.7-2.0%); `buildNavigationBar()` filled its container with plain `surface`, rendering with no visible chrome at all, and never tinted its icons per selection (fixed to `surfaceContainer`, 9.29% → ~1.46%); `buildToggleButtons()`/`buildSegmentedButton()` showed no checkmark on selected segments and didn't clip child painting to the outer stadium border (square corners poking past the rounded outline); `PopupMenuButtonState::open()`'s menu column used `CrossAxisAlignment::stretch` (silently claiming the full screen width instead of shrink-wrapping) and its items had no minimum touch-target height; `buildListTile()` painted no background at all (Flutter's convention) where real M3 `ListItem` defaults to an opaque surface fill; the shared card-rendering code forced every card to a fixed 240dp width instead of shrink-wrapping to content, and used the wrong tonal role for elevated/filled variants; `DropdownButton`'s and `buildButton()`'s outlined variant both unconditionally painted an opaque background fill instead of staying transparent/border-only; and `MaterialDesignSystem::buildNavigationRail()` only showed item labels when `extended=true`, where real M3's compact rail shows labels by default and uses a fixed 80dp width rather than sizing to content. Each verified against real-device captures, typically landing in a ~0.03-2.1% pixel-diff floor after the fix (from starting points as high as 100% for the worst case, a bottom sheet that filled the entire screen).

- **A cluster of real Cupertino/iOS rendering bugs found the same way, against real `UIKit` captures**: `buildDialog()`/`buildActionSheet()` were missing the background dimming scrim in `showDialog()`, had a clear-color compositing bug, didn't match real `UIAlertController`'s action ordering/bolding or its 3+-action row height, and needed a structural fix for an iOS 26 regression where `popoverPresentationController` now forces `UIAlertController(.actionSheet)` into a popover on iPhone; classic (pre-iOS-26) `buildDialog()`/`buildActionSheet()` were painting opaque flat fills where real `UIAlertController` has always used a translucent blur vibrancy card, and the dialog's action row painted an opaque tint strip that occluded the card's own blur; `buildButton()`'s centering `Align` was missing `width_factor` (button expanded to fill its container instead of sizing to its label); `buildAppBar()` left its title left-aligned instead of always centering it, matching real `UINavigationBar`'s behavior regardless of any "center_title" flag; `buildSearchField()` was fully opaque instead of using the same translucent-blur treatment as dialog/actionSheet, and used the literal word "search" instead of a magnifying-glass glyph; and `buildDataTable()` had an opaque card fill and dimmed header text where real iOS's grouped-list table has neither. Each verified against real captures on real iOS hardware/Simulator, typically dropping diff percentages from double digits (up to ~26%) into a low single-digit floor.

- **`campello_gpu` dependency bumped to `v0.23.1`** (`dependencies/campello_gpu.cmake`) — the latest tagged release at the time, a Windows/DirectX-only bugfix release (swapchain resize, descriptor-heap contiguity/leaks, cube-texture depth handling) with no API changes. Verified via a clean full rebuild.

- **Stroked rounded-rect borders vanished at the rect boundary (Metal/DirectX/Vulkan)** — `shapeVertex()`'s quad was sized to exactly the shape's logical rect with no allowance for stroke width; a centered stroke extends `stroke_w*0.5` outward from that boundary, but no triangle geometry existed out there for the rasterizer to shade. Any `DecoratedBox` border (`TextField`, `Button`, `Card`) rendered fine on whichever side of an edge happened to fall inside the rect and vanished on the side outside it, depending purely on sub-pixel rounding. Fixed by inflating the quad's vertex extent by `stroke_w*0.5 + aa` on stroked draws, leaving the SDF's reference rect unchanged; Vulkan's `rrect.frag` draws an inset (not centered) stroke so its exposure was a harder/aliased edge rather than a full vanish, but the same quad-clipping root cause applied.

- **Two GPU resource-lifetime bugs surfaced while investigating a Windows D3D12 `ReportCorruption` crash**: (1) `Renderer` now defers GPU resource destruction (textures, bind groups) via a retire-list keyed to the submitting `Fence` instead of destroying synchronously on cache eviction — a resource could previously be freed while still referenced by an in-flight command buffer under `Device::submit()`'s pipelined execution model; (2) `OffsetLayer` now carries a process-lifetime incarnation ID alongside its raw address for GPU cache-key purposes — a virtualized `ListView` recycling scrolled-out `RenderObject`s can allocate a new, unrelated `OffsetLayer` at a freed address, and keying solely on address let the clip-shape/shader-mask/shadow caches replay a previous occupant's stale content. Also: `D3DDrawBackend`'s blur-source bind-group cache switched from size-triggered `.clear()` to age-based eviction — the size-triggered clear was a live correctness bug, not just a perf tradeoff (DirectX frees a `BindGroup`'s descriptor slot for immediate reuse by a later allocation in the same not-yet-submitted command list; a `GridView` with many same-sized `ClipRRect` cells could clear the cache mid-frame and let a later cell's fresh `BindGroup` steal an earlier, already-recorded-but-not-yet-executed cell's descriptor slot before the GPU read it, swapping their rendered content).

- **Android: animations froze solid after ~2.1s of continued playback** (found while testing D-pad navigation on a real Chromecast with Google TV) — `AChoreographer_postFrameCallback()`'s callback type takes the vsync timestamp as `long`, 32 bits on the `armeabi-v7a` ABI these TV devices ship; truncating the real 64-bit monotonic nanosecond timestamp overflows roughly every 2.1s, jumping the timestamp backward. Every ticker/`AnimationController` computes elapsed/delta time from this value, sees non-advancing (or negative) time on that tick, and silently skips its update — vsync keeps firing, but the widget tree never gets marked dirty again. Looked exactly like a GPU driver hang (frozen frame, idle CPU, no crash) until traced with logging. Fixed by switching to `AChoreographer_postFrameCallback64` (real `int64_t`, available since API 29). Confirmed fixed on both a Chromecast with Google TV and a Google TV Streamer. Incidental hardening from the same investigation: the per-frame quad vertex-buffer pool now requests host-visible memory explicitly instead of best-effort probing for a combined type, avoiding a silent fallback to `Buffer::upload()`'s synchronous staging-copy-plus-fence-wait path on a buffer re-uploaded every animation frame.

- **Android: Unity Build redefinition of `g_java_vm`** — `src/android/clipboard.cpp` and `android_text_rasterizer.cpp` each declared a namespace-scope `static JavaVM* g_java_vm`; fine individually compiled, but Unity Build (batch size 16) can merge both into one translation unit depending on batch boundaries, producing a hard redefinition error. Renamed the rasterizer's copy to `g_rasterizer_java_vm`.

- **Tab reached `FocusManager`'s forward/backward traversal unconditionally**, even when the focused control legitimately wants to consume it itself (e.g. `RichTextField` inserting an indent, matching real code editors) — mirrors the same "focused node gets first chance" pattern already used for D-pad navigation.

- **macOS: hover cursor and transparent-titlebar safe-area were both wrong under Cupertino** — real AppKit controls (Xcode, Finder, System Settings) keep the plain arrow cursor on hover; the pointing-hand convention is web/Material, not Cupertino/HIG. New `DesignSystem::prefersPointerCursorOnHover()` (default `true`, `false` in `CupertinoDesignSystem`) drives the shared `Button` widget's cursor instead of hardcoding pointer. Separately, `Renderer::layoutPass()`/`generateDrawList()` apply `view_insets_` as an unconditional, renderer-level canvas shrink before any widget (including a `SafeArea` opting out) gets a say — for a window with a transparent title bar (`fullSizeContentView` + `titlebarAppearsTransparent`), continuing to report that inset permanently reserved a blank strip no widget tree could ever reclaim after opting into owning that space itself.

- **iOS visual fidelity, `navigationBar` real-capture**: `addTabBar()` pinned the real `UITabBar`'s bottom anchor to the raw view edge instead of the safe area (unlike `addSearchField()`'s existing top anchor), clipping the bar's bottom (rounded Liquid Glass pill corners, or the full classic bar) inside the home-indicator strip the export crop trims off; also needed a longer post-launch settle time (1.5s → 3.0s) for `UIGlassEffect` to finish compositing under Liquid Glass on a cold launch — reproduced directly (1.5s produced a blank capture, 3.0s a correctly light-tinted pill). A separate white-background change to the real-capture pipeline (reasoning the shared translucent backdrop should blend against white like the batch-export path does) was reverted: a real device screenshot genuinely shows black behind translucent/undrawn framebuffer regions in both themes (the raw screen compositor's undrawn-region color), matching the C++ harness's own long-standing `Color::black()` clear — forcing white made every real-captured case (dialog/actionSheet/searchField/navigationBar) diverge sharply from the C++ side. The shared red debug border every other (non-real-capture) widget capture already had was extended to `navigationRail` (`toggle_icon`, floating-margin/rounded-corner Liquid Glass card fixes, split `blur_sigma` 60/24 for extended/compact rather than one fixed radius over-blurring the narrower rail) and then to `dialog`/`actionSheet`/`searchField`, each drawn as a window-level overlay around the real view's actual on-screen frame post-layout.

- **Liquid Glass blur was silently capped at a Gaussian kernel radius of 24**, clamping every requested sigma above ~9.6 (including `navigationRail`'s 60px) to the same too-sharp result — raised to 96 (confirmed safe: every other Liquid Glass surface, using the smaller default 16px sigma, was unaffected within noise). `navigationRail`'s tint also needed light/dark-specific recalibration once blur was fixed (light: 0.45 → 0.6 white-tint weight; dark: unchanged at 0.45 after several overshooting iterations found no further improvement). Net: `navigationRail_extended`/`compact` diff dropped from 99.7%/99.8% to 92%/94% on `liquid_glass_light`; `liquid_glass_dark` held its prior 83.75%/93.65% baseline.

- **Windows gallery build was broken and sibling design-system libraries didn't compile under MSVC** — `examples/gallery/windows/CMakeLists.txt` wasn't linking `campello_ui`/`material`/`cupertino` at all (every themed widget silently no-op'd) and was missing the `CAMPELLO_GALLERY_ASSETS_DIR` compile definition macOS's CMakeLists already had; `-Wall -Wextra`, passed unconditionally to MSVC, is either silently ignored or a hard error depending on toolset version — guarded with `if(MSVC) .../W4/... else() -Wall -Wextra ... endif()` across `campello_ui`/`campello_material`/`campello_cupertino`. `dependencies/campello_gpu.cmake` also gained an opt-in local-checkout override (build against a sibling `../campello_gpu` working tree if present, falling back to the pinned tag otherwise) for iterating on both repos together without round-tripping through a published tag.

- **ShaderMask's Windows DX12 pipeline had placeholder (empty) bytecode** — `dx12_widgets.h`'s DXBC byte arrays were stand-ins left when the pipeline was first scaffolded; replaced with real compiled bytecode via `build_dx12_shaders.bat`, making `ShaderMask` actually functional on the DX12 backend.

- **`DropdownButton`'s trigger padding was hardcoded** (12px vertical, ~24px alone before the label's own line height), with no way for a caller embedding it in a height-constrained space to make it fit — found via a real bug: campello_editor's toolbar visibly grew the moment a project opened and this control's default-sized trigger first appeared. Exposed as `content_padding` (defaults to the previous hardcoded value, so existing callers are unaffected).

- **GPU backend: gradient LUT fix, filterQuality sampler plumbing, stroke rendering** — `buildGradientLUT()` indexed `colors`/`stops` assuming equal length, a real crash when they differ; now falls back to evenly-spaced stops.

- **`BoxBorder::all()` was marked `constexpr` on a non-literal return type** — `BoxBorder` contains `std::optional<BoxGradient>`, and `BoxGradient`'s variant alternatives hold `std::vector<Color>` members, not a literal type (no constexpr destructor pre-C++23), making a constexpr-returning factory here ill-formed. Xcode's clang didn't diagnose this; NDK's clang (building/testing on a physical Android device) correctly rejected it, blocking the Android build entirely. No call site relies on compile-time evaluation; dropped `constexpr`.

- **`ListView` rows whose content (not just item count/order) changed for a continuously-visible index stayed frozen at whatever content existed the first time that index was mounted** — the old "skip already-mounted indices" fast path only refreshed a row's `builder()` output on first mount, never again. Found via a real bug: a diagnostics panel whose row text kept changing while continuously visible showed permanently stale text per row. `updateChild()` already does the right thing either way (reuses the element in place when widget type/key match, or unmounts/recreates when they don't), so passing the existing element through on every `performBuild()` pass is both correct and no more expensive than the mount-only path for indices that didn't change.

- **`examples/gallery`'s sidebar nav list was a plain `Column` with no scrolling** — on a short window (or once enough sections accumulate to no longer fit) it clipped silently, leaving later tabs unreachable. Wrapped in a `SingleChildScrollView`, a no-op whenever everything already fits.

- **`HeroController` never actually ran a flight in a real app, and — once it did — targeted the wrong rect.** Two bugs, both invisible in the unit test suite because tests happened to mask them, both surfaced by actually running a Hero flight in the gallery instead of only unit-testing it: (1) `didChangeTop()` built the tag-matched manifest synchronously, but `push()`/`pop()` only *schedule* a rebuild (`setState()` → `Element::markNeedsBuild()`, a no-op dirty-mark in real apps — only test environments rebuild synchronously, via `markNeedsBuild()`'s own fallback for exactly that case), so `elementForRoute()` always returned null in a real app, the manifest stayed permanently empty, and no flight ever ran. Manifest-building is now deferred into the same post-frame callback as rect capture and the flight itself, mirroring Flutter's own `HeroController`, which defers Hero discovery for exactly this reason. (2) `runFlights()` captured both endpoints' rects once, at flight start — for a route wrapped in its own `SlideTransition`, that captures wherever the destination happened to be at that instant (e.g. still fully off-screen at the very start of a push), not where it's heading. Now re-reads both rects every tick instead (a plain field read, not new measurement work, since `RenderHero` keeps its captured rect current every frame regardless of being hidden), so the shuttle's moving endpoint tracks the route's own transition and converges on the real resting position. 1106/1106 tests passing, no regressions.

## [0.7.0] - 2026-08-13

### Added

- **Cross-platform `PlatformMenuBar` support for Windows/Linux.** Previously macOS-only (native `NSMenu`). Windows and Linux have no equivalent native menu-bar concept reachable without adopting a whole extra toolkit, so on those platforms `PlatformMenuDelegate` now reports `needsInWindowMenuBar() = true`, and a new `PlatformMenuBarView` widget renders the menu bar itself from the same `PlatformMenu`/`PlatformMenuItem` data, including keyboard accelerators. The horizontal top-level bar is a `ListView` (scrolls instead of clipping when there isn't enough width); each open dropdown's items are a `Row` of {label, `Expanded` spacer, shortcut caption} sized from actually-measured text. `FocusManager` gained a `globalKeyHandler()` getter so `PlatformMenuBarView` can chain onto any existing global key handler instead of clobbering it. The gallery example wires this up as a real demo: a "View" menu with Ctrl+1..9,0 shortcuts jumping to each tab.
- **`runApp()` overload accepting an explicit Linux `app_id`** (`inc/campello_widgets/linux/run_app.hpp`), plus a `defaultAppIdFromTitle()` fallback for callers that don't supply one. Sets `WM_CLASS` on X11 and `xdg_toplevel`'s `app_id` (via `libdecor_frame_set_app_id()`) on Wayland — what desktop shells use to match the running window to an installed `.desktop` file for the taskbar/alt-tab icon and name.
- Phase 12b: filled out `IDrawBackend` across Metal/Vulkan/D3D12 (`drawArc`, `drawPath`, `drawPoints`, `saveLayer`, `drawShaderMaskComposite`), sharing path flattening/tessellation via `src/gpu/path_tessellation.*`. Adds Vulkan line/shader_mask shaders and a DX12 `shader_mask.hlsl`. Refactors `GpuVisualRenderer` into a backend-agnostic core plus per-platform factories, and adds a Vulkan offscreen-readback path for headless GPU visual tests on Linux/Android.

### Changed

- **`campello_gpu` dependency re-pinned to `GIT_TAG v0.23.0`** (`dependencies/campello_gpu.cmake`) — this release's Linux fixes above (cursor handling, GPU shutdown ordering, BoxFit/paint-cache, HiDPI, rotated-image texture corruption, draw-surface barrier) were developed and verified against a local `SOURCE_DIR` checkout of `campello_gpu`; that checkout has now been tagged upstream as `v0.23.0`, so the build goes back to fetching the reproducible tagged commit instead of a local path. Includes, beyond what the `campello_widgets` fixes above already needed: the offscreen-pass `oldLayout`/barrier and texture-lifetime fixes referenced inline above, plus `Texture::createView()`'s new `mipLevelCount` parameter, `validationErrorCount()`/`resetValidationErrorCount()`, mipmap/array-layer and descriptor-type/pool fixes, and (landed after `v0.23.0`'s own changelog entry was written, folded into the same tag) Metal `TextureView` array/dimension fixes and a Metal/WebGPU/DirectX `createBindGroup()` signature mismatch fix. See `campello_gpu`'s own `CHANGELOG.md` for the full list.

### Fixed

- **[Linux] Cursor got stuck on whatever shape the window manager/compositor last set (typically a resize icon) once the pointer entered the window.** `setSystemCursor()` had no registered handler on Linux, so every `MouseRegion`/`TextField`/etc. cursor-shape change was a silent no-op. X11 now creates the standard cursor set via `XCreateFontCursor()` and wires `registerCursorHandler()` to `XDefineCursor()`. Wayland loads the system cursor theme via `wl_cursor_theme_load()` and wires it to `wl_pointer_set_cursor()` on a dedicated cursor surface, reclaiming the cursor immediately on `pointer_enter()` rather than waiting for the first move (libdecor manages its own cursor for the window decoration, and Wayland has no automatic reset crossing into the content surface). Needs the new `wayland-cursor` pkg-config dependency.
- **Two `ImageWidget`s pointing at the same image source, mounted in the same frame, triggered two independent decodes and two separate GPU textures for what should be one cached image** — found while chasing a Vulkan validation error under the gallery example; the redundant second `Texture` was what actually tripped it (see `campello_gpu`'s changelog for that half of the fix). `ImageLoader::loadAsync()` now checks an in-flight map before queuing a new decode task; a second caller for the same cache key gets the same `std::shared_future` the first caller holds. `loadAsync()`'s return type (and `ImageWidgetState::load_future_`) changed from `std::future` to `std::shared_future` since a plain `std::future` can't be shared across multiple waiters.
- **[Linux] `Renderer`/`VulkanDrawBackend` GPU resource caches (bind groups, textures, pipelines, samplers) could be destroyed on shutdown while the GPU was still executing the last submitted frame's command buffer** — `Device::submit()` is pipelined and doesn't block, so this reproduced 100% of the time under Vulkan validation layers on every clean shutdown. Fixed by having `Renderer::~Renderer()`/`VulkanDrawBackend::~VulkanDrawBackend()` call the existing `Device::waitForIdle()` before their own cache members tear down, mirroring how `Device::~Device()` already protects its own teardown.
- **Real `BoxFit`/clip rendering bug**: offscreen-composited children (`ClipRRect`/`ClipOval`/`ShaderMask`/`SaveLayer`) replayed nested clip commands with absolute canvas-space geometry, clipping content entirely outside the small offscreen child texture. Shared `translateChildCommands()` now corrects nested clip geometry for the synthetic offset. Verified against Flutter goldens for all 7 `BoxFit` modes.
- **Scroll/resize paint-cache performance**: `PictureLayer`'s "unsafe geometry" classification was overly broad, forcing a full re-record (and uncached GPU offscreen re-render) of any clipped/masked/shadowed content on every reposition — the common case during scroll and window resize. Narrowed to the two commands that actually need it; `OffsetLayer::maybeReplay()` now shifts clip-rect geometry by hand instead, unlocking GPU-side cache reuse. `RenderListView`/`GridView`/`SingleChildScrollView`/`TableView`/`TreeView` now self-boundary their own paint output via `OffsetLayer` so this applies to scrollable viewports directly, not just `RenderRepaintBoundary`.
- **[Linux/Wayland] Missing `VelocityTracker`-based wheel momentum handoff**, and a `frame_done()` callback bug starving the `poll()`-timeout tick.
- **Linux windows rendered at the wrong resolution on HiDPI displays** — neither the X11 nor the Wayland backend ever detected the display's scale factor: X11 hardcoded `device_pixel_ratio = 1.0f` and sized its window in raw (logical) pixels instead of physical ones; Wayland did the same and never called `wl_surface_set_buffer_scale()`. On a 2x display this produced a window rendered at half the intended physical resolution, visibly blurry or (under XWayland compositor scaling) undersized. Fixed in `src/linux/run_app.cpp` (new `getX11DisplayScale()`, reading `Xft.dpi` with a screen-mm fallback) and `src/linux/wayland_runner.cpp` (`wl_output`/`wl_surface.enter`-based scale discovery, `wl_surface_set_buffer_scale()`), both now sizing the window/swapchain in physical pixels. A second, related bug: even after physical sizing was correct, `Renderer::device_pixel_ratio_` — a separate member from `MediaQueryData`'s copy, and the one `buildFrame()` actually divides the physical viewport by to compute logical layout constraints — was never set on Linux, so layout still sized every fixed-size widget against the full physical viewport as if DPR were 1, shrinking all UI to roughly half its intended on-screen size. Fixed by calling `Renderer::setDevicePixelRatio()` after renderer construction in both backends. Verified on a live 2x-scale display (X11: exact physical-pixel window-size match via `xwininfo`; Wayland: `wl_surface.set_buffer_scale(2)` and correct swapchain size confirmed via `WAYLAND_DEBUG=1`; both: correct on-screen widget/text proportions).

- **Rotated/scaled images in the gallery's Images tab intermittently rendered another draw call's texture (e.g. glyph/text content) instead of the image** — `VulkanDrawBackend::drawTexturedQuad()` called `encoder.setBindGroup(0, bind_group)` *before* `encoder.setPipeline(...)`, the only place in the file to do so (every other draw method, e.g. `drawClipShapeComposite()`, correctly binds the pipeline first). The bind-group call was validated against whatever pipeline layout happened to still be bound from the *previous*, unrelated draw call in the pass (frequently a text/glyph draw) rather than the quad pipeline about to be selected — usually incompatible, which left the actual draw with no valid descriptor set 0 bound and made it sample whatever texture happened to still be resident in that binding slot. Confirmed via Vulkan validation layers (`VUID-vkCmdBindDescriptorSets-firstSet-00360`, `VUID-vkCmdDraw-None-08600`) and fixed by moving `setBindGroup()` after `setPipeline()` in both the axis-aligned and perspective/rotated code paths (`src/gpu/vulkan/vulkan_draw_backend.cpp`).

- **The gallery's Draw tab produced a broken, dotted stroke instead of a solid line, and content wiped by the Clear button could resurface on the next stroke** — traced to `campello_gpu`'s Vulkan offscreen-pass barrier (see `campello_gpu`'s own changelog for the fix); found and verified against a local `SOURCE_DIR` checkout at the time, now covered by the `v0.23.0` pin above. Verified with a solid, continuous stroke and a clean Clear-then-redraw cycle on both Linux (X11 and Wayland) and Android (Samsung Galaxy Tab S7 FE).

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
