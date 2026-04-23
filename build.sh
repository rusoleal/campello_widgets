#!/usr/bin/env bash
set -e

PLATFORM=${1:-$(uname -s | tr '[:upper:]' '[:lower:]')}
BUILD_DIR="build/${PLATFORM}-release"

# Only configure if the build directory doesn't exist yet.
# cmake --build will auto-reconfigure when CMakeLists.txt changes.
if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ]; then
    cmake -S . -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_EXAMPLES=ON
fi

cmake --build "${BUILD_DIR}" --config Release
