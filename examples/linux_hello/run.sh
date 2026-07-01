#!/usr/bin/env bash
set -e

# Usage: run.sh [release]   (default: debug)
MODE="${1:-debug}"

case "${MODE}" in
    debug)   BUILD_TYPE=Debug ;;
    release) BUILD_TYPE=Release ;;
    *)
        echo "Usage: $0 [debug|release]" >&2
        exit 1
        ;;
esac

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="${ROOT}/build/linux-${MODE}"

# Configure if missing or if the cached build type doesn't match.
CACHED_TYPE="$(cmake -L -N "${BUILD_DIR}" 2>/dev/null | grep CMAKE_BUILD_TYPE | cut -d= -f2)"
if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ] || [ "${CACHED_TYPE}" != "${BUILD_TYPE}" ]; then
    cmake -S "${ROOT}" -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DBUILD_EXAMPLES=ON
fi

cmake --build "${BUILD_DIR}" --config "${BUILD_TYPE}"

"${BUILD_DIR}/campello_widgets_linux_hello"
