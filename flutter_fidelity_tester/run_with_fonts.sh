#!/usr/bin/env bash
# Generate Flutter goldens with real text rendering using flutter run
set -e

cd "$(dirname "$0")"

echo "=========================================="
echo "Flutter Golden Generator (with real text)"
echo "=========================================="
echo ""

# Check for fonts directory
if [ ! -d "fonts" ] || [ -z "$(ls fonts/*.ttf 2>/dev/null)" ]; then
    echo "Fonts not found. Downloading..."
    bash download_fonts.sh
fi

# Create output directory
mkdir -p ../tests/visual_fidelity/flutter_goldens

# Check for connected devices
echo "Checking for available devices..."
flutter devices

echo ""
echo "This script will use 'flutter run' to generate goldens with real text rendering."
echo "This requires a desktop device (macos/linux/windows) or simulator."
echo ""

# Try to run on macOS first, then any available device
if flutter devices | grep -q "macOS"; then
    DEVICE="macOS"
    echo "Found macOS device. Using it for golden generation."
elif flutter devices | grep -q "Linux"; then
    DEVICE="Linux"
    echo "Found Linux device. Using it for golden generation."
elif flutter devices | grep -q "Windows"; then
    DEVICE="Windows"
    echo "Found Windows device. Using it for golden generation."
else
    echo "WARNING: No desktop device found. Please enable desktop support:"
    echo "  flutter config --enable-macos-desktop"
    echo "  flutter config --enable-linux-desktop"
    echo "  flutter config --enable-windows-desktop"
    echo ""
    echo "Falling back to flutter test (will show colored blocks for text)..."
    flutter test test/visual_goldens_test.dart
    exit 0
fi

echo ""
echo "Running golden generator on $DEVICE..."
echo "Note: The app will exit automatically after generating all goldens."
echo ""

# Run the generator
flutter run -d "$DEVICE" --release lib/golden_generator.dart

echo ""
echo "=========================================="
echo "Golden generation complete!"
echo "=========================================="
echo ""
echo "Generated files:"
ls -la ../tests/visual_fidelity/flutter_goldens/*.png 2>/dev/null || echo "  (none found)"
