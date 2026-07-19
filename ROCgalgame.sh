#!/bin/sh
set -eu

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
APP_DIR="$SELF_DIR/ROCgalgame"
if [ ! -d "$APP_DIR" ]; then
  APP_DIR="$SELF_DIR"
fi
BIN="$APP_DIR/rocgalgame_sdl"
LOG_FILE="${ROCGALGAME_LOG:-$APP_DIR/ROCgalgame.log}"

export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-alsa}"
export SDL_NOMOUSE="${SDL_NOMOUSE:-1}"
export ROCGALGAME_ROOT="$APP_DIR"
export ROCGALGAME_SCREEN_PROFILE="${ROCGALGAME_SCREEN_PROFILE:-1600x1440}"
export ROCGALGAME_DEVICE_MODEL="${ROCGALGAME_DEVICE_MODEL:-gkd350h-ultra}"
export ROCREADER_SYSTEM_VOLUME_LEVELS="${ROCREADER_SYSTEM_VOLUME_LEVELS:-20}"

if [ -z "${XDG_RUNTIME_DIR:-}" ]; then
  for sock in \
    /run/user/$(id -u 2>/dev/null || echo 0)/wayland-* \
    /run/$(id -u 2>/dev/null || echo 0)-runtime-dir/wayland-* \
    /run/*-runtime-dir/wayland-* \
    /tmp/*/wayland-*; do
    if [ -S "$sock" ]; then
      export XDG_RUNTIME_DIR="$(dirname "$sock")"
      export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-$(basename "$sock")}"
      break
    fi
  done
fi
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/rocgalgame-xdg}"
if [ -z "${WAYLAND_DISPLAY:-}" ]; then
  if [ -S "$XDG_RUNTIME_DIR/wayland-1" ]; then
    export WAYLAND_DISPLAY=wayland-1
  elif [ -S "$XDG_RUNTIME_DIR/wayland-0" ]; then
    export WAYLAND_DISPLAY=wayland-0
  fi
fi
mkdir -p "$XDG_RUNTIME_DIR" "$APP_DIR/cache" 2>/dev/null || true
chmod 700 "$XDG_RUNTIME_DIR" 2>/dev/null || true

LIB_FULL_DIR="$APP_DIR/lib"
LIB_SYSTEM_SDL_DIR="$APP_DIR/lib_system_sdl"
LD_LIBRARY_PATH_BASE="${LD_LIBRARY_PATH:-}"

set_runtime_libs() {
  lib_dir="$1"
  if [ -d "$lib_dir" ]; then
    export LD_LIBRARY_PATH="$lib_dir:$lib_dir/pulseaudio:/usr/lib32:/usr/lib:/lib:/mnt/vendor/lib:${LD_LIBRARY_PATH_BASE:-}"
  else
    export LD_LIBRARY_PATH="/usr/lib32:/usr/lib:/lib:/mnt/vendor/lib:${LD_LIBRARY_PATH_BASE:-}"
  fi
}

log_line() {
  printf '%s\n' "$1" >>"$LOG_FILE"
}

run_frontend_once() {
  set_runtime_libs "$LIB_SYSTEM_SDL_DIR"
  if [ -n "${SDL_VIDEODRIVER:-}" ]; then
    "$BIN" >>"$LOG_FILE" 2>&1
    return $?
  fi
  for drv in wayland KMSDRM kmsdrm x11; do
    log_line "[launcher] frontend driver=$drv"
    if SDL_VIDEODRIVER="$drv" "$BIN" >>"$LOG_FILE" 2>&1; then
      return 0
    fi
    rc=$?
    [ "$rc" -ge 128 ] 2>/dev/null && return "$rc"
  done
  set_runtime_libs "$LIB_FULL_DIR"
  "$BIN" >>"$LOG_FILE" 2>&1
}

if [ ! -x "$BIN" ]; then
  log_line "[launcher] missing frontend binary: $BIN"
  exit 4
fi

log_line "[launcher] xdg=${XDG_RUNTIME_DIR:-} wayland=${WAYLAND_DISPLAY:-} video=${SDL_VIDEODRIVER:-auto}"
if run_frontend_once; then
  exit 0
else
  exit $?
fi
