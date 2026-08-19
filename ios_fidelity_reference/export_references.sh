#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${SCRIPT_DIR}"

BUNDLE_ID="systems.leal.FidelityReference"
OUTPUT_DIR="${SCRIPT_DIR}/output"

# Build once. Deployment target is 16.0, so a single Simulator build runs
# unmodified on both OS versions below — #available(iOS 26.0, *) checks
# resolve correctly against whichever OS actually launches it.
echo "Building FidelityReference for iOS Simulator..."
xcodebuild -project FidelityReference.xcodeproj \
    -scheme FidelityReference \
    -sdk iphonesimulator \
    -destination 'platform=iOS Simulator,name=iPhone 17 Pro' \
    clean build > /tmp/fidelity_reference_build.log 2>&1

APP_PATH=$(find "${HOME}/Library/Developer/Xcode/DerivedData" \
    -path "*/Build/Products/Debug-iphonesimulator/FidelityReference.app" \
    -type d -print -quit)

if [ -z "${APP_PATH}" ]; then
    echo "ERROR: Could not find built FidelityReference.app"
    exit 1
fi

echo "Built app: ${APP_PATH}"

# Copy shared Liquid Glass background into the app bundle so the reference app
# can load it as a bundled resource.
BG_SRC="${ROOT_DIR}/tests/visual_fidelity/test_images/liquid_glass_background.png"
BG_DST="${APP_PATH}/liquid_glass_background.png"
if [ -f "${BG_SRC}" ]; then
    cp "${BG_SRC}" "${BG_DST}"
    echo "Copied liquid glass background to app bundle"
else
    echo "WARNING: Liquid glass background not found at ${BG_SRC}"
fi

# Finds an available simulator by device name + runtime-version prefix (e.g.
# "iPhone 16 Pro" / "iOS-18") — name-scoped rather than a hardcoded UDID,
# since UDIDs aren't stable across machines/simulator re-creation. Runtime
# keys look like "com.apple.CoreSimulator.SimRuntime.iOS-18-6" (dashes, not
# dots), so the prefix argument must match that form.
find_simulator_udid() {
    local device_name="$1" runtime_prefix="$2"
    xcrun simctl list devices available -j | python3 -c "
import json, sys
data = json.load(sys.stdin)
name, prefix = sys.argv[1], sys.argv[2]
for runtime, devices in data['devices'].items():
    if prefix not in runtime:
        continue
    for d in devices:
        if d['name'] == name:
            print(d['udid'])
            sys.exit(0)
" "${device_name}" "${runtime_prefix}"
}

boot_if_needed() {
    local udid="$1"
    local state
    state=$(xcrun simctl list devices | grep -F "${udid}" | grep -oE '\(Booted\)|\(Shutdown\)' || true)
    if [ "${state}" != "(Booted)" ]; then
        echo "Booting simulator ${udid}..."
        xcrun simctl boot "${udid}"
    fi
}

# Runs one export pass on the given simulator, exporting only the given
# theme pair, and merges its output into OUTPUT_DIR. Each pass only ever
# writes its own theme subdirectories, so merging is conflict-free.
run_export_pass() {
    local device_name="$1" runtime_prefix="$2" themes_arg="$3"

    local udid
    udid=$(find_simulator_udid "${device_name}" "${runtime_prefix}")
    if [ -z "${udid}" ]; then
        echo "ERROR: No available simulator matching name='${device_name}' runtime~='${runtime_prefix}'"
        exit 1
    fi
    echo "=== ${device_name} (runtime ~${runtime_prefix}), themes=${themes_arg} -> ${udid} ==="

    boot_if_needed "${udid}"
    xcrun simctl install "${udid}" "${APP_PATH}"

    # Clear any previous output container to avoid pulling stale data.
    local data_container
    data_container=$(xcrun simctl get_app_container "${udid}" "${BUNDLE_ID}" data 2>/dev/null || true)
    if [ -n "${data_container}" ]; then
        rm -rf "${data_container}/Documents/fidelity_output"
    fi

    echo "Launching app to export reference screenshots (${themes_arg})..."
    xcrun simctl launch --console-pty "${udid}" "${BUNDLE_ID}" "--themes=${themes_arg}" \
        | tee "/tmp/fidelity_reference_launch_${themes_arg//,/_}.log"

    # Wait until the process exits (it calls exit(0) when done).
    while xcrun simctl spawn "${udid}" launchctl list | grep -q "${BUNDLE_ID}"; do
        sleep 0.5
    done

    data_container=$(xcrun simctl get_app_container "${udid}" "${BUNDLE_ID}" data)
    local src_dir="${data_container}/Documents/fidelity_output"
    if [ ! -d "${src_dir}" ]; then
        echo "ERROR: No fidelity_output found at ${src_dir}"
        exit 1
    fi

    mkdir -p "${OUTPUT_DIR}"
    cp -R "${src_dir}/"* "${OUTPUT_DIR}/"
    echo "Merged $(find "${src_dir}" -name '*.png' | wc -l) screenshots from this pass into ${OUTPUT_DIR}"

    run_real_capture_pass "${udid}" "${themes_arg}"
}

# Builders whose reference is a genuinely live-presented system control
# (see RealCapture.swift) rather than a batch-exported offscreen snapshot,
# and each one's states — mirrors ComponentCatalog.swift's states().
REAL_CAPTURE_BUILDERS="dialog actionSheet searchField navigationBar"
real_capture_states() {
    case "$1" in
        dialog) echo "one_action two_actions three_actions" ;;
        actionSheet) echo "open" ;;
        searchField) echo "empty filled" ;;
        navigationBar) echo "three_items" ;;
        *) echo "" ;;
    esac
}

# Captures one real-presented case: launch (non-blocking — `--console-pty`
# would block forever since this process idles instead of exiting),
# briefly wait for presentation to settle, screenshot the whole simulator
# screen externally, terminate, then crop away the status bar/home
# indicator using the exact safe-area insets the app itself measured and
# wrote to Documents/safe_area.txt (a guessed fixed margin would silently
# drift if the device/OS changes). Overwrites that case's batch-exported
# mockup PNG with the real capture.
run_real_capture_case() {
    local udid="$1" theme="$2" builder="$3" state="$4"
    local case_id="${builder}_${state}"
    local out_path="${OUTPUT_DIR}/${theme}/${case_id}.png"

    xcrun simctl terminate "${udid}" "${BUNDLE_ID}" >/dev/null 2>&1 || true
    local data_container
    data_container=$(xcrun simctl get_app_container "${udid}" "${BUNDLE_ID}" data 2>/dev/null || true)
    if [ -n "${data_container}" ]; then
        rm -f "${data_container}/Documents/safe_area.txt"
    fi

    xcrun simctl launch "${udid}" "${BUNDLE_ID}" "--present-case=${theme}:${case_id}" > /dev/null
    # navigationRail's UISplitViewController is heavier to lay out than
    # the other real-capture builders (replaces window.rootViewController
    # entirely, activating real column-layout machinery) — 1.5s wasn't
    # consistently enough: 3 of 4 iPad captures in one run came back
    # showing only the backdrop with no sidebar rendered at all, while
    # the same construction reliably works when launched manually with
    # natural pauses between commands.
    #
    # navigationBar's real UITabBar under Liquid Glass has the same
    # settle-time problem: UIGlassEffect compositing hadn't finished by
    # 1.5s on a cold app launch, producing either a blank screenshot or a
    # too-dark/uncomposited-looking pill (confirmed by re-running the same
    # capture at 3.0s, which came back correctly light-tinted). Classic
    # (non-glass) navigationBar and the other three real-capture builders
    # haven't shown this failure mode, so they keep the shorter sleep.
    if [ "${builder}" = "navigationRail" ]; then
        sleep 3.5
    elif [ "${builder}" = "navigationBar" ]; then
        sleep 3.0
    else
        sleep 1.5
    fi

    local raw_path="/tmp/fidelity_real_${theme}_${case_id}_$$.png"
    xcrun simctl io "${udid}" screenshot "${raw_path}" > /dev/null

    xcrun simctl terminate "${udid}" "${BUNDLE_ID}" >/dev/null 2>&1 || true

    data_container=$(xcrun simctl get_app_container "${udid}" "${BUNDLE_ID}" data)
    local insets_file="${data_container}/Documents/safe_area.txt"
    local top=190 bottom=110
    if [ -f "${insets_file}" ]; then
        top=$(grep -o 'top=[0-9]*' "${insets_file}" | cut -d= -f2)
        bottom=$(grep -o 'bottom=[0-9]*' "${insets_file}" | cut -d= -f2)
    else
        echo "WARNING: ${insets_file} missing, using fallback crop margins"
    fi

    # navigationRail is real screen furniture (a UISplitViewController
    # sidebar), not a full-screen surface — only the rail portion at the
    # left edge matters, the detail column beside it is discarded. Widths
    # measured directly from a real capture with the detail column's
    # backdrop image actually loaded (the first measurement attempt used
    # a manual test run where that image had silently failed to load,
    # making the whole screen look uniformly blank/white and hiding the
    # true sidebar/detail boundary — this re-measurement used the
    # gray-sidebar-to-colorful-backdrop color transition instead, sampled
    # at multiple rows well below any icon/toolbar content, and lines up
    # almost exactly with 2x the requested 100pt/260pt column width
    # (RealCapture.swift's addNavigationRail), as expected for this
    # iPad's DPR of 2.
    local right_crop_py="w"
    if [ "${builder}" = "navigationRail" ]; then
        if [ "${state}" = "compact" ]; then
            right_crop_py="199"
        else
            right_crop_py="520"
        fi
    fi

    mkdir -p "$(dirname "${out_path}")"
    python3 -c "
from PIL import Image
img = Image.open('${raw_path}')
w, h = img.size
img.crop((0, ${top}, ${right_crop_py}, h - ${bottom})).save('${out_path}')
"
    rm -f "${raw_path}"
    echo "Real-captured ${theme}/${case_id}.png"
}

# navigationRail's real equivalent is a UISplitViewController with a
# `.sidebar`-appearance UICollectionView as its primary column (the same
# pattern Mail/Notes/Files use) — this only exists on iPad (a compact-
# width split view shows one column at a time, no rail), so it needs its
# own capture pass on an iPad simulator rather than folding into
# REAL_CAPTURE_BUILDERS/run_export_pass above, which are iPhone-only. The
# same app bundle installs and runs unmodified — Simulator builds aren't
# device-model-specific, only OS-version-specific — so no separate build
# is needed, just a separate install+launch target.
run_ipad_navigation_rail_pass() {
    local device_name="$1" runtime_prefix="$2" theme_light="$3" theme_dark="$4"

    local udid
    udid=$(find_simulator_udid "${device_name}" "${runtime_prefix}")
    if [ -z "${udid}" ]; then
        echo "ERROR: No available simulator matching name='${device_name}' runtime~='${runtime_prefix}'"
        exit 1
    fi
    echo "=== ${device_name} (runtime ~${runtime_prefix}) for navigationRail -> ${udid} ==="

    boot_if_needed "${udid}"
    xcrun simctl install "${udid}" "${APP_PATH}"

    local theme
    for theme in "${theme_light}" "${theme_dark}"; do
        local state
        for state in compact extended; do
            run_real_capture_case "${udid}" "${theme}" "navigationRail" "${state}"
        done
    done
}

# Runs every real-capture case for the given theme pair on the given
# simulator — called after that simulator's batch export pass so these
# PNGs overwrite the corresponding hand-mocked ones.
run_real_capture_pass() {
    local udid="$1" themes_csv="$2"
    local theme
    for theme in ${themes_csv//,/ }; do
        local builder
        for builder in ${REAL_CAPTURE_BUILDERS}; do
            local state
            for state in $(real_capture_states "${builder}"); do
                run_real_capture_case "${udid}" "${theme}" "${builder}" "${state}"
            done
        done
    done
}

rm -rf "${OUTPUT_DIR}"

# Classic (pre-Liquid-Glass) themes render on a real pre-iOS-26 simulator —
# genuine system chrome instead of guessing what it looks like.
run_export_pass "iPhone 16 Pro" "iOS-18" "cupertino_light,cupertino_dark"

# Liquid Glass themes render on iOS 26+, where UIGlassEffect/the redesigned
# system chrome actually exist.
run_export_pass "iPhone 17 Pro" "iOS-26" "liquid_glass_light,liquid_glass_dark"

# navigationRail: iPad-only real capture, one pass per OS version (same
# classic-vs-glass split as the iPhone passes above).
run_ipad_navigation_rail_pass "iPad Pro 11-inch (M4)" "iOS-18" "cupertino_light" "cupertino_dark"
run_ipad_navigation_rail_pass "iPad Pro 11-inch (M4)" "iOS-26" "liquid_glass_light" "liquid_glass_dark"

echo "Exported $(find "${OUTPUT_DIR}" -name '*.png' | wc -l) reference screenshots to ${OUTPUT_DIR}"
