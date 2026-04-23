#!/usr/bin/env bash
set -e

BUILD_DIR="build/darwin-debug"
APP_PATH="${BUILD_DIR}/examples/macos_counter/campello_widgets_counter.app"

if [ ! -f "build/darwin-debug/CMakeCache.txt" ]; then
    cmake -S . -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DBUILD_EXAMPLES=ON
fi

cmake --build "${BUILD_DIR}" --config Debug

open "${APP_PATH}"
