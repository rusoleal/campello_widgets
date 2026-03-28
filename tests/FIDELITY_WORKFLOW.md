# Fidelity Testing Workflow

This document explains the complete workflow for validating campello_widgets against Flutter using golden files.

## Overview

```
┌─────────────────────────────────────────────────────────────┐
│                     FLUTTER SIDE                            │
│  flutter_fidelity_tester/                                   │
│  └── test/fidelity_goldens_test.dart  ────────┐             │
│                                                │             │
│  Run: ./generate_goldens.sh                    ▼             │
│                                        ┌──────────────┐      │
│                                        │  Generate    │      │
│                                        │  golden JSON │      │
│                                        └──────┬───────┘      │
└───────────────────────────────────────────────┼──────────────┘
                                                │
                                                ▼
┌───────────────────────────────────────────────┼──────────────┐
│                    SHARED                     │              │
│  tests/goldens/                              │               │
│  ├── simple_column_flutter.json ◄─────────────┘              │
│  ├── simple_row_flutter.json                                 │
│  ├── nested_padding_flutter.json                             │
│  └── ...                                                     │
└──────────────────────────────────────────────────────────────┘
                                                │
                                                ▼
┌─────────────────────────────────────────────────────────────┐
│                     C++ SIDE                                │
│  tests/universal/test_fidelity_flutter_goldens.cpp          │
│                                                             │
│  1. Load golden JSON ◄────────────────────────────────┐     │
│  2. Build equivalent C++ widget tree                   │     │
│  3. Capture snapshot                                   │     │
│  4. Compare structures ────────────────────────────────┤     │
│  5. Report differences ◄───────────────────────────────┘     │
│                                                             │
│  Run: ./test.sh                                             │
└─────────────────────────────────────────────────────────────┘
```

## Step-by-Step Usage

### 1. Generate Flutter Goldens

```bash
cd flutter_fidelity_tester
./generate_goldens.sh
```

This creates JSON files in `tests/goldens/` representing Flutter's render tree output.

### 2. Build C++ with Tests

```bash
cd ..
BUILD_TESTS=ON ./build.sh
```

### 3. Run Fidelity Tests

```bash
cd build/darwin
./tests/campello_widgets_tests --gtest_filter="FlutterGoldenValidation*"
```

### 4. Review Results

If there are differences, the test output will show:

```
Expected: RenderFlex(width=100, height=50)
Actual:   RenderFlex(width=98, height=50)
Path:     root.children[0]
Difference: width mismatch
```

## Adding New Fidelity Tests

### Flutter Side

1. Add a test in `flutter_fidelity_tester/test/fidelity_goldens_test.dart`:

```dart
testWidgets('my_new_widget', (WidgetTester tester) async {
  final widget = MyWidget(...);
  
  await tester.pumpWidget(
    MaterialApp(home: Scaffold(body: widget)),
  );
  
  // Capture and save
  final snapshot = RenderTreeCapture.captureRenderTree(...);
  final goldenFile = File('${goldensDir.path}/my_new_widget_flutter.json');
  goldenFile.writeAsStringSync(fidelitySnapshot.toJsonString());
});
```

2. Run Flutter tests to generate golden

### C++ Side

1. Add corresponding test in `tests/universal/test_fidelity_flutter_goldens.cpp`:

```cpp
TEST(FlutterGoldenValidation, MyNewWidget) {
  if (!goldenFileExists("my_new_widget_flutter.json")) {
    GTEST_SKIP() << "Golden file not found";
  }
  
  // Build equivalent C++ widget tree
  auto root = std::make_shared<Render...>();
  // ... setup children ...
  
  // Capture and compare
  auto actual = cwt::captureSnapshot(*root, constraints, 400, 600);
  
  // Load golden and compare
  std::string golden_json = loadGolden("my_new_widget_flutter.json");
  // ... compare ...
}
```

2. Rebuild and run tests

## Understanding the Output

### Human-Readable Layout Tree

```
RenderFlex(400×600) at (0, 0) [400-400 × 600-600] {axis=vertical}
  RenderPadding(400×116) at (0, 0) [0-400 × 0-600]
    RenderColoredBox(384×100) at (8, 8) [0-400 × 0-584]
```

Format: `Type(width×height) at (x, y) [minWidth-maxWidth × minHeight-maxHeight]`

### JSON Structure

```json
{
  "viewport": { "width": 400, "height": 600 },
  "layout": {
    "type": "RenderFlex",
    "size": { "width": 400, "height": 600 },
    "offset": { "x": 0, "y": 0 },
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
    "children": [
      {
        "type": "RenderPadding",
        "size": { "width": 400, "height": 116 },
        ...
      }
    ]
  },
  "paint_commands": [
    {
      "type": "rect",
      "rect": { "left": 8, "top": 8, "right": 392, "bottom": 108 },
      "paint": {
        "color": { "r": 0.129, "g": 0.588, "b": 0.953, "a": 1.0 },
        "style": "fill"
      }
    }
  ]
}
```

## Common Differences

### Expected Differences (Handled by Framework)

1. **Float Precision**: Minor differences in float arithmetic
   - Solution: Use tolerance (default 0.001)

2. **Type Names**: Flutter uses different class names
   - `RenderColumn` (Flutter) vs `RenderFlex` (C++)
   - Solution: Semantic mapping or property comparison

3. **Color Precision**: Platform-specific color handling
   - Solution: Compare with tolerance

### Unexpected Differences (Bugs)

1. **Size Mismatch**: Different computed sizes indicate layout bugs
2. **Offset Mismatch**: Positioning logic difference
3. **Missing Children**: Child management difference
4. **Missing Commands**: Paint logic difference

## Debugging Failed Tests

### 1. Print Both Trees

```cpp
TEST(Debugging, CompareTrees) {
  auto cpp_tree = buildCppTree();
  auto snapshot = cwt::captureSnapshot(*cpp_tree, ...);
  
  std::cout << "C++ Tree:\n" << cwt::renderNodeToString(snapshot.layout);
  
  // Load Flutter golden
  auto flutter_json = cwt::loadGoldenFile("widget_flutter.json");
  std::cout << "Flutter JSON:\n" << flutter_json;
}
```

### 2. Use Detailed Comparison

```cpp
auto result = cwt::compareRenderTrees(expected, actual);
for (const auto& diff : result.differences) {
  std::cout << "Difference: " << diff << "\n";
}
```

### 3. Check Specific Properties

```cpp
EXPECT_FLOAT_EQ(actual.layout.width, 100.0f) 
  << "Width mismatch at: " << path;
```

## Continuous Integration

### GitHub Actions Example

```yaml
name: Fidelity Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Setup Flutter
        uses: subosito/flutter-action@v2
      
      - name: Generate Goldens
        run: |
          cd flutter_fidelity_tester
          flutter pub get
          flutter test test/fidelity_goldens_test.dart
      
      - name: Build C++ Tests
        run: |
          cmake -B build -DBUILD_TESTS=ON
          cmake --build build --target campello_widgets_tests
      
      - name: Run Fidelity Tests
        run: ./build/tests/campello_widgets_tests
```

## Best Practices

1. **Keep goldens minimal**: Test one concept per golden
2. **Use consistent viewport sizes**: Standard sizes (400x600, 800x600)
3. **Document intentional differences**: Add comments when C++ differs by design
4. **Version control goldens**: Track golden changes in git
5. **Update goldens deliberately**: Don't auto-update without review

## Troubleshooting

### Golden file not found

```
[SKIPPED] Golden file not found. Run Flutter fidelity tester first.
```

**Solution**: Run `./flutter_fidelity_tester/generate_goldens.sh`

### Build errors

```
fatal error: 'campello_widgets/testing/fidelity.hpp' file not found
```

**Solution**: Make sure `BUILD_TESTS=ON` is set

### Runtime errors

```
Segmentation fault in compareRenderTrees
```

**Solution**: Check that both trees have the same structure (children count)

## Next Steps

1. Generate Flutter goldens for all existing C++ tests
2. Add more complex widget combinations
3. Implement paint command capture in Flutter
4. Add animation frame comparison
5. Create visual diff reports
