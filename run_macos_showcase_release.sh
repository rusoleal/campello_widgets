#!/usr/bin/env bash
set -e

BUILD_DIR="build/darwin-release"
APP_PATH="${BUILD_DIR}/examples/macos_showcase/campello_widgets_showcase.app"

if [ ! -f "build/darwin-release/CMakeCache.txt" ]; then
    cmake -S . -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_EXAMPLES=ON
fi

cmake --build "${BUILD_DIR}" --config Release

"${APP_PATH}/Contents/MacOS/campello_widgets_showcase"
