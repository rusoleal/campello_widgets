#!/usr/bin/env bash
#
# Run C++ Tests Only - For environments without Flutter
#
# This script runs the C++ fidelity tests using existing golden files.
# Use run_fidelity_tests.sh for the complete workflow including Flutter.
#

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

BUILD_DIR="${SCRIPT_DIR}/build/darwin-release-test"
GOLDENS_DIR="${SCRIPT_DIR}/tests/goldens"

# Arguments
VERBOSE=false
REBUILD=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        --rebuild)
            REBUILD=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  -v, --verbose    Verbose output"
            echo "  --rebuild        Force rebuild"
            echo "  -h, --help       Show help"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

echo "================================================"
echo "  Campello Widgets - C++ Tests Only"
echo "================================================"
echo ""

# Check for goldens
if [[ ! -d "$GOLDENS_DIR" ]] || [[ -z "$(ls -A "$GOLDENS_DIR"/*.json 2>/dev/null)" ]]; then
    echo -e "${YELLOW}[WARN]${NC} No golden files found in $GOLDENS_DIR"
    echo "Run ./run_fidelity_tests.sh first to generate goldens from Flutter."
    echo ""
fi

# Build if needed
if [[ ! -f "$BUILD_DIR/tests/campello_widgets_tests" ]] || [[ "$REBUILD" == true ]]; then
    echo "Building C++ tests..."
    cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -DBUILD_EXAMPLES=OFF > /dev/null 2>&1
    cmake --build "$BUILD_DIR" --target campello_widgets_tests -j$(sysctl -n hw.ncpu) > /dev/null 2>&1
    echo -e "${GREEN}[PASS]${NC} Build complete"
    echo ""
fi

# Run tests
echo "Running tests..."
echo "------------------------------------------------"

if [[ "$VERBOSE" == true ]]; then
    cd "$BUILD_DIR" && ./tests/campello_widgets_tests
else
    cd "$BUILD_DIR" && ./tests/campello_widgets_tests 2>&1 | grep -E "^\[|tests? from|PASSED|FAILED" || true
fi

echo "------------------------------------------------"
echo ""
echo "Done!"
