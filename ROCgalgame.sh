#!/bin/sh
set -eu

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
LAUNCHER_PATH="$SELF_DIR/$(basename -- "$0")"

# The ES ports entry and the IUX application share one runtime. Keep the full
# launcher in ports as a fallback for legacy installations without app/.
case "$SELF_DIR" in
  */ports)
    for iux_launcher in \
      /storage/games-external/app/ROCgalgame/launch.sh \
      "$SELF_DIR/../../app/ROCgalgame/launch.sh"; do
      if [ -x "$iux_launcher" ]; then
        exec "$iux_launcher" "$@"
      fi
    done
    ;;
esac

APP_DIR="$SELF_DIR/ROCgalgame"
if [ ! -d "$APP_DIR" ]; then
  APP_DIR="$SELF_DIR"
fi
BIN="$APP_DIR/rocgalgame_sdl"
LOG_FILE="${ROCGALGAME_LOG:-$APP_DIR/ROCgalgame.log}"
UPDATE_DOWNLOADS_DIR="$APP_DIR/Downloads"
UPDATE_MARKER="$UPDATE_DOWNLOADS_DIR/ROCgalgame_update_pending.txt"
UPDATE_STAGE_DIR="$APP_DIR/cache/update_stage"

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

marker_value() {
  key="$1"
  awk -F= -v wanted="$key" '$1 == wanted { print substr($0, index($0, "=") + 1); exit }' "$UPDATE_MARKER" |
    tr -d '\r'
}

extract_update_zip() {
  package="$1"
  rm -rf "$UPDATE_STAGE_DIR"
  mkdir -p "$UPDATE_STAGE_DIR"
  if command -v unzip >/dev/null 2>&1; then
    unzip -oq "$package" -d "$UPDATE_STAGE_DIR" >>"$LOG_FILE" 2>&1
    return $?
  fi
  if command -v busybox >/dev/null 2>&1; then
    busybox unzip -o "$package" -d "$UPDATE_STAGE_DIR" >>"$LOG_FILE" 2>&1
    return $?
  fi
  if command -v python3 >/dev/null 2>&1; then
    python3 -m zipfile -e "$package" "$UPDATE_STAGE_DIR" >>"$LOG_FILE" 2>&1
    return $?
  fi
  return 127
}

replace_runtime_entry() {
  name="$1"
  staged="$2/$name"
  [ -e "$staged" ] || return 0
  rm -rf "$APP_DIR/$name"
  cp -a "$staged" "$APP_DIR/"
}

find_staged_runtime_dir() {
  for candidate in \
    "$UPDATE_STAGE_DIR/app/ROCgalgame" \
    "$UPDATE_STAGE_DIR/roms/ports/ROCgalgame" \
    "$UPDATE_STAGE_DIR/Roms/ports/ROCgalgame"; do
    [ -d "$candidate" ] && { printf '%s' "$candidate"; return 0; }
  done
  return 1
}

find_staged_launcher_file() {
  staged_runtime="$1"
  if [ -n "$staged_runtime" ] && [ -f "$staged_runtime/launch.sh" ]; then
    printf '%s' "$staged_runtime/launch.sh"
    return 0
  fi
  for candidate in \
    "$UPDATE_STAGE_DIR/roms/ports/ROCgalgame.sh" \
    "$UPDATE_STAGE_DIR/Roms/ports/ROCgalgame.sh"; do
    [ -f "$candidate" ] && { printf '%s' "$candidate"; return 0; }
  done
  return 1
}

install_es_launcher_if_present() {
  staged_es_launcher="$UPDATE_STAGE_DIR/roms/ports/ROCgalgame.sh"
  [ -f "$staged_es_launcher" ] || return 0
  for ports_dir in /storage/roms/ports "$APP_DIR/../../roms/ports"; do
    [ -d "$ports_dir" ] || continue
    cp "$staged_es_launcher" "$ports_dir/ROCgalgame.sh.new"
    chmod +x "$ports_dir/ROCgalgame.sh.new" 2>/dev/null || true
    mv "$ports_dir/ROCgalgame.sh.new" "$ports_dir/ROCgalgame.sh"
    return 0
  done
}

migrate_legacy_path_list() {
  path_list="$1"
  [ -f "$path_list" ] || return 0
  normalized_root="$(printf '%s' "$APP_DIR" | tr '[:upper:]' '[:lower:]')"
  migrated="$path_list.migrate.$$"
  awk -v root="$normalized_root" '
    {
      legacy = "/storage/roms/ports/rocgalgame"
      if (index($0, legacy) == 1) $0 = root substr($0, length(legacy) + 1)
      legacy = "/storage/games-external/roms/ports/rocgalgame"
      if (index($0, legacy) == 1) $0 = root substr($0, length(legacy) + 1)
      print
    }
  ' "$path_list" >"$migrated"
  if cmp -s "$path_list" "$migrated"; then
    rm -f "$migrated"
  else
    mv "$migrated" "$path_list"
    log_line "[launcher] migrated persistent paths in $path_list"
  fi
}

repair_legacy_volume_mute_once() {
  marker="$APP_DIR/cache/application_volume_v1_migrated"
  [ -f "$marker" ] && return 0

  # Older builds wired the in-app key-sound slider to ALSA and used the
  # persistent Master mute switch at 0%. Repair that state once on upgrade.
  if grep -Eq '^[[:space:]]*system_volume_percent[[:space:]]*=[[:space:]]*0[[:space:]]*$' \
      "$APP_DIR/native_config.ini" 2>/dev/null; then
    if [ -x /usr/bin/amixer ] && \
        env -u LD_LIBRARY_PATH /usr/bin/amixer -q sset Master unmute >/dev/null 2>&1; then
      log_line "[launcher] repaired legacy Master mute state"
    else
      log_line "[launcher] failed to repair legacy Master mute state"
      return 0
    fi
  fi

  mkdir -p "$(dirname "$marker")" 2>/dev/null || true
  : >"$marker" 2>/dev/null || true
}

perform_pending_update_if_any() {
  [ -f "$UPDATE_MARKER" ] || return 0
  package_name="$(marker_value filename)"
  package_version="$(marker_value version)"
  package_path="$UPDATE_DOWNLOADS_DIR/$package_name"
  if [ -z "$package_name" ] || [ ! -f "$package_path" ]; then
    log_line "[update] pending package missing: $package_path"
    return 0
  fi
  if ! extract_update_zip "$package_path"; then
    log_line "[update] failed to extract: $package_path"
    rm -rf "$UPDATE_STAGE_DIR"
    return 0
  fi

  staged_runtime="$(find_staged_runtime_dir || true)"
  staged_launcher="$(find_staged_launcher_file "$staged_runtime" || true)"
  if [ ! -x "$staged_runtime/rocgalgame_sdl" ] || [ ! -f "$staged_runtime/ui.pack" ]; then
    log_line "[update] staged runtime validation failed"
    rm -rf "$UPDATE_STAGE_DIR"
    return 0
  fi

  # Only replace application-owned runtime files. User data, config, keymap,
  # game cores, games, covers, saves, and cache stay untouched.
  replace_runtime_entry "rocgalgame_sdl" "$staged_runtime"
  replace_runtime_entry "ui.pack" "$staged_runtime"
  replace_runtime_entry "fonts" "$staged_runtime"
  replace_runtime_entry "sounds" "$staged_runtime"
  replace_runtime_entry "lib" "$staged_runtime"
  replace_runtime_entry "lib_system_sdl" "$staged_runtime"
  replace_runtime_entry "version.txt" "$staged_runtime"
  replace_runtime_entry "config.json" "$staged_runtime"
  replace_runtime_entry "rocgalgame.png" "$staged_runtime"
  replace_runtime_entry "icon.png" "$staged_runtime"

  if [ -n "$staged_launcher" ] && [ -f "$staged_launcher" ]; then
    cp "$staged_launcher" "$LAUNCHER_PATH.new"
    mv "$LAUNCHER_PATH.new" "$LAUNCHER_PATH"
  fi
  install_es_launcher_if_present
  chmod +x "$APP_DIR/rocgalgame_sdl" "$LAUNCHER_PATH" 2>/dev/null || true
  rm -f "$UPDATE_MARKER" "$package_path"
  rm -rf "$UPDATE_STAGE_DIR"
  log_line "[update] installed version=${package_version:-unknown}"
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

if [ "${1:-}" = "--install-pending-update" ]; then
  perform_pending_update_if_any
  exit $?
fi

perform_pending_update_if_any
migrate_legacy_path_list "$APP_DIR/cache/favorites.txt"
migrate_legacy_path_list "$APP_DIR/cache/history.txt"
repair_legacy_volume_mute_once

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
