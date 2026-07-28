#!/bin/sh
set -u

APP_DIR="${APP_DIR:-/storage/roms/ports/ROCgalgame}"
CORE="$APP_DIR/cores/krkr/krkr2"
PROJECT="${PROJECT:-$APP_DIR/cache/krkr2-minimal-test}"
if [ -f "$PROJECT" ]; then
  PROJECT_WORKDIR="${PROJECT_WORKDIR:-$(dirname "$PROJECT")}"
else
  PROJECT_WORKDIR="${PROJECT_WORKDIR:-$PROJECT}"
fi
TEST_NAME="${TEST_NAME:-krkr2-minimal}"
LOG_DIR="${LOG_DIR:-$APP_DIR/logs/$TEST_NAME}"
RUN_SECONDS="${RUN_SECONDS:-8}"
EXPECT_TJS_MARKER="${EXPECT_TJS_MARKER-KRKR2_MINIMAL_TJS_OK}"
EXPECT_INPUT_MARKER="${EXPECT_INPUT_MARKER-}"
INJECT_POINTER="${INJECT_POINTER:-0}"
DISPLAY_BACKEND="${ROCGALGAME_KRKR_DISPLAY_BACKEND:-wayland}"
REQUIRE_HARDWARE="${REQUIRE_HARDWARE:-0}"
UINPUT_CLICKER="${UINPUT_CLICKER:-$APP_DIR/inject_uinput_click.py}"
UINPUT_BUTTON="${UINPUT_BUTTON:-left}"
XWAYLAND_WIDTH="${XWAYLAND_WIDTH:-1600}"
XWAYLAND_HEIGHT="${XWAYLAND_HEIGHT:-1440}"
STAMP="$(date +%Y%m%d-%H%M%S)"
LOG_FILE="$LOG_DIR/$TEST_NAME-$STAMP.log"
TREE_FILE="$LOG_DIR/$TEST_NAME-$STAMP.sway-tree.json"

mkdir -p "$LOG_DIR"
test -x "$CORE" || { echo "[krkr2_test] missing core: $CORE"; exit 2; }
if [ -f "$PROJECT" ]; then
  case "$PROJECT" in
    *.xp3|*.XP3) ;;
    *) echo "[krkr2_test] project file is not an XP3 archive: $PROJECT"; exit 3 ;;
  esac
elif [ ! -f "$PROJECT/startup.tjs" ] && ! ls "$PROJECT"/*.xp3 >/dev/null 2>&1; then
  echo "[krkr2_test] project has neither startup.tjs nor an XP3 archive: $PROJECT"
  exit 3
fi

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/0-runtime-dir}"
export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-1}"
export SWAYSOCK="${SWAYSOCK:-/run/0-runtime-dir/sway-ipc.0.sock}"
export ROCGALGAME_KRKR_RUNTIME="${ROCGALGAME_KRKR_RUNTIME:-krkr2}"
export ROCGALGAME_KRKR_DISPLAY_BACKEND="$DISPLAY_BACKEND"
export ROCGALGAME_KRKR_XWAYLAND_WIDTH="$XWAYLAND_WIDTH"
export ROCGALGAME_KRKR_XWAYLAND_HEIGHT="$XWAYLAND_HEIGHT"
export ROCGALGAME_INPUT_PROFILE="${ROCGALGAME_INPUT_PROFILE:-gkd350h-ultra}"
export ROCGALGAME_KRKR_VIRTUAL_MOUSE="${ROCGALGAME_KRKR_VIRTUAL_MOUSE:-1}"
export ROCGALGAME_KRKR_SWAP_AB="${ROCGALGAME_KRKR_SWAP_AB:-1}"
export ROCGALGAME_MOUSE_SPEED="${ROCGALGAME_MOUSE_SPEED:-720}"
export ROCGALGAME_MOUSE_ACCEL="${ROCGALGAME_MOUSE_ACCEL:-1.6}"
export LD_LIBRARY_PATH="/usr/lib/mali:$APP_DIR/cores/krkr/lib_krkr2:$APP_DIR/lib_system_sdl:$APP_DIR/lib:/usr/lib:/lib:/mnt/vendor/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

xwayland_log="$LOG_DIR/xwayland-$STAMP.log"
case "$DISPLAY_BACKEND" in
  wayland)
    unset DISPLAY
    export GDK_BACKEND=wayland
    export SDL_VIDEODRIVER=wayland
    wayland_socket="$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY"
    test -S "$wayland_socket" || {
      echo "[krkr2_test] Wayland socket is unavailable: $wayland_socket"
      exit 5
    }
    window_selector='[title="krkr2"]'
    ;;
  xwayland)
    export DISPLAY="${DISPLAY:-:2}"
    export GDK_BACKEND=x11
    export SDL_VIDEODRIVER=x11
    display_number="${DISPLAY#:}"
    x_socket="/tmp/.X11-unix/X$display_number"
    window_selector="[title=\"Xwayland on $DISPLAY\"]"
    if [ ! -S "$x_socket" ]; then
      private_gl="$APP_DIR/cores/krkr/lib_krkr2/libGL.so.1"
      test -f "$private_gl" || {
        echo "[krkr2_test] missing private GLVND library: $private_gl"
        exit 6
      }
      command -v Xwayland >/dev/null 2>&1 || {
        echo "[krkr2_test] Xwayland is unavailable"
        exit 7
      }
      swaymsg -s "$SWAYSOCK" exec \
        "env LD_LIBRARY_PATH=$APP_DIR/cores/krkr/lib_krkr2:/usr/lib:/lib LIBGL_ALWAYS_SOFTWARE=1 Xwayland $DISPLAY -ac -terminate -geometry ${XWAYLAND_WIDTH}x${XWAYLAND_HEIGHT} -glamor off -shm >$xwayland_log 2>&1" \
        >/dev/null
      wait_count=0
      while [ ! -S "$x_socket" ] && [ "$wait_count" -lt 10 ]; do
        sleep 1
        wait_count=$((wait_count + 1))
      done
      test -S "$x_socket" || {
        echo "[krkr2_test] private Xwayland failed to create $x_socket"
        cat "$xwayland_log" 2>/dev/null || true
        exit 8
      }
      echo "[krkr2_test] private_xwayland=$DISPLAY"
    fi
    ;;
  *)
    echo "[krkr2_test] unsupported display backend: $DISPLAY_BACKEND"
    exit 9
    ;;
esac

echo "[krkr2_test] core=$CORE"
echo "[krkr2_test] project=$PROJECT"
echo "[krkr2_test] workdir=$PROJECT_WORKDIR"
echo "[krkr2_test] backend=$DISPLAY_BACKEND wayland=$WAYLAND_DISPLAY xdg=$XDG_RUNTIME_DIR sway=$SWAYSOCK"

if ldd "$CORE" 2>&1 | tee "$LOG_FILE.ldd" | grep -q 'not found'; then
  echo "[krkr2_test] unresolved runtime dependency"
  cat "$LOG_FILE.ldd"
  exit 4
fi

cd "$PROJECT_WORKDIR"
"$CORE" "$PROJECT" >"$LOG_FILE" 2>&1 &
pid=$!
echo "[krkr2_test] pid=$pid"
if [ "$INJECT_POINTER" = "1" ] && [ "$RUN_SECONDS" -ge 7 ]; then
  sleep 4
  swaymsg -s "$SWAYSOCK" "$window_selector" focus >/dev/null 2>&1 || true
  sleep 1
  swaymsg -s "$SWAYSOCK" seat seat0 cursor set 80 80 >/dev/null 2>&1 || true
  swaymsg -s "$SWAYSOCK" seat seat0 cursor press button1 >/dev/null 2>&1 || true
  swaymsg -s "$SWAYSOCK" seat seat0 cursor release button1 >/dev/null 2>&1 || true
  swaymsg -s "$SWAYSOCK" seat seat0 cursor set 480 320 >/dev/null 2>&1 || true
  if command -v python3 >/dev/null 2>&1 && [ -f "$UINPUT_CLICKER" ]; then
    python3 "$UINPUT_CLICKER" "$UINPUT_BUTTON" >/dev/null 2>&1 || true
  else
    swaymsg -s "$SWAYSOCK" seat seat0 cursor press button1 >/dev/null 2>&1 || true
    sleep 1
    swaymsg -s "$SWAYSOCK" seat seat0 cursor release button1 >/dev/null 2>&1 || true
  fi
  sleep $((RUN_SECONDS - 6))
else
  sleep "$RUN_SECONDS"
fi

alive=0
if kill -0 "$pid" 2>/dev/null; then
  alive=1
fi

window_found=0
if command -v swaymsg >/dev/null 2>&1 && [ -S "$SWAYSOCK" ]; then
  swaymsg -s "$SWAYSOCK" -t get_tree >"$TREE_FILE" 2>&1 || true
  if grep -qi 'krkr2' "$TREE_FILE" ||
     { [ "$DISPLAY_BACKEND" = "xwayland" ] &&
       grep -qi "Xwayland on ${DISPLAY:-}" "$TREE_FILE"; }; then
    window_found=1
  fi
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
tjs_found=1
if [ -n "$EXPECT_TJS_MARKER" ] && ! grep -q "$EXPECT_TJS_MARKER" "$LOG_FILE"; then
  tjs_found=0
elif [ -n "$EXPECT_TJS_MARKER" ]; then
  tjs_found=1
fi
input_found=1
if [ -n "$EXPECT_INPUT_MARKER" ] && ! grep -q "$EXPECT_INPUT_MARKER" "$LOG_FILE"; then
  input_found=0
fi
renderer="$(sed -n 's/^.*OpenGL ES renderer:[[:space:]]*//p' "$LOG_FILE" | tail -n 1)"
hardware_renderer=1
if [ "$REQUIRE_HARDWARE" = "1" ]; then
  if [ -z "$renderer" ] || printf '%s\n' "$renderer" | grep -Eqi \
       'softpipe|llvmpipe|software rasterizer'; then
    hardware_renderer=0
  fi
fi

echo "[krkr2_test] alive_after_${RUN_SECONDS}s=$alive"
echo "[krkr2_test] window_found=$window_found"
echo "[krkr2_test] tjs_marker_found=$tjs_found"
echo "[krkr2_test] expected_tjs_marker=${EXPECT_TJS_MARKER:-none}"
echo "[krkr2_test] input_marker_found=$input_found"
echo "[krkr2_test] expected_input_marker=${EXPECT_INPUT_MARKER:-none}"
echo "[krkr2_test] exit_code=$exit_code"
echo "[krkr2_test] renderer=${renderer:-missing}"
echo "[krkr2_test] hardware_renderer=$hardware_renderer"
echo "[krkr2_test] log=$LOG_FILE"
echo "[krkr2_test] tree=$TREE_FILE"
echo "[krkr2_test] log_tail_begin"
tail -n 120 "$LOG_FILE" 2>/dev/null || true
echo "[krkr2_test] log_tail_end"

if [ "$alive" -ne 1 ] || [ "$window_found" -ne 1 ] ||
   [ "$tjs_found" -ne 1 ] || [ "$input_found" -ne 1 ] ||
   [ "$hardware_renderer" -ne 1 ]; then
  exit 10
fi
