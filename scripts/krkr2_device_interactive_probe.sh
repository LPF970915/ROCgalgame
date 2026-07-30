#!/bin/sh
set -eu

APP_DIR="${APP_DIR:-/storage/games-external/app/ROCgalgame}"
CORE="${CORE:-$APP_DIR/cores/krkr/krkr2}"
SOURCE_DIR="${1:?usage: krkr2_device_interactive_probe.sh SOURCE_DIR ENTRY [CASE_ID]}"
ENTRY="${2:?usage: krkr2_device_interactive_probe.sh SOURCE_DIR ENTRY [CASE_ID]}"
CASE_ID="${3:-interactive}"
STAMP="$(date +%Y%m%d-%H%M%S)"
ROOT="/tmp/rocgalgame-interactive-$CASE_ID-$STAMP"
CASE_DIR="$ROOT/game"
SAVE_DIR="$ROOT/save-root"
CAPTURE_DIR="$ROOT/captures"
INPUT_FIFO="$ROOT/input.fifo"
INPUT_COMMANDS="$ROOT/input.commands"
CAPTURE_REQUEST="$ROOT/capture.request"
POINTER_REQUEST="$ROOT/pointer.request"
LOG_FILE="$ROOT/krkr2.log"
STATE_FILE="/tmp/rocgalgame-interactive-state"
CORE_PID=""
WRITER_PID=""

cleanup() {
  if [ -n "$CORE_PID" ] && kill -0 "$CORE_PID" 2>/dev/null; then
    kill -TERM "$CORE_PID" 2>/dev/null || true
    sleep 1
    kill -KILL "$CORE_PID" 2>/dev/null || true
  fi
  if [ -n "$WRITER_PID" ] && kill -0 "$WRITER_PID" 2>/dev/null; then
    kill -TERM "$WRITER_PID" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

copy_or_link_item() {
  source_item="$1"
  destination="$2"
  name="$(basename "$source_item")"

  if [ -d "$source_item" ]; then
    case "$name" in
      savedata|save|saves|strsave)
        cp -a "$source_item" "$destination/"
        return
        ;;
    esac
    size_kb="$(du -sk "$source_item" 2>/dev/null | awk '{print $1}')"
    case "$size_kb" in ''|*[!0-9]*) size_kb=999999;; esac
    if [ "$size_kb" -le 8192 ]; then
      cp -a "$source_item" "$destination/"
    else
      ln -s "$source_item" "$destination/$name"
    fi
    return
  fi

  size_bytes="$(stat -c %s "$source_item" 2>/dev/null || echo 999999999)"
  case "$size_bytes" in ''|*[!0-9]*) size_bytes=999999999;; esac
  if [ "$size_bytes" -le 2097152 ]; then
    cp -p "$source_item" "$destination/$name"
  else
    ln -s "$source_item" "$destination/$name"
  fi
}

test -x "$CORE"
test -d "$SOURCE_DIR"
if [ "$ENTRY" != "." ]; then
  test -e "$SOURCE_DIR/$ENTRY"
fi

mkdir -p "$CASE_DIR" "$SAVE_DIR" "$CAPTURE_DIR"
if [ -n "${SAVE_SEED_DIR:-}" ] && [ -d "$SAVE_SEED_DIR" ]; then
  cp -a "$SAVE_SEED_DIR"/. "$SAVE_DIR"/
fi
for source_item in "$SOURCE_DIR"/*; do
  [ -e "$source_item" ] || continue
  copy_or_link_item "$source_item" "$CASE_DIR"
done
for save_name in savedata save saves strsave; do
  [ -e "$CASE_DIR/$save_name" ] || mkdir -p "$CASE_DIR/$save_name"
done

mkfifo "$INPUT_FIFO"
: >"$INPUT_COMMANDS"
: >"$CAPTURE_REQUEST"
: >"$POINTER_REQUEST"
tail -n 0 -f "$INPUT_COMMANDS" >"$INPUT_FIFO" &
WRITER_PID=$!

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
export ROCGALGAME_KRKR_INPUT_FIFO="$INPUT_FIFO"
export ROCGALGAME_KRKR_PRESENTATION_CAPTURE_REQUEST="$CAPTURE_REQUEST"
export ROCGALGAME_KRKR_POINTER_REQUEST="$POINTER_REQUEST"
export KRKRSDL2_PATH="$SOURCE_DIR/plugin:$APP_DIR/cores/krkr/plugin:$APP_DIR/plugin"
export LD_LIBRARY_PATH="/usr/lib/mali:$APP_DIR/cores/krkr/lib_krkr2:$APP_DIR/lib_system_sdl:$APP_DIR/lib:/usr/lib:/lib:/mnt/vendor/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
unset DISPLAY

cd "$CASE_DIR"
if [ "$ENTRY" = "." ]; then
  "$CORE" "$CASE_DIR" >"$LOG_FILE" 2>&1 &
else
  "$CORE" "$CASE_DIR/$ENTRY" >"$LOG_FILE" 2>&1 &
fi
CORE_PID=$!

{
  printf 'root=%s\n' "$ROOT"
  printf 'pid=%s\n' "$CORE_PID"
  printf 'commands=%s\n' "$INPUT_COMMANDS"
  printf 'fifo=%s\n' "$INPUT_FIFO"
  printf 'request=%s\n' "$CAPTURE_REQUEST"
  printf 'pointer=%s\n' "$POINTER_REQUEST"
  printf 'captures=%s\n' "$CAPTURE_DIR"
  printf 'log=%s\n' "$LOG_FILE"
  printf 'source=%s\n' "$SOURCE_DIR"
  printf 'entry=%s\n' "$ENTRY"
} >"$STATE_FILE.tmp"
mv "$STATE_FILE.tmp" "$STATE_FILE"

set +e
wait "$CORE_PID"
EXIT_CODE=$?
set -e
CORE_PID=""
printf 'exit_code=%s\n' "$EXIT_CODE" >>"$STATE_FILE"
exit "$EXIT_CODE"
