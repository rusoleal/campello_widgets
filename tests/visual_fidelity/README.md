# Visual Fidelity Testing

This directory contains visual regression tests that compare actual rendered PNG images between Flutter and C++ implementations.

## How It Works

1. **Flutter side** generates reference PNG images using offscreen rendering
2. **C++ side** renders the same widget tree to an offscreen texture and saves as PNG
3. **Comparison** is done pixel-by-pixel with a configurable tolerance

## Directory Structure

```
tests/visual_fidelity/
├── flutter_goldens/     # PNG files generated from Flutter
│   ├── simple_column.png
│   ├── simple_row.png
│   └── ...
├── cpp_output/          # PNG files generated from C++ (gitignored)
│   └── ...
└── diffs/               # Visual diff images for failed tests (gitignored)
    ├── simple_column_diff.png
    ├── simple_row_diff.png
    └── ...
```

## Running Tests

```bash
# Generate Flutter goldens (from project root)
cd flutter_fidelity_tester
flutter test test/visual_goldens_test.dart

# Run C++ visual tests
./run_fidelity_tests.sh --visual

# Or run everything
./run_fidelity_tests.sh
```

## Adding New Visual Tests

1. Add a `testWidgets` block in `flutter_fidelity_tester/test/visual_goldens_test.dart`
2. Add corresponding C++ test in `tests/universal/test_visual_fidelity.cpp`
3. Both should render the same widget structure at the same size

## Image Format

All images are saved as:
- Format: PNG
- Size: 1280x720 (default viewport for fidelity tests)
- Color space: sRGB
- Alpha: Premultiplied

## Understanding Diff Images

When visual tests detect differences, a `_diff.png` file is generated in the `diffs/` folder:

| Color | Meaning |
|-------|---------|
| **Dark gray/black** | Pixels match within tolerance |
| **Red/yellow** | Pixels differ significantly |
| **Yellow border** | Indicates the image contains differences |

The diff highlights exactly where Flutter and C++ renders diverge, making it easy to:
- Identify rendering bugs
- Spot missing features (e.g., anti-aliasing)
- Verify color accuracy

Example interpretation:
- Solid red regions = completely different colors
- Pink/light red = subtle differences (anti-aliasing, alpha blending)
- Yellow border = visual indicator that differences exist in this image
