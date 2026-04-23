#!/usr/bin/env bash
set -e

BUILD_DIR="build/darwin-release"
APP_PATH="${BUILD_DIR}/examples/macos_counter/campello_widgets_counter.app"

if [ ! -f "build/darwin-release/CMakeCache.txt" ]; then
    cmake -S . -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_EXAMPLES=ON
fi

cmake --build "${BUILD_DIR}" --config Release

open "${APP_PATH}"
