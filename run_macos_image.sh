#!/usr/bin/env bash
set -e

BUILD_DIR="build/darwin"
APP_PATH="${BUILD_DIR}/campello_widgets_image.app"

cmake -S . -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_EXAMPLES=ON

cmake --build "${BUILD_DIR}" --config Debug --target campello_widgets_image

open "${APP_PATH}"
