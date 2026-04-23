#!/usr/bin/env bash
#
# Run unit tests for campello_widgets
#
# Usage:
#   ./test.sh                     # Run all tests
#   ./test.sh --fidelity          # Run Flutter fidelity tests
#   INTEGRATION=ON ./test.sh      # Include platform integration tests
#

set -e

# Check for fidelity flag
if [[ "$1" == "--fidelity" ]]; then
    shift
    ./run_fidelity_tests.sh "$@"
    exit $?
fi

PLATFORM=${1:-$(uname -s | tr '[:upper:]' '[:lower:]')}
BUILD_DIR="build/${PLATFORM}-debug-test"

echo "Building tests..."
if [ ! -f "build/${PLATFORM}-debug-test/CMakeCache.txt" ]; then
    cmake -S . -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DBUILD_TESTS=ON \
        -DBUILD_INTEGRATION_TESTS=${INTEGRATION:-OFF}
fi

cmake --build "${BUILD_DIR}" --config Debug

echo ""
echo "Running tests..."
ctest --test-dir "${BUILD_DIR}" --output-on-failure

echo ""
echo "================================================"
echo "Tests complete!"
echo ""
echo "For Flutter fidelity testing, run:"
echo "  ./run_fidelity_tests.sh"
echo ""
echo "See also:"
echo "  ./run_cpp_tests_only.sh     # C++ tests only"
echo "  tests/FIDELITY_TESTING.md   # Fidelity framework docs"
echo "================================================"
