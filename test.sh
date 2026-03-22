#!/usr/bin/env bash
set -e

PLATFORM=${1:-$(uname -s | tr '[:upper:]' '[:lower:]')}
BUILD_DIR="build/${PLATFORM}_test"

cmake -S . -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTS=ON \
    -DBUILD_INTEGRATION_TESTS=${INTEGRATION:-OFF}

cmake --build "${BUILD_DIR}" --config Debug
ctest --test-dir "${BUILD_DIR}" --output-on-failure
