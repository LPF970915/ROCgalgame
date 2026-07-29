#!/bin/sh
set -u

APP_DIR="${APP_DIR:-/storage/roms/ports/ROCgalgame}"
CORE="${CORE:-$APP_DIR/cores/krkr/krkr2.candidate-packinone}"
CASE_SOURCE="${CASE_SOURCE:?set CASE_SOURCE to one real game directory}"
CASE_ID="${CASE_ID:-candidate}"
RUN_SECONDS="${RUN_SECONDS:-20}"
TEST_ROOT="${TEST_ROOT:-/tmp/rocgalgame-krkr2-frontend-$CASE_ID}"
LOG="${LOG:-$APP_DIR/logs/krkr2-frontend-$CASE_ID.log}"
TREE="${TREE:-$APP_DIR/logs/krkr2-frontend-$CASE_ID-tree.json}"

test -x "$APP_DIR/rocgalgame_sdl"
test -x "$CORE"
test -d "$CASE_SOURCE"

if pgrep -x rocgalgame_sdl >/dev/null 2>&1 || pgrep -x krkr2 >/dev/null 2>&1; then
  echo "[candidate-frontend] refusing to interrupt an active frontend/core" >&2
  exit 2
fi

mkdir -p "$TEST_ROOT/cores/krkr" "$TEST_ROOT/games" "$TEST_ROOT/saves" \
  "$TEST_ROOT/cache" "$TEST_ROOT/logs" "$(dirname "$LOG")"
cp -p "$APP_DIR/native_config.ini" "$TEST_ROOT/native_config.ini"

link_item() {
  source_path="$1"
  target_path="$2"
  if [ ! -e "$target_path" ] && [ ! -L "$target_path" ]; then
    ln -s "$source_path" "$target_path"
  fi
}

for name in ui.pack fonts sounds native_keymap.ini pwr_new.sh lib lib_system_sdl; do
  if [ -e "$APP_DIR/$name" ]; then
    link_item "$APP_DIR/$name" "$TEST_ROOT/$name"
  fi
done
for name in Resources lib_krkr2 plugin; do
  if [ -e "$APP_DIR/cores/krkr/$name" ]; then
    link_item "$APP_DIR/cores/krkr/$name" "$TEST_ROOT/cores/krkr/$name"
  fi
done
link_item "$CORE" "$TEST_ROOT/cores/krkr/krkr2"
link_item "$CASE_SOURCE" "$TEST_ROOT/games/$(basename "$CASE_SOURCE")"

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/0-runtime-dir}"
export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-1}"
export SWAYSOCK="${SWAYSOCK:-/run/0-runtime-dir/sway-ipc.0.sock}"
export GDK_BACKEND=wayland
export SDL_VIDEODRIVER=wayland
export ROCGALGAME_ROOT="$TEST_ROOT"
export ROCGALGAME_NAV_INDEX=1
export ROCGALGAME_AUTOLAUNCH_FIRST=1
export ROCGALGAME_KRKR_DISPLAY_BACKEND=wayland
export LD_LIBRARY_PATH="$APP_DIR/lib_system_sdl:$APP_DIR/lib:/usr/lib/mali:/usr/lib:/lib:/mnt/vendor/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
unset DISPLAY

cd "$TEST_ROOT"
"$APP_DIR/rocgalgame_sdl" >"$LOG" 2>&1 &
frontend_pid=$!
sleep "$RUN_SECONDS"

frontend_alive=0
core_alive=0
if kill -0 "$frontend_pid" 2>/dev/null; then frontend_alive=1; fi
core_pid="$(pgrep -f "^$TEST_ROOT/cores/krkr/krkr2" | head -n 1 || true)"
if [ -n "$core_pid" ] && kill -0 "$core_pid" 2>/dev/null; then core_alive=1; fi

window=0
if command -v swaymsg >/dev/null 2>&1 && [ -S "$SWAYSOCK" ]; then
  swaymsg -s "$SWAYSOCK" -t get_tree >"$TREE" 2>&1 || true
  if grep -qi 'krkr2' "$TREE"; then window=1; fi
fi

if [ "$core_alive" -eq 1 ]; then kill -TERM "$core_pid" 2>/dev/null || true; fi
sleep 2
if [ -n "$core_pid" ] && kill -0 "$core_pid" 2>/dev/null; then
  kill -KILL "$core_pid" 2>/dev/null || true
fi
if kill -0 "$frontend_pid" 2>/dev/null; then
  kill -TERM "$frontend_pid" 2>/dev/null || true
  sleep 1
fi
if kill -0 "$frontend_pid" 2>/dev/null; then
  kill -KILL "$frontend_pid" 2>/dev/null || true
fi
wait "$frontend_pid" 2>/dev/null
frontend_exit=$?

core_log="$(find "$TEST_ROOT/logs" -type f -name '*.log' -print 2>/dev/null | sort | tail -n 1)"
startup=0
exceptions=0
if [ -n "$core_log" ]; then
  grep -q 'Startup script ended' "$core_log" && startup=1
  exceptions="$(grep -Eic 'An exception occurred|Unhandled exception|Segmentation fault|SIGSEGV|std::terminate' "$core_log" 2>/dev/null || true)"
fi
if [ "$startup" -eq 0 ] && grep -q 'Startup script ended' "$LOG" 2>/dev/null; then
  startup=1
fi
frontend_exceptions="$(grep -Eic 'An exception occurred|Unhandled exception|Segmentation fault|SIGSEGV|std::terminate' "$LOG" 2>/dev/null || true)"
exceptions=$((exceptions + frontend_exceptions))

echo "[candidate-frontend] frontend_alive=$frontend_alive core_alive=$core_alive window=$window startup=$startup exceptions=$exceptions frontend_exit=$frontend_exit"
echo "[candidate-frontend] root=$TEST_ROOT"
echo "[candidate-frontend] frontend_log=$LOG"
echo "[candidate-frontend] core_log=$core_log"

if [ "$core_alive" -eq 1 ] && [ "$window" -eq 1 ] && \
   [ "$startup" -eq 1 ] && [ "$exceptions" -eq 0 ]; then
  exit 0
fi
exit 1
