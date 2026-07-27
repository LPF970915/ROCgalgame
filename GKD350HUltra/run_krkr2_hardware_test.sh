#!/bin/sh
set -eu

APP_DIR="${APP_DIR:-/storage/roms/ports/ROCgalgame}"
XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/0-runtime-dir}"
WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-1}"
SWAYSOCK="${SWAYSOCK:-/run/0-runtime-dir/sway-ipc.0.sock}"
RUN_SECONDS="${RUN_SECONDS:-12}"

export APP_DIR XDG_RUNTIME_DIR WAYLAND_DISPLAY SWAYSOCK RUN_SECONDS
export ROCGALGAME_KRKR_DISPLAY_BACKEND=wayland
export GDK_BACKEND=wayland
export SDL_VIDEODRIVER=wayland
export REQUIRE_HARDWARE=1
export LD_LIBRARY_PATH="/usr/lib/mali:$APP_DIR/cores/krkr/lib_krkr2:$APP_DIR/lib_system_sdl:$APP_DIR/lib:/usr/lib:/lib:/mnt/vendor/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
unset DISPLAY

test -S "$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY" || {
  echo "[krkr2_hw] missing Wayland socket: $XDG_RUNTIME_DIR/$WAYLAND_DISPLAY"
  exit 2
}
test -x "$APP_DIR/cores/krkr/krkr2" || {
  echo "[krkr2_hw] missing KRKR2 core"
  exit 3
}

echo "[krkr2_hw] backend=wayland egl=1 gles=1"
/bin/sh "$APP_DIR/run_krkr2_minimal_test.sh"
echo "[krkr2_hw] passed native Wayland/EGL/GLES hardware validation"
