#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Run Linux CI build + tests inside a Docker container that mirrors the
# GitHub Actions ubuntu-24.04 environment.
#
# This is useful for reproducing Linux-only failures (e.g. GCC-specific
# codegen issues, unity-build batch ordering differences) on macOS or
# Windows development machines.
# ---------------------------------------------------------------------------
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

BUILD_TYPE="${1:-Debug}"
BUILD_DIR="build/linux_docker_${BUILD_TYPE}"
IMAGE_TAG="campello_widgets:linux"

# ---------------------------------------------------------------------------
# Build the image if it doesn't exist yet
# ---------------------------------------------------------------------------
if ! docker image inspect "${IMAGE_TAG}" >/dev/null 2>&1; then
    echo "[docker_test] Building Docker image ${IMAGE_TAG} ..."
    docker build -t "${IMAGE_TAG}" -f "${SCRIPT_DIR}/Dockerfile" "${PROJECT_ROOT}"
fi

# ---------------------------------------------------------------------------
# Run configure + build + test inside the container
# ---------------------------------------------------------------------------
echo "[docker_test] Build type: ${BUILD_TYPE}"
echo "[docker_test] Build dir : ${BUILD_DIR}"

docker run --rm \
    -v "${PROJECT_ROOT}:/workspace" \
    -w /workspace \
    "${IMAGE_TAG}" \
    bash -c "
        set -e
        echo '--- Versions ---'
        cmake --version
        gcc --version
        echo '--- Configure ---'
        cmake -S . -B '${BUILD_DIR}' \
            -DCMAKE_BUILD_TYPE='${BUILD_TYPE}' \
            -DBUILD_TESTS=ON \
            -G Ninja
        echo '--- Build ---'
        cmake --build '${BUILD_DIR}' --config '${BUILD_TYPE}' -j\$(nproc)
        echo '--- Test ---'
        ctest --test-dir '${BUILD_DIR}' \
            --output-on-failure \
            -C '${BUILD_TYPE}' \
            -E 'campello_input'
    "

echo "[docker_test] Done."
