#!/bin/sh
set -eu

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
APP_DIR="$SELF_DIR/ROCgalgame"
if [ ! -d "$APP_DIR" ]; then
  APP_DIR="$SELF_DIR"
fi
BIN="$APP_DIR/rocgalgame_sdl"
LOG_FILE="${ROCGALGAME_LOG:-$APP_DIR/ROCgalgame.log}"
REQUEST_FILE="$APP_DIR/cache/launch_request.ini"

export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-alsa}"
export SDL_NOMOUSE="${SDL_NOMOUSE:-1}"
export ROCGALGAME_ROOT="$APP_DIR"
export ROCGALGAME_SCREEN_PROFILE="${ROCGALGAME_SCREEN_PROFILE:-1600x1440}"
export ROCGALGAME_DEVICE_MODEL="${ROCGALGAME_DEVICE_MODEL:-gkd350h-ultra}"
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

log_env() {
  log_line "[launcher] xdg=${XDG_RUNTIME_DIR:-} wayland=${WAYLAND_DISPLAY:-} video=${SDL_VIDEODRIVER:-auto}"
}

ini_get() {
  key="$1"
  file="$2"
  sed -n "s/^$key=//p" "$file" 2>/dev/null | sed -n '1p'
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
    [ "$rc" -eq 42 ] && return 42
    [ "$rc" -ge 128 ] 2>/dev/null && return "$rc"
  done
  set_runtime_libs "$LIB_FULL_DIR"
  "$BIN" >>"$LOG_FILE" 2>&1
}

run_core_request() {
  [ -f "$REQUEST_FILE" ] || return 0
  core="$(ini_get core "$REQUEST_FILE")"
  game_path="$(ini_get path "$REQUEST_FILE")"
  save_path="$(ini_get save "$REQUEST_FILE")"
  encoding="$(ini_get encoding "$REQUEST_FILE")"
  aspect="$(ini_get aspect "$REQUEST_FILE")"
  filter="$(ini_get filter "$REQUEST_FILE")"
  virtual_mouse="$(ini_get virtual_mouse "$REQUEST_FILE")"
  mouse_speed="$(ini_get mouse_speed "$REQUEST_FILE")"
  mouse_accel="$(ini_get mouse_accel "$REQUEST_FILE")"
  rm -f "$REQUEST_FILE"

  [ -n "$core" ] || return 0
  [ -n "$game_path" ] || return 0
  mkdir -p "$save_path" 2>/dev/null || true

  case "$core" in
    ons)
      exe="$APP_DIR/cores/ons/onsyuri"
      if [ ! -x "$exe" ]; then
        log_line "[launcher] missing ONS core: $exe"
        return 0
      fi
      set_runtime_libs "$LIB_FULL_DIR"
      export ROCGALGAME_VIRTUAL_MOUSE="${virtual_mouse:-1}"
      export ROCGALGAME_MOUSE_SPEED="${mouse_speed:-720}"
      export ROCGALGAME_MOUSE_ACCEL="${mouse_accel:-1.6}"
      export ROCGALGAME_ASPECT="${aspect:-fit-width}"
      export ROCGALGAME_FILTER="${filter:-clean}"
      case "${encoding:-gbk}" in
        sjis|shift-jis|shift_jis) enc_arg="--enc:sjis" ;;
        utf8|utf-8) enc_arg="--enc:utf8" ;;
        gbk|gb2312|cp936|*) enc_arg="--enc:gbk" ;;
      esac
      font_path="$APP_DIR/fonts/ui_font.ttf"
      [ -f "$game_path/default.ttf" ] && font_path="$game_path/default.ttf"
      set -- --root "$game_path" --save-dir "$save_path" --font "$font_path" "$enc_arg" --force-button-shortcut
      if [ "${aspect:-fit-width}" = "stretch" ]; then
        set -- "$@" --fullscreen2
      else
        set -- "$@" --fullscreen
      fi
      log_line "[launcher] start ONS path=$game_path save=$save_path aspect=${aspect:-fit-width} enc=${encoding:-gbk} font=$font_path"
      if [ -d "$game_path" ]; then
        set +e
        (cd "$game_path" && SDL_NOMOUSE=0 SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-wayland}" "$exe" "$@") >>"$LOG_FILE" 2>&1
        core_rc=$?
        set -e
      else
        log_line "[launcher] missing game path: $game_path"
        core_rc=2
      fi
      log_line "[launcher] ONS exited rc=$core_rc"
      ;;
    krkr)
      exe="$APP_DIR/cores/krkr/krkrsdl2"
      if [ ! -x "$exe" ]; then
        log_line "[launcher] missing KRKR core: $exe"
        return 0
      fi
      set_runtime_libs "$LIB_FULL_DIR"
      export KRKRSDL2_PATH="$game_path/plugin:$APP_DIR/cores/krkr/plugin:$APP_DIR/plugin"
      SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-wayland}" "$exe" "$game_path/data.xp3" -datapath="$save_path" -deffont="Noto Sans CJK JP" >>"$LOG_FILE" 2>&1 || true
      ;;
    *)
      log_line "[launcher] unknown core: $core aspect=$aspect filter=$filter virtual_mouse=$virtual_mouse"
      ;;
  esac
}

if [ ! -x "$BIN" ]; then
  log_line "[launcher] missing frontend binary: $BIN"
  exit 4
fi

log_env
if [ -f "$REQUEST_FILE" ]; then
  log_line "[launcher] discarded stale launch request at startup"
  rm -f "$REQUEST_FILE"
fi

while true; do
  if run_frontend_once; then
    rc=0
  else
    rc=$?
  fi
  if [ "$rc" -eq 42 ]; then
    run_core_request
    continue
  fi
  exit "$rc"
done
