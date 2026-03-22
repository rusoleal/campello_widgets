# campello_widgets — Claude Code Instructions

## Project Overview

`campello_widgets` is a C++20 UI framework modelled on Flutter's three-tree architecture (Widget / Element / RenderObject), built on top of `campello_gpu` (rendering) and `campello_input` (input handling). It targets macOS, iOS, Android, Windows, and Linux.

## Namespace

All code lives in `systems::leal::campello_widgets`. This mirrors `campello_gpu`'s `systems::leal::campello_gpu`.

## Directory Layout

```
inc/campello_widgets/   # Public headers (.hpp) — mirrors src/ structure
src/                    # Implementations (.cpp)
  widgets/              # Widget + Element types
  ui/                   # RenderObject types and low-level UI primitives
  macos/                # macOS platform entry point
  ios/                  # iOS platform entry point
  android/              # Android platform entry point
tests/
  universal/            # CPU-only unit tests (no GPU required)
  platform/             # Platform integration tests
examples/               # One subdirectory per platform
dependencies/           # *.cmake FetchContent wrappers
  campello_gpu.cmake    # pinned to v0.3.7
  campello_input.cmake  # pinned to main (no tags yet)
  vector_math.cmake
  googletest.cmake
macos.cmake             # Platform-specific CMake logic (included by CMakeLists.txt)
ios.cmake
android.cmake
windows.cmake
linux.cmake
```

New files must follow this layout exactly. Public API goes in `inc/campello_widgets/`, implementation in `src/`.

## Build

```bash
# Build (Release, with examples)
./build.sh

# Build for a specific platform
./build.sh darwin      # macOS
./build.sh windows
./build.sh linux

# Run macOS examples (Debug)
./run_macos_example.sh          # Hello World
./run_macos_counter.sh          # Counter (StatefulWidget demo)
./run_macos_listview.sh         # ListView
./run_macos_animated.sh         # Animated transitions

# Release variants
./run_macos_example_release.sh
./run_macos_counter_release.sh
./run_macos_listview_release.sh
./run_macos_animated_release.sh
```

Build output lands in `build/<platform>/` (e.g. `build/darwin/`).

## Tests

```bash
# Universal unit tests (no GPU, runs on any machine)
./test.sh

# With platform integration tests
INTEGRATION=ON ./test.sh
```

Tests use GoogleTest. Universal tests are in `tests/universal/` and cover layout engine types (`BoxConstraints`, `RenderAlign`, `RenderFlex`, `RenderListView`, `RenderPadding`, `RenderSizedBox`). Do not mock the GPU; integration tests hit the real platform layer.

## CMake Options

| Option | Default | Purpose |
|---|---|---|
| `BUILD_TESTS` | OFF | Universal unit tests |
| `BUILD_INTEGRATION_TESTS` | OFF | Platform integration tests (also enables `BUILD_TESTS`) |
| `BUILD_EXAMPLES` | OFF | Example applications |

## Architecture Conventions

### Three-Tree Model

```
Widget Tree         Element Tree        RenderObject Tree
(immutable          (mutable            (layout + paint)
 descriptions)       instances)
```

- **Widget** — immutable, ref-counted via `std::enable_shared_from_this`. Cheap to create.
- **WidgetRef** = `std::shared_ptr<const Widget>` — used everywhere in the API.
- **Element** — mutable live instance; drives reconciliation via `updateChild()`.
- **RenderObject / RenderBox** — owns layout and paint; issues draw calls via `Canvas` / `PaintContext`.

### Widget Types

- `StatelessWidget` — implements `build(BuildContext&) const`.
- `StatefulWidget` + `State<W>` (template) + `StateBase` (non-template base held by `StatefulElement`).
- `RenderObjectWidget` → `SingleChildRenderObjectWidget` / `MultiChildRenderObjectWidget` — bridge to RenderObject tree.

### Layout Protocol

Constraints-down, sizes-up (identical to Flutter's box model):
1. Parent passes `BoxConstraints` down.
2. Child computes its size within those constraints and returns it.
3. Parent uses reported size to position the child.

### Key Subsystems

- **Canvas / PaintContext** — wraps GPU draw commands; opacity is baked multiplicatively via `Canvas::setOpacity()`.
- **PointerDispatcher** — hit-tests the RenderObject tree on pointer-down, captures pointer, dispatches subsequent events.
- **FocusManager / FocusNode** — tab traversal, key routing.
- **TickerScheduler** — vsync-driven ticker; feeds `AnimationController`.

## Code Style

- C++20, no exceptions in hot paths.
- snake_case for files, variables, and methods. PascalCase for types.
- Private member variables suffixed with `_` (e.g. `child_`, `element_`).
- Designated initialisers for widget factory structs (`.padding = ...`, `.child = ...`).
- Headers use `#pragma once`.
- Implementations `#include` their own header first, then dependencies.

## Phase Status (as of 2026-03-22)

| Phase | Status |
|---|---|
| 1 — Project Scaffolding | Mostly done (CI + platform macros pending) |
| 2 — Core Widget Infrastructure | Complete |
| 3 — Layout System | Complete |
| 4 — Rendering Pipeline | Complete |
| 5 — Basic Render Widgets | Complete |
| 6 — Composited Widgets | Complete |
| 7 — Input Handling | Complete |
| 8 — Animation System | Complete |
| 9 — Scrolling | Complete |
| 10 — Platform Integration | Partial (macOS + Android done; iOS/Windows/Linux pending) |
| 11 — Developer Experience | Partial (unit tests + examples done; debug overlay + headless tests pending) |
