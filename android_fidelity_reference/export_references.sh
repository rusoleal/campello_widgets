#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

export ANDROID_HOME="${ANDROID_HOME:-$HOME/Library/Android/sdk}"
export JAVA_HOME="${JAVA_HOME:-/Applications/Android Studio.app/Contents/jbr/Contents/Home}"
ADB="${ANDROID_HOME}/platform-tools/adb"

PACKAGE="systems.leal.fidelityreference"
OUTPUT_DIR="${SCRIPT_DIR}/output"

echo "Building FidelityReference..."
./gradlew :app:assembleDebug --console=plain > /tmp/android_fidelity_reference_build.log 2>&1
APK="${SCRIPT_DIR}/app/build/outputs/apk/debug/app-debug.apk"
if [ ! -f "${APK}" ]; then
    echo "ERROR: build failed, see /tmp/android_fidelity_reference_build.log"
    exit 1
fi

if ! "${ADB}" get-state 2>/dev/null | grep -q device; then
    echo "ERROR: no booted device/emulator found (adb get-state)"
    exit 1
fi

# Suppresses the one-time "swipe down to exit fullscreen" system overlay
# that otherwise covers the top third of the first screenshot after any
# app requests hidden system bars — a real system dialog, not app content,
# and not something a fidelity comparison should be measuring.
"${ADB}" shell settings put secure immersive_mode_confirmations confirmed

echo "Installing..."
"${ADB}" install -r "${APK}" > /dev/null

# {builder}_{state} case ids — mirrors themed_component_harness.cpp's
# "button"/"switch"/"card" builder blocks exactly (same content/labels),
# per ComponentCatalog.kt's `when` cases.
CASES="button_primary button_secondary button_tertiary button_danger button_disabled switch_on switch_off switch_disabled card_elevated card_filled card_outlined slider_value slider_disabled chip_unselected chip_selected divider_default divider_indented listTile_one_line listTile_two_line textField_empty textField_filled textField_disabled segmentedButton_three_segments dialog_one_action dialog_two_actions dialog_three_actions tabBar_two_tabs dropdownButton_closed toggleButtons_multi popupMenuButton_closed popupMenuButton_open badge_dot badge_number iconButton_plain iconButton_filled iconButton_selected navigationRail_compact"
THEMES="expressive_light expressive_dark"

# Hiding system bars (MainActivity's WindowInsetsControllerCompat call) does
# not reliably stay hidden across repeated `am start` cycles in this
# harness's launch/screenshot/force-stop loop — the status/nav bars reappear
# in the actual screencap even though the app requested them hidden. Rather
# than fight window-manager timing, capture the full screen and crop the
# system bars out here, using the real inset heights this specific emulator
# reports (`adb shell dumpsys window displays` -> statusBars/navigationBars
# InsetsSource frames), the same crop-not-guess approach used for iOS's
# real-captured dialog references.
STATUS_BAR_PX=136
NAV_BAR_PX=63

rm -rf "${OUTPUT_DIR}"
for theme in ${THEMES}; do
    mkdir -p "${OUTPUT_DIR}/${theme}"
    for case_id in ${CASES}; do
        "${ADB}" shell am force-stop "${PACKAGE}" >/dev/null 2>&1 || true
        "${ADB}" shell am start -n "${PACKAGE}/.MainActivity" --es case "${case_id}" --es theme "${theme}" > /dev/null
        # Android's mandatory splash-screen API (12+) shows the app's icon
        # centered over a colored disc while the Activity cold-starts —
        # exactly where every centered test component also sits. 1.5s
        # wasn't consistently enough for a fresh `am start` after
        # force-stop to clear it before the screenshot; confirmed by
        # inspecting a captured reference showing the bare Android-robot
        # splash icon instead of real app content.
        sleep 3

        "${ADB}" shell screencap -p /sdcard/fidelity_case.png
        raw_path="/tmp/android_fidelity_raw_${theme}_${case_id}.png"
        "${ADB}" pull /sdcard/fidelity_case.png "${raw_path}" > /dev/null
        python3 -c "
from PIL import Image
img = Image.open('${raw_path}')
w, h = img.size
img.crop((0, ${STATUS_BAR_PX}, w, h - ${NAV_BAR_PX})).save('${OUTPUT_DIR}/${theme}/${case_id}.png')
"
        rm -f "${raw_path}"
        echo "Captured ${theme}/${case_id}.png"
    done
done
"${ADB}" shell am force-stop "${PACKAGE}" >/dev/null 2>&1 || true

echo "Exported $(find "${OUTPUT_DIR}" -name '*.png' | wc -l) reference screenshots to ${OUTPUT_DIR}"
