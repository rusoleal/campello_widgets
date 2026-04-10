# campello_widgets

A C++20 UI framework inspired by Flutter's widget composition model, built on top of `campello_gpu` for rendering and `campello_input` for input handling.

## Overview

`campello_widgets` provides a declarative, composable approach to building graphical user interfaces in C++. The design philosophy mirrors Flutter's three-tree architecture — a **Widget tree**, an **Element tree**, and a **RenderObject tree** — giving you the expressiveness of a modern UI toolkit with the performance of native C++.

The library is fully multiplatform and targets:

- Windows
- macOS
- Linux
- iOS
- Android

## Dependencies

| Package | Role |
|---|---|
| [`campello_gpu`](https://github.com/rusoleal/campello_gpu) | Graphics rendering backend (multiplatform) |
| `campello_input` | Input event handling (keyboard, mouse, touch, gamepad) |
| `campello_image` | Image loading (JPEG, PNG, WebP, GIF, BMP, TGA) |
| `vector_math` | SIMD-optimized vector and matrix math |

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

## License

See [LICENSE](LICENSE).
