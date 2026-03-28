# Flutter Fidelity Tester

Generates golden reference files from Flutter for campello_widgets fidelity testing.

## Purpose

This Flutter project creates JSON golden files that represent the "ground truth" render tree structure from Flutter's rendering engine. These goldens are used by the C++ campello_widgets test suite to verify layout fidelity.

## Setup

```bash
cd flutter_fidelity_tester
flutter pub get
```

## Generating Goldens

```bash
./generate_goldens.sh
```

Or manually:

```bash
flutter test test/fidelity_goldens_test.dart
```

## Output

Golden files are written to `../tests/goldens/`:

- `simple_column_flutter.json` - Basic Column with Padding and Expanded
- `simple_row_flutter.json` - Row with Expanded children
- `nested_padding_flutter.json` - Nested Padding widgets
- `align_center_flutter.json` - Center alignment
- `sized_box_flutter.json` - Explicit sizing constraints
- `flex_expanded_flutter.json` - Multiple Expanded with flex factors
- `stack_positioned_flutter.json` - Stack with Positioned children

## Using in C++ Tests

The generated JSON files can be loaded in C++ fidelity tests:

```cpp
std::string flutter_golden = cwt::loadGoldenFile("simple_column_flutter.json");
FidelitySnapshot expected = FidelitySnapshot::fromJson(flutter_golden);

// Build equivalent C++ widget tree
auto root = /* ... */;

// Capture and compare
FidelitySnapshot actual = cwt::captureSnapshot(*root, constraints, 400, 600);
auto result = cwt::compareRenderTrees(expected.layout, actual.layout);

EXPECT_TRUE(result.match) << result.differences[0];
```

## Adding New Goldens

To add a new fidelity test:

1. Add a new `testWidgets` block in `test/fidelity_goldens_test.dart`
2. Build the equivalent Flutter widget tree
3. Capture and save to a new golden file
4. Add corresponding C++ test in `tests/universal/test_fidelity.cpp`

## JSON Format

The golden files follow this structure:

```json
{
  "description": "Human readable description",
  "viewport": {
    "width": 400,
    "height": 600
  },
  "layout": {
    "type": "RenderFlex",
    "size": {"width": 400, "height": 600},
    "offset": {"x": 0, "y": 0},
    "constraints": {
      "min_width": 400,
      "max_width": 400,
      "min_height": 600,
      "max_height": 600
    },
    "properties": {
      "axis": "vertical",
      "main_axis_size": "max"
    },
    "children": [ /* ... */ ]
  }
}
```

## Differences from C++ Output

Some differences are expected due to platform-specific behavior:

- **Text metrics**: Flutter uses platform-specific font rendering
- **Precision**: Minor floating point differences (handled by tolerance)
- **Render object names**: May differ slightly (e.g., `RenderFlex` vs `RenderColumn`)

The comparison utilities handle these differences through:
- Float tolerance (default 0.001)
- Normalized type name matching
- Optional property filtering
