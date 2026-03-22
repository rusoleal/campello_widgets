#!/usr/bin/env bash
set -e

PLATFORM=${1:-$(uname -s | tr '[:upper:]' '[:lower:]')}
BUILD_DIR="build/${PLATFORM}"

cmake -S . -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_EXAMPLES=ON

cmake --build "${BUILD_DIR}" --config Release
