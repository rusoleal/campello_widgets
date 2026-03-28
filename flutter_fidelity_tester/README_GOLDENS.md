# Flutter Golden File Generation

This document explains how to generate golden files for visual fidelity testing between Flutter and C++ implementations.

## The Problem

By default, `flutter test` replaces all text with colored rectangles (using the "Ahem" font). This ensures pixel-perfect reproducibility across different systems, but makes text-based fidelity tests meaningless since the C++ implementation renders actual text.

## Two Solutions

### Option 1: Quick Test (Colored Blocks) - `flutter test`

Use this for non-text tests or CI environments:

```bash
cd flutter_fidelity_tester
flutter test test/visual_goldens_test.dart
```

**Output**: Text is rendered as colored blocks matching text color and bounding box.

### Option 2: Real Text Rendering - `flutter run` (Recommended)

Use this for accurate text fidelity comparison:

```bash
cd flutter_fidelity_tester
bash run_with_fonts.sh
```

**Requirements**:
- Desktop support enabled: `flutter config --enable-macos-desktop`
- macOS, Linux, or Windows device available
- Roboto font downloaded (script handles this)

**Output**: Text is rendered with actual glyphs using the Roboto font.

## How It Works

### Why `flutter test` Shows Blocks

Flutter's test framework intentionally replaces fonts with "Ahem" - a font where every glyph is a solid rectangle. This ensures:
- Pixel-perfect reproducibility across OS versions
- No dependency on system fonts
- Faster test execution

### How `flutter run` Shows Real Text

When running with `flutter run`, Flutter uses real font rendering:
- Loads actual TTF font files
- Renders proper glyphs with hinting
- Platform-specific font rasterization

## Font Configuration

The project uses **Roboto** as the test font:
- Open source (Apache 2.0 license)
- Available on all platforms
- Good match for system sans-serif fonts

Font files are stored in `flutter_fidelity_tester/fonts/`.

## Updating Goldens

To regenerate all golden files with real text:

```bash
cd flutter_fidelity_tester

# Download fonts (first time only)
bash download_fonts.sh

# Generate with real text
bash run_with_fonts.sh

# Or use the main script from project root
cd ..
bash run_fidelity_tests.sh --generate
```

## Comparing Results

After generating goldens, run C++ tests:

```bash
./run_fidelity_tests.sh --visual
```

This will:
1. Generate C++ PNG output
2. Compare against Flutter goldens
3. Create diff images in `tests/visual_fidelity/diffs/`

## Troubleshooting

### "No desktop device found"

Enable desktop support:
```bash
flutter config --enable-macos-desktop  # macOS
flutter config --enable-linux-desktop  # Linux
flutter config --enable-windows-desktop  # Windows
```

### Text looks different between Flutter and C++

This is expected - font rasterization differs between:
- Flutter/Skia text rendering
- C++ Metal/OpenGL text rendering
- Different font hinting algorithms

Use tolerance-based comparison (already configured at 20% for text tests).

### Fonts not loading

Check that fonts exist:
```bash
ls flutter_fidelity_tester/fonts/*.ttf
```

If missing, run:
```bash
cd flutter_fidelity_tester
bash download_fonts.sh
```
