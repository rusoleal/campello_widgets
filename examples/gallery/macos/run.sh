#!/usr/bin/env bash
set -e

MODE="${1:-debug}"

case "${MODE}" in
    debug)   BUILD_TYPE=Debug ;;
    release) BUILD_TYPE=Release ;;
    *)
        echo "Usage: $0 [debug|release]" >&2
        exit 1
        ;;
esac

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${ROOT}/build/darwin-${MODE}"

CACHED_TYPE="$(cmake -L -N "${BUILD_DIR}" 2>/dev/null | grep CMAKE_BUILD_TYPE | cut -d= -f2)"
if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ] || [ "${CACHED_TYPE}" != "${BUILD_TYPE}" ]; then
    cmake -S "${ROOT}" -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DBUILD_EXAMPLES=ON
fi

cmake --build "${BUILD_DIR}" --config "${BUILD_TYPE}" --target campello_widgets_gallery

open "${BUILD_DIR}/examples/gallery/macos/campello_widgets_gallery.app"
