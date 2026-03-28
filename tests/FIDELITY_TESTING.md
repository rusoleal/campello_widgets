# Fidelity Testing Framework

This framework enables comprehensive testing to ensure fidelity between Flutter widgets and the campello_widgets C++ implementation. It provides multi-layer validation without requiring pixel-perfect image comparisons.

## Overview

The framework tests at three layers:

1. **Layout Fidelity (Layer 2)**: Compares the render tree structure - sizes, positions, and constraints
2. **Paint Fidelity (Layer 3)**: Compares the draw commands emitted by Canvas
3. **Visual Fidelity (Layer 4)**: Optional pixel-level comparison with tolerance

## Key Components

### `fidelity.hpp/cpp` - Core Testing Infrastructure

Located in `inc/campello_widgets/testing/` and `src/testing/`.

#### RenderNodeSnapshot
Captures the state of a RenderObject:
```cpp
struct RenderNodeSnapshot {
    std::string type;        // e.g., "RenderFlex", "RenderPadding"
    float width, height;     // Computed size
    float offset_x, offset_y; // Position in parent
    BoxConstraints constraints; // Layout constraints
    std::vector<RenderNodeSnapshot> children;
    std::vector<std::pair<std::string, std::string>> properties;
};
```

#### DrawCommandSnapshot
Captures paint commands:
```cpp
struct DrawCommandSnapshot {
    std::string type;  // "rect", "text", "image", "push_clip", etc.
    // Type-specific fields...
};
```

#### FidelitySnapshot
Complete capture of both layout and paint:
```cpp
struct FidelitySnapshot {
    RenderNodeSnapshot layout;
    std::vector<DrawCommandSnapshot> paint_commands;
    float viewport_width, viewport_height;
};
```

## Usage

### Basic Layout Test

```cpp
#include <campello_widgets/testing/fidelity.hpp>

namespace cwt = systems::leal::campello_widgets::testing;

// Build your widget tree
auto root = std::make_shared<RenderFlex>();
// ... add children ...

// Capture the layout
root.layout(BoxConstraints::tight(400.0f, 600.0f));
auto snapshot = cwt::dumpRenderTree(*root, Offset::zero());

// Verify properties
EXPECT_EQ(snapshot.type, "RenderFlex");
EXPECT_FLOAT_EQ(snapshot.width, 400.0f);
EXPECT_FLOAT_EQ(snapshot.height, 600.0f);
```

### Complete Fidelity Capture

```cpp
// Captures both layout and paint commands
auto snapshot = cwt::captureSnapshot(
    *root,
    BoxConstraints::tight(400.0f, 600.0f),
    400.0f, 600.0f  // viewport dimensions
);

// Export to JSON for comparison with Flutter
std::string json = snapshot.toJson();
```

### Comparing Against Golden Files

```cpp
// Load Flutter-generated golden
std::string golden_json = cwt::loadGoldenFile("my_widget.json");
FidelitySnapshot expected = FidelitySnapshot::fromJson(golden_json);

// Capture actual
FidelitySnapshot actual = cwt::captureSnapshot(*root, constraints, 400, 600);

// Compare
auto result = cwt::compareRenderTrees(expected.layout, actual.layout);
EXPECT_TRUE(result.match) << result.differences[0];

auto paint_result = cwt::compareDrawLists(
    expected.paint_commands, 
    actual.paint_commands,
    0.001f  // tolerance
);
EXPECT_TRUE(paint_result.match);
```

### Human-Readable Output

```cpp
auto snapshot = cwt::dumpRenderTree(*root, Offset::zero());
std::cout << cwt::renderNodeToString(snapshot);
```

Output:
```
RenderFlex(400×600) at (0, 0) [400-400 × 600-600]
  RenderPadding(400×116) at (0, 0) [0-400 × 0-600]
    RenderColoredBox(384×100) at (8, 8) [0-400 × 0-584]
```

## Golden File Format

Golden files are JSON with this structure:

```json
{
  "viewport": { "width": 400, "height": 600 },
  "layout": {
    "type": "RenderFlex",
    "size": { "width": 400, "height": 600 },
    "offset": { "x": 0, "y": 0 },
    "constraints": {
      "min_width": 400, "max_width": 400,
      "min_height": 600, "max_height": 600
    },
    "properties": { "axis": "vertical" },
    "children": [ /* ... */ ]
  },
  "paint_commands": [
    { "type": "push_transform", "matrix": [ /* 16 floats */ ] },
    { "type": "rect", "rect": { /* ... */ }, "paint": { /* ... */ } },
    { "type": "pop_transform" }
  ]
}
```

## Generating Goldens from Flutter

To generate golden files from Flutter:

1. Build the same widget tree in Flutter
2. Use `RenderObject.showOnScreen()` or debug dump
3. Capture the render tree structure
4. Export paint commands via custom render object
5. Save to `tests/goldens/<test_name>.json`

Example Flutter code:
```dart
// In a widget test
testWidgets('generate golden', (WidgetTester tester) async {
  await tester.pumpWidget(MyWidget());
  
  // Get render tree
  final renderObject = tester.renderObject(find.byType(Column));
  final treeString = renderObject.toStringDeep();
  
  // Parse and convert to your golden format
  // ...
});
```

## Running Tests

```bash
# Build with tests
BUILD_TESTS=ON ./build.sh

# Run all tests
cd build/darwin && ./tests/campello_widgets_tests

# Run only fidelity tests
./tests/campello_widgets_tests --gtest_filter="Fidelity*"

# Run specific test
./tests/campello_widgets_tests --gtest_filter="FidelityEndToEnd.CaptureSimpleWidgetTree"
```

## Benefits Over Pixel Comparison

| Aspect | Pixel Comparison | This Framework |
|--------|-----------------|----------------|
| Failure Diagnosis | "Images differ" | "Flex child width: expected 100, got 98" |
| Platform Variance | Fragile (fonts, AA) | Robust (semantic comparison) |
| Maintenance | Regenerate all goldens | Update specific properties |
| CI Reliability | GPU-dependent | Headless, no GPU needed |
| Debugging | Visual diff | Structured diff output |

## Future Enhancements

1. **Multi-child support**: Add friend declarations or accessors to RenderFlex for complete child tree capture
2. **Proper JSON parser**: Replace placeholder JSON parsing with a proper library (nlohmann/json)
3. **Flutter bridge**: Automated golden generation via Flutter driver
4. **Visual smoke tests**: Add optional pixel comparison for critical widgets
5. **Animation fidelity**: Capture and compare animation keyframes

## Notes

- The PaintContext now has a headless constructor for testing without GPU
- RenderFlex child offsets aren't fully captured (requires private member access)
- Text rendering comparison assumes identical font metrics
- Color values are compared with configurable tolerance for float differences
