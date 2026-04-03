#!/usr/bin/env bash
set -e

BUILD_DIR="build/darwin"
APP_PATH="${BUILD_DIR}/examples/macos_table_view/campello_widgets_tableview.app"

cmake -S . -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_EXAMPLES=ON

cmake --build "${BUILD_DIR}" --config Debug

open "${APP_PATH}"
