#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build/darwin"
DOT_FILE="$SCRIPT_DIR/build/deps.dot"
OUT_FILE="$SCRIPT_DIR/dependencies.png"

if ! command -v dot &>/dev/null; then
    echo "error: graphviz not found. Install it with: brew install graphviz"
    exit 1
fi

if [ ! -d "$BUILD_DIR" ]; then
    echo "error: build directory not found at $BUILD_DIR. Run ./build.sh first."
    exit 1
fi

echo "Generating dependency graph..."
cmake --graphviz="$DOT_FILE" "$BUILD_DIR" > /dev/null

echo "Rendering $OUT_FILE..."
dot -Tpng "$DOT_FILE" -o "$OUT_FILE"

echo "Done: $OUT_FILE"
