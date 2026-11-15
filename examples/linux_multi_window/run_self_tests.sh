#!/usr/bin/env bash
# Runs the multi-window example's self-tests (CW_MULTIWINDOW_DEMO=2, 3, 4) on
# a virtual display, and fails if any check fails or the app doesn't quit.
#
#   run_self_tests.sh <path/to/campello_widgets_multi_window> x11|wayland [out_dir]
#
#   x11      Xvfb (xvfb-run). Real X input through XTest.
#   wayland  a headless Weston. Input injected into each window's View (see
#            main.cpp). Needs weston and a libdecor plugin.
#
# Without a GPU, point Vulkan at lavapipe, e.g.
#   VK_ICD_FILENAMES=$(ls /usr/share/vulkan/icd.d/lvp_icd*.json | head -1)
# With out_dir, each run's output (and the captures) are kept there.
set -u

BIN=$(realpath "$1")
BACKEND=$2
OUT=${3:-$(mktemp -d)}
mkdir -p "$OUT"
OUT=$(realpath "$OUT")

WESTON_PID=
cleanup() { [ -n "$WESTON_PID" ] && kill "$WESTON_PID" 2>/dev/null; }
trap cleanup EXIT

if [ "$BACKEND" = wayland ]; then
    export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-$(mktemp -d)}
    chmod 700 "$XDG_RUNTIME_DIR"
    SOCKET=cw-self-test-$$
    weston --backend=headless --renderer=pixman --width=1280 --height=900 \
           --socket="$SOCKET" --idle-time=0 > "$OUT/weston.log" 2>&1 &
    WESTON_PID=$!
    for _ in $(seq 100); do [ -S "$XDG_RUNTIME_DIR/$SOCKET" ] && break; sleep 0.1; done
    if [ ! -S "$XDG_RUNTIME_DIR/$SOCKET" ]; then
        echo "weston didn't start:"; cat "$OUT/weston.log"; exit 1
    fi
    export WAYLAND_DISPLAY=$SOCKET
elif [ "$BACKEND" != x11 ]; then
    echo "usage: $0 <binary> x11|wayland [out_dir]"; exit 2
fi

failed=0
for mode in 2 3 4; do
    dir="$OUT/demo$mode"
    mkdir -p "$dir"
    if [ "$BACKEND" = x11 ]; then
        env -u WAYLAND_DISPLAY CW_MULTIWINDOW_DEMO=$mode CW_MULTIWINDOW_DEMO_DIR="$dir" \
            timeout 120 xvfb-run -a -s "-screen 0 1600x1200x24" "$BIN" > "$dir/app.log" 2>&1
    else
        CW_MULTIWINDOW_DEMO=$mode CW_MULTIWINDOW_DEMO_DIR="$dir" \
            timeout 120 "$BIN" > "$dir/app.log" 2>&1
    fi
    status=$?
    if [ $status -eq 0 ] && grep -q "SELF-TEST PASSED" "$dir/app.log"; then
        echo "[$BACKEND] DEMO $mode: passed"
    else
        echo "[$BACKEND] DEMO $mode: FAILED (exit $status)"
        grep "^\[demo\]\|CRASH\|error" "$dir/app.log"
        failed=1
    fi
done
exit $failed
