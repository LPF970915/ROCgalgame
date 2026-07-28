#!/bin/sh
set -u

APP_DIR="${APP_DIR:-/storage/roms/ports/ROCgalgame}"
PROJECT="${PROJECT:?PROJECT must point to an XP3 archive}"
PROJECT_WORKDIR="${PROJECT_WORKDIR:-$(dirname "$PROJECT")}"
DISPLAY="${DISPLAY:-:3}"
DISPLAY_BACKEND="${ROCGALGAME_KRKR_DISPLAY_BACKEND:-xwayland}"
PROOF_FILE="${PROOF_FILE:-/tmp/krkr2-presentation-proof.png}"
OUTPUT_FILE="${OUTPUT_FILE:-/tmp/krkr2-presentation-proof.out}"
GRIM_OUTPUT="${GRIM_OUTPUT:-/tmp/krkr2-presentation-grim.out}"

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/0-runtime-dir}"
export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-1}"
export SWAYSOCK="${SWAYSOCK:-/run/0-runtime-dir/sway-ipc.0.sock}"

rm -f "$PROOF_FILE" "$OUTPUT_FILE" "$GRIM_OUTPUT"
APP_DIR="$APP_DIR" \
PROJECT="$PROJECT" \
PROJECT_WORKDIR="$PROJECT_WORKDIR" \
TEST_NAME=krkr2-presentation-proof \
LOG_DIR="$APP_DIR/logs/krkr2-presentation-proof" \
RUN_SECONDS="${RUN_SECONDS:-35}" \
EXPECT_TJS_MARKER= \
REQUIRE_HARDWARE=1 \
ROCGALGAME_KRKR_DISPLAY_BACKEND="$DISPLAY_BACKEND" \
DISPLAY="$DISPLAY" \
/bin/sh /tmp/run_krkr2_minimal_test.sh >"$OUTPUT_FILE" 2>&1 &
test_pid=$!

sleep 12
if [ "$DISPLAY_BACKEND" = wayland ]; then
  window_selector='[title="krkr2"]'
else
  window_selector="[title=\"Xwayland on $DISPLAY\"]"
fi
swaymsg -r -s "$SWAYSOCK" '[app_id="emulationstation"] fullscreen disable' \
  >>"$GRIM_OUTPUT" 2>&1 || true
swaymsg -r -s "$SWAYSOCK" "$window_selector focus" \
  >>"$GRIM_OUTPUT" 2>&1 || true
swaymsg -r -s "$SWAYSOCK" "$window_selector fullscreen enable" \
  >>"$GRIM_OUTPUT" 2>&1 || true
swaymsg -s "$SWAYSOCK" -t get_tree >"$PROOF_FILE.sway-tree.json" 2>&1 || true
sleep 2
grim "$PROOF_FILE" >"$GRIM_OUTPUT" 2>&1
grim_status=$?
capture_backend=grim
if [ "$grim_status" -ne 0 ] && [ -r /dev/fb0 ] && command -v ffmpeg >/dev/null 2>&1; then
  capture_backend=fbdev
  ffmpeg -hide_banner -loglevel error -f fbdev -framerate 1 -i /dev/fb0 \
    -frames:v 1 -y "$PROOF_FILE" >>"$GRIM_OUTPUT" 2>&1
  capture_status=$?
else
  capture_status=$grim_status
fi
if [ "$capture_status" -ne 0 ] && [ -r /dev/fb0 ]; then
  capture_backend=fbdev-raw
  dd if=/dev/fb0 of="$PROOF_FILE.raw" bs=5760 count=1600 \
    >>"$GRIM_OUTPUT" 2>&1
  capture_status=$?
fi

wait "$test_pid"
test_status=$?
echo "grim_status=$grim_status"
echo "capture_backend=$capture_backend"
echo "capture_status=$capture_status"
echo "test_status=$test_status"
ls -l "$PROOF_FILE" 2>/dev/null || true
ls -l "$PROOF_FILE.raw" 2>/dev/null || true
cat "$GRIM_OUTPUT"
tail -n 30 "$OUTPUT_FILE"
exit "$test_status"
