#!/usr/bin/env bash
set -e

BUILD_DIR="build/darwin-debug"
APP_PATH="${BUILD_DIR}/examples/macos_showcase/campello_widgets_showcase.app"

# Only configure if the build directory doesn't exist yet.
# cmake --build will auto-reconfigure when CMakeLists.txt changes.
if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ]; then
    cmake -S . -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DBUILD_EXAMPLES=ON
fi

cmake --build "${BUILD_DIR}" --config Debug

# Enable Metal API Validation for debug builds
MTL_DEBUG_LAYER=1 \
MTL_DEBUG_LAYER_ERROR_MODE=assert \
MTL_DEBUG_LAYER_WARNING_MODE=assert \
"${APP_PATH}/Contents/MacOS/campello_widgets_showcase"
