#!/usr/bin/env bash
set -e

BUILD_DIR="build/darwin-debug"
APP_PATH="${BUILD_DIR}/campello_widgets_image.app"

if [ ! -f "build/darwin-debug/CMakeCache.txt" ]; then
    cmake -S . -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DBUILD_EXAMPLES=ON
fi

cmake --build "${BUILD_DIR}" --config Debug --target campello_widgets_image

open "${APP_PATH}"
