#!/bin/sh
set -u

APP_DIR="${APP_DIR:-/storage/roms/ports/ROCgalgame}"
CORE="${CORE:-$APP_DIR/cores/krkr/krkr2.candidate-kag-emb-escape}"
CASE_DIR="${CASE_DIR:-/tmp/rocgalgame-krkr2-library-sweep-20260729-031822/game-58d6eb3263eb}"
PROBE="${PROBE:-$CASE_DIR/startup-state-probe.tjs}"
SAVE_DIR="${SAVE_DIR:-/tmp/krkr2-kag-emb-state-save}"
LOG="${LOG:-$APP_DIR/logs/krkr2-kag-emb-state-probe.log}"
TREE="${TREE:-$APP_DIR/logs/krkr2-kag-emb-state-probe-tree.json}"
SCREENSHOT="${SCREENSHOT:-$APP_DIR/logs/krkr2-kag-emb-state-probe.png}"
RUN_SECONDS="${RUN_SECONDS:-18}"
CAPTURE_SECONDS="${CAPTURE_SECONDS:-1 3 5 8 12 18}"
DEBUG_ARG="${DEBUG_ARG:--debug=yes}"

test -x "$CORE"
test -s "$CASE_DIR/data.xp3"
test -s "$PROBE"

if pgrep -x rocgalgame_sdl >/dev/null 2>&1 || pgrep -x krkr2 >/dev/null 2>&1; then
  echo "[kag-emb-probe] refusing to interrupt an active frontend/core" >&2
  exit 2
fi

mkdir -p "$SAVE_DIR" "$(dirname "$LOG")"

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/0-runtime-dir}"
export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-1}"
export SWAYSOCK="${SWAYSOCK:-/run/0-runtime-dir/sway-ipc.0.sock}"
export GDK_BACKEND=wayland
export SDL_VIDEODRIVER=wayland
export ROCGALGAME_KRKR_RUNTIME=krkr2
export ROCGALGAME_KRKR_DISPLAY_BACKEND=wayland
export ROCGALGAME_KRKR_VIRTUAL_MOUSE=1
export ROCGALGAME_KRKR_SWAP_AB=1
export ROCGALGAME_INPUT_PROFILE=gkd350h-ultra
export ROCGALGAME_MOUSE_SPEED=1080
export ROCGALGAME_MOUSE_ACCEL=1.0
export ROCGALGAME_KRKR_SAVE_PATH="$SAVE_DIR"
export ROCGALGAME_KRKR_PRESENTATION_PROBE=1
export LD_LIBRARY_PATH="/usr/lib/mali:$APP_DIR/cores/krkr/lib_krkr2:$APP_DIR/lib_system_sdl:$APP_DIR/lib:/usr/lib:/lib:/mnt/vendor/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
unset DISPLAY

cd "$CASE_DIR" || exit 2
"$CORE" "$CASE_DIR/data.xp3" "-startup=$PROBE" "$DEBUG_ARG" >"$LOG" 2>&1 &
pid=$!

elapsed=0
for capture_at in $CAPTURE_SECONDS; do
  if [ "$capture_at" -gt "$RUN_SECONDS" ]; then
    continue
  fi
  delta=$((capture_at - elapsed))
  if [ "$delta" -gt 0 ]; then
    sleep "$delta"
  fi
  elapsed="$capture_at"
  if command -v grim >/dev/null 2>&1; then
    grim "${SCREENSHOT%.png}-${capture_at}s.png" 2>/dev/null || true
  fi
done
if [ "$elapsed" -lt "$RUN_SECONDS" ]; then
  sleep $((RUN_SECONDS - elapsed))
fi

alive=0
if kill -0 "$pid" 2>/dev/null; then
  alive=1
fi

if command -v swaymsg >/dev/null 2>&1 && [ -S "$SWAYSOCK" ]; then
  swaymsg -s "$SWAYSOCK" -t get_tree >"$TREE" 2>&1 || true
fi
if command -v grim >/dev/null 2>&1; then
  grim "$SCREENSHOT" 2>/dev/null || true
fi

if [ "$alive" -eq 1 ]; then
  kill -TERM "$pid" 2>/dev/null || true
  sleep 2
  if kill -0 "$pid" 2>/dev/null; then
    kill -KILL "$pid" 2>/dev/null || true
  fi
fi
wait "$pid" 2>/dev/null
exit_code=$?

echo "[kag-emb-probe] alive_after_${RUN_SECONDS}s=$alive exit_code=$exit_code"
grep -E 'STATE_PROBE (wrapper_after_startup|kag_state|conductor|hooks|storage|custom|macros)|Startup script ended|An exception occurred|Unhandled exception|Segmentation fault|perf fps=' "$LOG" | tail -n 80 || true
