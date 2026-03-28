#!/usr/bin/env bash
set -e

echo "Generating Flutter golden files for campello_widgets fidelity testing..."
echo ""

# Check if flutter is available
if ! command -v flutter &> /dev/null; then
    echo "Error: Flutter is not installed or not in PATH"
    exit 1
fi

echo "Flutter version:"
flutter --version
echo ""

# Get dependencies
echo "Getting Flutter dependencies..."
flutter pub get
echo ""

# Create goldens directories
mkdir -p ../tests/goldens
mkdir -p ../tests/visual_fidelity/flutter_goldens

# Check for --real-text flag
USE_REAL_TEXT=false
for arg in "$@"; do
    if [ "$arg" == "--real-text" ]; then
        USE_REAL_TEXT=true
    fi
done

# Run JSON goldens tests (these don't have text rendering issues)
echo "Running tests to generate JSON golden files..."
flutter test test/fidelity_goldens_test.dart
echo ""

# Run visual goldens tests
echo "Running tests to generate visual (PNG) golden files..."

if [ "$USE_REAL_TEXT" = true ]; then
    echo "Using REAL TEXT rendering (requires desktop device)..."
    bash run_with_fonts.sh
else
    echo "Using default test mode (text will appear as colored blocks)."
    echo "For real text rendering, use: $0 --real-text"
    echo ""
    flutter test test/visual_goldens_test.dart
fi
echo ""

echo ""
echo "Golden files generation complete."
echo ""

# List generated files
echo "JSON goldens: ../tests/goldens/"
ls -la ../tests/goldens/*.json 2>/dev/null | awk '{print "    " $9 " (" $5 " bytes)"}' || echo "    (none)"
echo ""

echo "PNG goldens: ../tests/visual_fidelity/flutter_goldens/"
ls -la ../tests/visual_fidelity/flutter_goldens/*.png 2>/dev/null | awk '{print "    " $9 " (" $5 " bytes)"}' || echo "    (none)"
echo ""

# Check if PNG files were generated with content
PNG_COUNT=$(ls -1 ../tests/visual_fidelity/flutter_goldens/*.png 2>/dev/null | wc -l)
VALID_PNGS=$(find ../tests/visual_fidelity/flutter_goldens -name "*.png" -size +0c 2>/dev/null | wc -l)

echo "========================================"
echo "Summary:"
echo "  Total PNG files: $PNG_COUNT"
echo "  Valid PNG files (>0 bytes): $VALID_PNGS"
if [ "$USE_REAL_TEXT" = false ]; then
    echo ""
    echo "Note: Text appears as colored blocks. Use --real-text for actual glyphs."
fi
echo "========================================"

if [ "$VALID_PNGS" -eq 0 ] && [ "$PNG_COUNT" -gt 0 ]; then
    echo "WARNING: PNG files exist but all are 0 bytes. There may be an issue with generation."
    exit 1
fi

echo "Done!"
