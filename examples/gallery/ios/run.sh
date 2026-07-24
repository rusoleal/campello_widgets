#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
cd "${ROOT_DIR}"

BUNDLE_ID="systems.leal.campello-widgets-gallery"

# Development team used for automatic code signing on a physical device —
# already cached in build/ios-device/CMakeCache.txt from prior manual
# setup; passed explicitly here so a from-scratch configure reproduces it.
DEVELOPMENT_TEAM="SJ5Y59W7JK"

# A connected/paired physical device shows up as an "available" row here;
# a device that's known but not currently reachable does not.
DEVICE_UDID=$(xcrun devicectl list devices --hide-headers 2>/dev/null \
    | grep "available" \
    | grep -oE "[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}" \
    | head -1 || true)

if [ -n "${DEVICE_UDID}" ]; then
    echo "Connected iOS device found (${DEVICE_UDID}) — building for device."

    BUILD_DIR="build/ios-device"
    APP_PATH="${BUILD_DIR}/examples/gallery/ios/Debug-iphoneos/campello_widgets_gallery.app"

    if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ]; then
        cmake -S . -B "${BUILD_DIR}" \
            -G Xcode \
            -DCMAKE_SYSTEM_NAME=iOS \
            -DCMAKE_OSX_SYSROOT=iphoneos \
            -DBUILD_EXAMPLES=ON \
            -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_STYLE=Automatic \
            -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM="${DEVELOPMENT_TEAM}"
    fi

    cmake --build "${BUILD_DIR}" --target campello_widgets_gallery -- -sdk iphoneos

    # A stale install signed under a different provisioning state (e.g. a
    # prior ad-hoc/dev-suffixed identifier) makes devicectl reject the
    # install as a "mismatched application-identifier" upgrade — uninstall
    # first (ignoring failure when nothing is installed yet) so this is
    # always a clean install.
    xcrun devicectl device uninstall app --device "${DEVICE_UDID}" "${BUNDLE_ID}" >/dev/null 2>&1 || true

    xcrun devicectl device install app --device "${DEVICE_UDID}" "${APP_PATH}"
    xcrun devicectl device process launch --device "${DEVICE_UDID}" "${BUNDLE_ID}"
else
    echo "No connected iOS device found — falling back to the Simulator."

    BUILD_DIR="build/ios-sim"
    APP_PATH="${BUILD_DIR}/examples/gallery/ios/Debug-iphonesimulator/campello_widgets_gallery.app"

    if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ]; then
        cmake -S . -B "${BUILD_DIR}" \
            -G Xcode \
            -DCMAKE_SYSTEM_NAME=iOS \
            -DCMAKE_OSX_SYSROOT=iphonesimulator \
            -DBUILD_EXAMPLES=ON
    fi

    cmake --build "${BUILD_DIR}" --target campello_widgets_gallery -- -sdk iphonesimulator

    # Reuse an already-booted simulator if there is one; otherwise boot the
    # first available iPhone runtime device.
    SIM_UDID=$(xcrun simctl list devices | grep -m1 "(Booted)" | grep -oE "[0-9A-F-]{36}" || true)
    if [ -z "${SIM_UDID}" ]; then
        SIM_UDID=$(xcrun simctl list devices available iPhone | grep -m1 -oE "[0-9A-F-]{36}")
        xcrun simctl boot "${SIM_UDID}"
        open -a Simulator
    fi

    xcrun simctl install "${SIM_UDID}" "${APP_PATH}"
    xcrun simctl launch "${SIM_UDID}" "${BUNDLE_ID}"

    open -a Simulator
fi
