#!/usr/bin/env bash
set -e

BUILD_DIR="build/darwin"
APP_PATH="${BUILD_DIR}/examples/macos_tree_view/campello_widgets_treeview.app"

cmake -S . -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_EXAMPLES=ON

cmake --build "${BUILD_DIR}" --config Release

open "${APP_PATH}"
