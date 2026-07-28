#!/bin/sh
set -u

APP_DIR="${APP_DIR:-/storage/roms/ports/ROCgalgame}"
PROJECT="${PROJECT:-$APP_DIR/games/NEKOPARA Vol.2/data.xp3}"
WORKDIR="${PROJECT_WORKDIR:-$(dirname "$PROJECT")}"
LOG_FILE="${LOG_FILE:-/tmp/krkrsdl2-wayland-protocol.log}"
RUN_SECONDS="${RUN_SECONDS:-8}"
SAVE_DIR="${SAVE_DIR:-/tmp/krkrsdl2-wayland-save}"

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/0-runtime-dir}"
export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-1}"
export SDL_VIDEODRIVER=wayland
export WAYLAND_DEBUG=1
export KRKRSDL2_PATH="${KRKRSDL2_PATH:-$WORKDIR}"

mkdir -p "$SAVE_DIR"
rm -f "$LOG_FILE"
cd "$WORKDIR"
"$APP_DIR/cores/krkr/krkrsdl2" "$PROJECT" \
  "-datapath=$SAVE_DIR" -contfreq=60 -drawthread=auto -gclim=96 \
  "-deffont=$APP_DIR/fonts/ui_font_02.ttf" -nosel >"$LOG_FILE" 2>&1 &
pid=$!
sleep "$RUN_SECONDS"
alive=0
if kill -0 "$pid" 2>/dev/null; then
  alive=1
  kill -TERM "$pid" 2>/dev/null || true
fi
wait "$pid" 2>/dev/null
status=$?
echo "pid=$pid alive_after_${RUN_SECONDS}s=$alive exit_code=$status"
wc -l "$LOG_FILE"
exit 0
