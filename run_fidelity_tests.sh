#!/usr/bin/env bash
#
# Run Fidelity Tests - Complete validation of campello_widgets against Flutter
#
# This script:
# 1. Generates golden files from Flutter (JSON + PNG with REAL fonts)
# 2. Builds C++ tests
# 3. Runs C++ fidelity tests (JSON + Visual)
# 4. Reports results
#
# NOTE: Visual goldens will use REAL font rendering if macOS desktop support is enabled.
#       Otherwise, they use flutter test with Ahem font (colored blocks for text).
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Configuration
BUILD_DIR="${SCRIPT_DIR}/build/darwin-release-test"
GOLDENS_DIR="${SCRIPT_DIR}/tests/goldens"
VISUAL_DIR="${SCRIPT_DIR}/tests/visual_fidelity"
FLUTTER_DIR="${SCRIPT_DIR}/flutter_fidelity_tester"

# Parse arguments
SKIP_FLUTTER=false
SKIP_BUILD=false
VERBOSE=false
SPECIFIC_TEST=""
RUN_VISUAL=true
RUN_JSON=false

print_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -h, --help          Show this help message"
    echo "  --skip-flutter      Skip Flutter golden generation (use existing)"
    echo "  --skip-build        Skip C++ build step"
    echo "  --verbose           Verbose output"
    echo "  --test <name>       Run specific test (e.g., SimpleColumn)"
    echo "  --visual            Run visual (PNG) per-pixel tests (default)"
    echo "  --json              Run JSON layout tests only"
    echo "  --all               Run both JSON and visual tests"
    echo ""
    echo "Examples:"
    echo "  $0                  Run visual per-pixel fidelity tests (default)"
    echo "  $0 --all            Run both JSON and visual tests"
    echo "  $0 --json           Run JSON layout tests only"
    echo "  $0 --skip-flutter   Use existing goldens"
}

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            print_usage
            exit 0
            ;;
        --skip-flutter)
            SKIP_FLUTTER=true
            shift
            ;;
        --skip-build)
            SKIP_BUILD=true
            shift
            ;;
        --verbose)
            VERBOSE=true
            shift
            ;;
        --test)
            SPECIFIC_TEST="$2"
            shift 2
            ;;
        --visual)
            RUN_VISUAL=true
            RUN_JSON=false
            shift
            ;;
        --json)
            RUN_VISUAL=false
            RUN_JSON=true
            shift
            ;;
        --all)
            RUN_VISUAL=true
            RUN_JSON=true
            shift
            ;;
        *)
            echo "Unknown option: $1"
            print_usage
            exit 1
            ;;
    esac
done

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[PASS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[FAIL]${NC} $1"
}

# Header
echo "================================================"
echo "  Campello Widgets - Fidelity Test Suite"
echo "================================================"
echo ""

# Check prerequisites
check_prerequisites() {
    log_info "Checking prerequisites..."
    
    # Check for CMake
    if ! command -v cmake &> /dev/null; then
        log_error "CMake not found. Please install CMake."
        exit 1
    fi
    
    # Check for Flutter (unless skipping)
    if [[ "$SKIP_FLUTTER" == false ]]; then
        if ! command -v flutter &> /dev/null; then
            log_error "Flutter not found. Install Flutter or use --skip-flutter"
            exit 1
        fi
        log_success "Flutter found: $(flutter --version | head -1)"
    fi
    
    # Check for C++ compiler
    if ! command -v clang++ &> /dev/null && ! command -v g++ &> /dev/null; then
        log_error "No C++ compiler found. Please install clang++ or g++."
        exit 1
    fi
    
    log_success "All prerequisites satisfied"
    echo ""
}

# Generate Flutter goldens (JSON)
generate_flutter_json_goldens() {
    if [[ "$SKIP_FLUTTER" == true ]]; then
        log_info "Skipping Flutter JSON golden generation (--skip-flutter)"
        
        if [[ ! -d "$GOLDENS_DIR" ]] || [[ -z "$(ls -A "$GOLDENS_DIR"/*.json 2>/dev/null)" ]]; then
            log_warning "No existing JSON golden files found in $GOLDENS_DIR"
        else
            log_success "Found existing JSON golden files"
        fi
        echo ""
        return
    fi
    
    log_info "Generating Flutter JSON golden files..."
    
    cd "$FLUTTER_DIR"
    
    # Get dependencies
    log_info "Getting Flutter dependencies..."
    if [[ "$VERBOSE" == true ]]; then
        flutter pub get
    else
        flutter pub get > /dev/null 2>&1
    fi
    
    # Create goldens directory
    mkdir -p "$GOLDENS_DIR"
    
    # Run tests (generates goldens)
    log_info "Running Flutter tests to generate JSON goldens..."
    if [[ "$VERBOSE" == true ]]; then
        flutter test test/fidelity_goldens_test.dart
    else
        flutter test test/fidelity_goldens_test.dart 2>&1 | grep -E "(Generated:|golden)" || true
    fi
    
    cd "$SCRIPT_DIR"
    
    # Verify goldens were created
    GOLDEN_COUNT=$(ls -1 "$GOLDENS_DIR"/*_flutter.json 2>/dev/null | wc -l)
    
    if [[ $GOLDEN_COUNT -gt 0 ]]; then
        log_success "Generated $GOLDEN_COUNT JSON golden files"
    fi
    echo ""
}

# Download fonts if needed
download_fonts_if_needed() {
    if [[ ! -f "$FLUTTER_DIR/fonts/Roboto-Regular.ttf" ]]; then
        log_info "Fonts not found. Downloading..."
        cd "$FLUTTER_DIR"
        if [[ -f "download_fonts.sh" ]]; then
            bash download_fonts.sh
        else
            log_warning "Font download script not found. Text rendering may fail."
        fi
        cd "$SCRIPT_DIR"
    fi
}

# Generate Flutter visual goldens (PNG) with REAL font rendering
generate_flutter_visual_goldens() {
    if [[ "$SKIP_FLUTTER" == true ]]; then
        log_info "Skipping Flutter visual golden generation (--skip-flutter)"
        
        if [[ ! -d "$VISUAL_DIR/flutter_goldens" ]] || [[ -z "$(ls -A "$VISUAL_DIR/flutter_goldens"/*.png 2>/dev/null)" ]]; then
            log_warning "No existing visual golden files found"
        else
            log_success "Found existing visual golden files"
        fi
        echo ""
        return
    fi
    
    log_info "Generating Flutter visual golden files (PNG)..."
    
    cd "$FLUTTER_DIR"
    
    # Get dependencies
    log_info "Getting Flutter dependencies..."
    if [[ "$VERBOSE" == true ]]; then
        flutter pub get
    else
        flutter pub get > /dev/null 2>&1
    fi
    
    # Download fonts if needed
    download_fonts_if_needed
    
    # Create goldens directory
    mkdir -p "$VISUAL_DIR/flutter_goldens"
    
    # Use flutter test for golden generation.
    # Note: flutter run with macOS desktop support would provide real font rendering,
    # but macOS app sandboxing prevents writing output to the project directory.
    log_info "Using flutter test for golden generation (Ahem font - colored blocks for text)..."
    if [[ "$VERBOSE" == true ]]; then
        flutter test test/visual_goldens_test.dart
    else
        flutter test test/visual_goldens_test.dart 2>&1 | grep -E "(Generated:|Generating|passed|failed)" || true
    fi
    
    cd "$SCRIPT_DIR"
    
    # Verify goldens were created (check for non-zero size)
    GOLDEN_COUNT=$(find "$VISUAL_DIR/flutter_goldens" -name "*.png" -size +0c 2>/dev/null | wc -l)
    
    if [[ $GOLDEN_COUNT -gt 0 ]]; then
        log_success "Generated $GOLDEN_COUNT visual golden files"
    else
        log_error "No visual golden files were generated"
        return 1
    fi
    echo ""
}

# Build C++ tests
build_cpp_tests() {
    if [[ "$SKIP_BUILD" == true ]]; then
        log_info "Skipping C++ build (--skip-build)"
        
        if [[ ! -f "$BUILD_DIR/tests/campello_widgets_tests" ]]; then
            log_error "C++ tests not found. Build first or remove --skip-build"
            exit 1
        fi
        
        log_success "Using existing build"
        echo ""
        return
    fi
    
    log_info "Building C++ tests..."
    
    # Configure with tests enabled
    log_info "Configuring CMake..."
    if [[ "$VERBOSE" == true ]]; then
        cmake -S . -B "$BUILD_DIR" \
            -DCMAKE_BUILD_TYPE=Release \
            -DBUILD_TESTS=ON \
            -DBUILD_EXAMPLES=OFF
    else
        cmake -S . -B "$BUILD_DIR" \
            -DCMAKE_BUILD_TYPE=Release \
            -DBUILD_TESTS=ON \
            -DBUILD_EXAMPLES=OFF > /dev/null 2>&1
    fi
    
    # Build
    log_info "Compiling..."
    if [[ "$VERBOSE" == true ]]; then
        cmake --build "$BUILD_DIR" --target campello_widgets_tests -j$(sysctl -n hw.ncpu)
    else
        cmake --build "$BUILD_DIR" --target campello_widgets_tests -j$(sysctl -n hw.ncpu) > /dev/null 2>&1
    fi
    
    if [[ ! -f "$BUILD_DIR/tests/campello_widgets_tests" ]]; then
        log_error "Build failed - test executable not found"
        exit 1
    fi
    
    log_success "C++ tests built successfully"
    echo ""
}

# Run C++ JSON fidelity tests
run_json_tests() {
    log_info "Running C++ JSON fidelity tests..."
    echo ""
    
    cd "$BUILD_DIR"
    
    # Determine test filter
    if [[ -n "$SPECIFIC_TEST" ]]; then
        TEST_FILTER="FlutterGoldenValidation.$SPECIFIC_TEST"
    else
        TEST_FILTER="FlutterGoldenValidation*"
    fi
    
    # Run tests
    echo "------------------------------------------------"
    if [[ "$VERBOSE" == true ]]; then
        ./tests/campello_widgets_tests --gtest_filter="$TEST_FILTER"
    else
        ./tests/campello_widgets_tests --gtest_filter="$TEST_FILTER" 2>&1
    fi
    TEST_RESULT=${PIPESTATUS[0]}
    echo "------------------------------------------------"
    echo ""
    
    cd "$SCRIPT_DIR"
    
    return $TEST_RESULT
}

# Run C++ visual fidelity tests
run_visual_tests() {
    log_info "Running C++ visual fidelity tests..."
    echo ""
    
    cd "$BUILD_DIR"
    
    # Determine test filter
    if [[ -n "$SPECIFIC_TEST" ]]; then
        TEST_FILTER="VisualFidelity.$SPECIFIC_TEST"
    else
        TEST_FILTER="VisualFidelity*"
    fi
    
    # Run tests
    echo "------------------------------------------------"
    if [[ "$VERBOSE" == true ]]; then
        ./tests/campello_widgets_tests --gtest_filter="$TEST_FILTER"
    else
        ./tests/campello_widgets_tests --gtest_filter="$TEST_FILTER" 2>&1
    fi
    TEST_RESULT=${PIPESTATUS[0]}
    echo "------------------------------------------------"
    echo ""
    
    cd "$SCRIPT_DIR"
    
    # List generated files
    if [[ -d "$VISUAL_DIR/cpp_output" ]]; then
        log_info "Generated C++ visual output files:"
        ls -la "$VISUAL_DIR/cpp_output"/*.png 2>/dev/null | awk '{print "  " $9 " (" $5 " bytes)"}' || echo "  (none)"
        echo ""
    fi
    
    return $TEST_RESULT
}

# Print summary
print_summary() {
    local json_passed=$1
    local json_failed=$2
    local visual_passed=$3
    local visual_failed=$4
    
    echo "================================================"
    echo "  Fidelity Test Summary"
    echo "================================================"
    echo ""
    
    if [[ "$RUN_JSON" == true ]]; then
        echo "JSON Tests:"
        if [[ $json_failed -eq 0 ]]; then
            log_success "  All JSON tests passed!"
        else
            log_error "  $json_failed JSON test(s) failed"
        fi
        echo ""
    fi
    
    if [[ "$RUN_VISUAL" == true ]]; then
        echo "Visual Tests:"
        if [[ $visual_failed -eq 0 ]]; then
            log_success "  All visual tests passed!"
        else
            log_error "  $visual_failed visual test(s) failed"
        fi
        echo ""
    fi
    
    # List output locations
    if [[ "$RUN_JSON" == true ]]; then
        echo "JSON goldens: $GOLDENS_DIR"
    fi
    if [[ "$RUN_VISUAL" == true ]]; then
        echo "Visual goldens: $VISUAL_DIR/flutter_goldens"
        echo "Visual output:  $VISUAL_DIR/cpp_output"
    fi
    echo ""
    
    if [[ $json_failed -gt 0 || $visual_failed -gt 0 ]]; then
        exit 1
    fi
}

# Parse test output for counts
count_tests() {
    local output="$1"
    
    PASSED=$(echo "$output" | grep -oE "\[  PASSED  \] [0-9]+ test" | grep -oE "[0-9]+" | head -1 || echo "0")
    FAILED=$(echo "$output" | grep -oE "\[  FAILED  \] [0-9]+ test" | grep -oE "[0-9]+" | head -1 || echo "0")
    
    if [[ "$PASSED" == "0" && "$FAILED" == "0" ]]; then
        PASSED=$(echo "$output" | grep -oE "[0-9]+ passed" | grep -oE "[0-9]+" | head -1 || echo "0")
        FAILED=$(echo "$output" | grep -oE "[0-9]+ failed" | grep -oE "[0-9]+" | head -1 || echo "0")
    fi
    
    echo "$PASSED $FAILED"
}

# Main execution
main() {
    local start_time=$(date +%s)
    
    check_prerequisites
    
    # Generate Flutter goldens
    if [[ "$RUN_JSON" == true ]]; then
        generate_flutter_json_goldens
    fi
    if [[ "$RUN_VISUAL" == true ]]; then
        generate_flutter_visual_goldens
    fi
    
    build_cpp_tests
    
    # Run tests
    JSON_PASSED=0
    JSON_FAILED=0
    VISUAL_PASSED=0
    VISUAL_FAILED=0
    
    if [[ "$RUN_JSON" == true ]]; then
        JSON_OUTPUT=$(run_json_tests 2>&1) || true
        echo "$JSON_OUTPUT"
        read JSON_PASSED JSON_FAILED <<< $(count_tests "$JSON_OUTPUT")
    fi
    
    if [[ "$RUN_VISUAL" == true ]]; then
        VISUAL_OUTPUT=$(run_visual_tests 2>&1) || true
        echo "$VISUAL_OUTPUT"
        read VISUAL_PASSED VISUAL_FAILED <<< $(count_tests "$VISUAL_OUTPUT")
    fi
    
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    # Print summary
    print_summary "$JSON_PASSED" "$JSON_FAILED" "$VISUAL_PASSED" "$VISUAL_FAILED"
    
    echo "Time elapsed: ${duration}s"
    echo ""
}

# Run main
main
