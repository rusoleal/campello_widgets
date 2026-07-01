# campello_widgets

A C++20 UI framework inspired by Flutter's widget composition model, built on top of [campello_gpu](https://github.com/rusoleal/campello_gpu) for rendering and [campello_input](https://github.com/rusoleal/campello_input) for input handling.

## 🚀 Part of the Campello Engine

This project is a module within the **Campello** ecosystem.

👉 Main repository: https://github.com/rusoleal/campello

Campello is a modular, composable game engine built as a collection of independent libraries.
Each module is designed to work standalone, but integrates seamlessly into the engine runtime.

## Overview

`campello_widgets` provides a declarative, composable approach to building graphical user interfaces in C++. The design philosophy mirrors Flutter's three-tree architecture — a **Widget tree**, an **Element tree**, and a **RenderObject tree** — giving you the expressiveness of a modern UI toolkit with the performance of native C++.

The library is fully multiplatform and targets:

- Windows
- macOS
- Linux
- iOS
- Android

## Threading Model

> **⚠️ Single-threaded only.** `campello_widgets` is architected as a single-threaded UI framework (like Flutter). All widget tree manipulation, layout, painting, animation, input handling, and state changes **must** happen on one thread (the "main" or "UI" thread).
>
> Calling `setState()`, mutating an `AnimationController`, dispatching pointer events, or touching the focus system from a background thread will cause data races, memory corruption, and crashes. The framework does not provide internal synchronization for these operations.
>
> The only APIs that are safe to use from background threads are:
> - `ImageLoader::loadAsync()` / `ImageLoader::loadSync()`
> - `ImageCache::get()` / `ImageCache::put()` / `ImageCache::evict()`
> - `PlatformMenuDelegate` methods (internally synchronized)
>
> If you need to update the UI from a background thread, marshal the call to the main thread via your platform's mechanism (e.g. `dispatch_async(dispatch_get_main_queue(), ...)` on Apple platforms, `runOnUiThread()` on Android, or `PostMessage()` on Windows).

## Dependencies

| Package | Role |
|---|---|
| [`campello_gpu`](https://github.com/rusoleal/campello_gpu) | Graphics rendering backend (multiplatform) |
| [`campello_input`](https://github.com/rusoleal/campello_input) | Input event handling (keyboard, mouse, touch, gamepad) |
| [`campello_image`](https://github.com/rusoleal/campello_image) | Image loading (JPEG, PNG, WebP, GIF, BMP, TGA) |
| [`vector_math`](https://github.com/rusoleal/vector_math) | SIMD-optimized vector and matrix math |

## Architecture

### Three-Tree Model

The framework follows the same rendering pipeline as Flutter:

```
Widget Tree         Element Tree        RenderObject Tree
(immutable          (mutable            (layout + paint)
 descriptions)       instances)
```

- **Widget** — a lightweight, immutable description of a piece of UI. Cheap to create and discard.
- **Element** — the live instance of a widget in the tree. Manages the widget lifecycle and reconciliation between rebuilds.
- **RenderObject** — owns layout and painting. Communicates with `campello_gpu` to issue draw calls.

### Widget Types

```cpp
// Describes UI without mutable state
class StatelessWidget : public Widget { ... };

// Owns mutable state; rebuilds when state changes
class StatefulWidget : public Widget { ... };

// Directly controls layout and painting
class RenderObjectWidget : public Widget { ... };
```

### Layout Protocol

Layout follows a **constraints-down, sizes-up** protocol identical to Flutter's box model:

1. Parent passes `BoxConstraints` (min/max width and height) down to children.
2. Each child computes its own size within those constraints.
3. Parent uses the reported size to position the child.

### Rendering

All draw calls are issued through `campello_gpu`. The RenderObject tree is traversed each frame; dirty subtrees are repainted into GPU-backed layers that are then composited.

### Input

Input events (pointer, keyboard, touch) are received from `campello_input` and dispatched down the widget tree through a hit-testing pass on the RenderObject tree.

### IME (Input Method Editor)

Full IME support on macOS enables entering complex characters:

- **Accented characters**: `´` + `e` → `é`, `` ` `` + `a` → `à`, `~` + `n` → `ñ`
- **CJK input**: Chinese, Japanese, Korean character composition
- **Emoji picker**: System emoji picker integration (Ctrl+Cmd+Space on macOS)

Composing text is visually indicated with an underline. The `TextEditingController` provides methods to interact with the composition state (`isComposing()`, `beginComposing()`, `commitComposing()`, etc.).

### Image Loading

Integrated `campello_image` provides asynchronous image loading with caching:

```cpp
// Display image from URL
auto image = NetworkImage::create("https://example.com/photo.jpg");

// Display local image
auto image = ImageWidget::create(ImageProvider::fromFile("assets/logo.png"));
```

Supported formats: JPEG, PNG, WebP, GIF, BMP, TGA. Images are decoded asynchronously and cached in memory with an LRU eviction policy.

## Basic Widgets

| Widget | Description |
|---|---|
| `Container` | Box with optional padding, margin, decoration, and child |
| `Row` / `Column` | Linear layout along horizontal / vertical axis |
| `Stack` | Overlapping children with absolute or relative positioning |
| `Padding` | Applies insets around a single child |
| `Align` | Positions a child within itself using an alignment value |
| `SizedBox` | Forces a child (or empty space) to a specific size |
| `Text` | Renders a styled text string |
| `Image` | Renders a GPU texture |
| `ImageWidget` | Displays images from files or network with caching |
| `NetworkImage` | Downloads and displays images from URLs |
| `TextField` | Editable text input with IME support (accents, CJK) |
| `GestureDetector` | Wraps a child and listens for pointer/touch gestures |
| `Scaffold` | Top-level layout structure (background, layers) |

## Design System & Theming

`campello_widgets` includes a pluggable design-system layer that decouples visual style from widget logic. The base framework provides `DesignTokens`, `DesignSystem`, and `Theme` (an `InheritedWidget`), while a default `CampelloDesignSystem` ships with a distinct warm-teal visual identity.

```cpp
// Wrap your app in a Theme to activate a design system
runApp(Theme::create(Theme{
    .design_system = std::make_shared<CampelloDesignSystem>(CampelloDesignSystem::dark()),
    .child = std::make_shared<MyApp>(),
}));

// Adaptive widgets automatically delegate to the active design system
auto button = Button::create({.label = Text::create("Save"), .on_pressed = onSave});
```

Adaptive widgets (e.g. `Button`, `Card`, `AppBar`, `NavigationBar`) call `Theme::of(ctx).buildXxx(config)` so their appearance is determined by the active `DesignSystem` rather than hardcoded values. This enables runtime switching between light/dark modes and future extraction of `CampelloDesignSystem` into independent theme libraries.

## Example (Planned API)

```cpp
class MyApp : public StatelessWidget {
public:
    WidgetRef build(BuildContext& ctx) const override {
        return Container::create({
            .padding = EdgeInsets::all(16.0f),
            .child = Column::create({
                .children = {
                    Text::create("Hello, campello_widgets!"),
                    SizedBox::create({.height = 8.0f}),
                    Text::create("Built with C++20"),
                }
            })
        });
    }
};
```

## Testing

### Unit Tests

```bash
./test.sh              # Run all unit tests
./test.sh --fidelity   # Run Flutter fidelity tests
```

### Flutter Fidelity Testing

Validate that campello_widgets renders identically to Flutter:

```bash
# Full workflow: generate Flutter goldens + run C++ tests
./run_fidelity_tests.sh

# C++ tests only (using existing goldens)
./run_cpp_tests_only.sh

# Specific test
./run_fidelity_tests.sh --test SimpleColumn
```

The fidelity testing framework compares:
- **Layout**: Render tree structure, sizes, positions
- **Paint**: Draw commands emitted by Canvas
- **Visual** (optional): Pixel-level comparison

See [FIDELITY_TESTING.md](tests/FIDELITY_TESTING.md) for details.

## Build System

The project uses CMake with C++20 as the minimum standard. Dependencies are automatically fetched via CMake's `FetchContent`.

```bash
# Standard build
cmake -B build -DBUILD_EXAMPLES=ON
cmake --build build

# With Unity Build for faster compilation (default ON)
cmake -B build -DENABLE_UNITY_BUILD=ON -DBUILD_EXAMPLES=ON
cmake --build build

# Disable Unity Build if you encounter issues
cmake -B build -DENABLE_UNITY_BUILD=OFF -DBUILD_EXAMPLES=ON
```

### CMake Options

| Option | Default | Description |
|---|---|---|
| `BUILD_EXAMPLES` | OFF | Build example applications |
| `BUILD_TESTS` | OFF | Build unit tests |
| `ENABLE_UNITY_BUILD` | ON | Enable unity build for faster compilation |

### Consume as Dependency

```cmake
find_package(campello_widgets REQUIRED)
target_link_libraries(my_app PRIVATE campello_widgets)
```

## Platform Development

### macOS

**Requirements:** Xcode (provides Clang, Metal, Cocoa, CoreText and all other required system frameworks — no extra installs needed).

**Build:**
```bash
./build.sh darwin
```

**Run examples:**
```bash
./run_macos_example.sh        # Hello World
./run_macos_counter.sh        # StatefulWidget counter
./run_macos_listview.sh       # ListView
./run_macos_animated.sh       # Animated transitions
./run_macos_gestures.sh       # Gesture detection
./run_macos_textfield.sh      # TextField + IME
./run_macos_keyboard.sh       # Keyboard input
./run_macos_image.sh          # Image loading
./run_macos_showcase.sh       # Full widget showcase
./run_macos_table_view.sh     # Table view
./run_macos_tree_view.sh      # Tree view
./run_macos_menu_test.sh      # Platform menus
```

Release variants are available as `run_macos_*_release.sh`.

---

### Linux

**Requirements:** The following system packages must be installed before building. These are OS-level interface libraries (display server, font subsystem, IME) that cannot be bundled via CMake — they need to match the running system.

```bash
sudo apt-get install -y \
    pkg-config \
    libx11-dev \
    libdbus-1-dev \
    libfreetype-dev \
    libharfbuzz-dev \
    libfontconfig-dev \
    libwayland-dev \
    libxkbcommon-dev \
    libcurl4-openssl-dev
```

| Package | Role |
|---|---|
| `pkg-config` | Required by CMake to locate system libraries |
| `libx11-dev` | X11 display server client library |
| `libdbus-1-dev` | D-Bus, used for IBus IME integration |
| `libfreetype-dev` | Font rasterization |
| `libharfbuzz-dev` | Text shaping |
| `libfontconfig-dev` | System font discovery and configuration |
| `libwayland-dev` | Wayland display protocol (optional, enabled if found) |
| `libxkbcommon-dev` | Keyboard layout handling for Wayland |
| `libcurl4-openssl-dev` | HTTP client for network image loading |

**Build:**
```bash
./build.sh linux
```

**Run example:**
```bash
./examples/linux_hello/run.sh
```

Or manually after building:
```bash
./build/linux-release/campello_widgets_linux_hello
```

---

### Windows

**Requirements:** Visual Studio 2022 (or later) with the **Desktop development with C++** workload. All Windows API dependencies (`DirectWrite`, `Direct2D`, `DXGI`, `Direct3D`, `WinHTTP`, `IMM32`) are part of the Windows SDK included with Visual Studio — no extra installs needed.

**Build:**
```bat
build.bat
```

Or with CMake directly:
```bat
cmake -S . -B build\windows -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=ON
cmake --build build\windows --config Release
```

---

### iOS

**Requirements:** Xcode on macOS with the iOS SDK installed. Build is cross-compiled from macOS targeting an iOS device or simulator.

```bash
cmake -S . -B build/ios \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 \
    -DBUILD_EXAMPLES=ON
cmake --build build/ios --config Release
```

The library is built as a static archive (`libcampello_widgets.a`) to be embedded in an Xcode project or app bundle.

---

### Android

**Requirements:** Android NDK (r25 or later). Set the `ANDROID_NDK_HOME` environment variable or pass `-DANDROID_NDK=<path>` to CMake. The NDK provides all required headers and system libraries.

```bash
cmake -S . -B build/android \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-26 \
    -DBUILD_EXAMPLES=ON
cmake --build build/android --config Release
```

Supported ABIs: `arm64-v8a`, `x86_64`. Minimum API level: 26 (Android 8.0).

---

## License

See [LICENSE](LICENSE).
