#!/bin/sh
set -u

APP_DIR="${APP_DIR:-/storage/games-external/app/ROCgalgame}"
GAMES_DIR="${GAMES_DIR:-$APP_DIR/games}"
HELPER="${HELPER:-/tmp/gkd_uinput_sequence.py}"
POINTER_HELPER="${POINTER_HELPER:-/tmp/gkd_uinput_click.py}"
RUN_SECONDS="${RUN_SECONDS:-70}"
RSS_LIMIT_KB="${RSS_LIMIT_KB:-950000}"
CAPTURE_SECONDS="${CAPTURE_SECONDS:-3 12 22 36 55}"
CORE_FILTER="${CORE_FILTER:-krkr}"
FORCE_KRKR_RUNTIME="${FORCE_KRKR_RUNTIME:-}"
MIN_NONBLACK_RATIO="${MIN_NONBLACK_RATIO:-0.01}"
REQUIRE_FRAME_DIFF="${REQUIRE_FRAME_DIFF:-1}"
REQUIRE_SWAP_FRAME="${REQUIRE_SWAP_FRAME:-1}"
POINTER_CLICK_SECONDS="${POINTER_CLICK_SECONDS:-}"
INPUT_SEQUENCE="${INPUT_SEQUENCE:-sleep:8,tap:a:0.2,sleep:6,tap:a:0.2,sleep:8,tap:a:0.2,sleep:8,tap:a:0.2}"
EXTRA_KILL_NAMES="${EXTRA_KILL_NAMES:-}"
TITLE_REGEX="${TITLE_REGEX:-}"
TITLE_EXCLUDE_REGEX="${TITLE_EXCLUDE_REGEX:-}"
CASE_FILTER="${CASE_FILTER:-}"
MAX_CASES="${MAX_CASES:-0}"
DISCOVER_ONLY="${DISCOVER_ONLY:-0}"
UNLOCK_BEFORE_CASE="${UNLOCK_BEFORE_CASE:-1}"
HIDE_IUX_BEFORE_CASE="${HIDE_IUX_BEFORE_CASE:-0}"
STAMP="$(date +%Y%m%d-%H%M%S)"
LOG_DIR="${LOG_DIR:-$APP_DIR/logs/krkr2-frontend-sweep-$STAMP}"
TEST_ROOT="${TEST_ROOT:-/tmp/rocgalgame-krkr2-frontend-sweep-$STAMP}"
SUMMARY="$LOG_DIR/summary.tsv"
ALLOW_GAME_COPY="${ALLOW_GAME_COPY:-0}"
DISCOVERED=0
RUN_COUNT=0
FAILED_COUNT=0
CURRENT_FRONTEND_PID=""
CURRENT_UINPUT_PID=""
CURRENT_GAME_MOUNT=""
CURRENT_GAME_MOUNT_LIST=""

kill_pid_list() {
  signal="$1"
  shift
  for pid in "$@"; do
    case "$pid" in ''|*[!0-9]*) continue ;; esac
    [ "$pid" = "$$" ] && continue
    kill "-$signal" "$pid" 2>/dev/null || true
  done
}

kill_runtime_processes() {
  pids="$(pidof rocgalgame_sdl onsyuri krkr2 krkrsdl2 2>/dev/null || true)"
  if [ -n "$pids" ]; then
    kill_pid_list TERM $pids
    sleep 1
    kill_pid_list KILL $pids
  fi
  if [ -n "$EXTRA_KILL_NAMES" ]; then
    pids="$(pidof $EXTRA_KILL_NAMES 2>/dev/null || true)"
    if [ -n "$pids" ]; then
      kill_pid_list TERM $pids
      sleep 1
      kill_pid_list KILL $pids
    fi
  fi
}

current_core_pid() {
  runtime="${1:-${expected_runtime:-}}"
  case "$runtime" in
    onsyuri|krkr2|krkrsdl2) pidof "$runtime" 2>/dev/null | awk '{print $1}' ;;
    *) return 1 ;;
  esac
}

kill_stale_uinput_helpers() {
  helper_pids=""
  for pid in $(pidof python3 2>/dev/null || true); do
    cmdline="$(tr '\0' ' ' <"/proc/$pid/cmdline" 2>/dev/null || true)"
    case "$cmdline" in
      *"$HELPER"*) helper_pids="$helper_pids $pid" ;;
    esac
  done
  if [ -n "$helper_pids" ]; then
    kill_pid_list TERM $helper_pids
    sleep 1
    kill_pid_list KILL $helper_pids
  fi
}

unmount_current_game_view() {
  if [ -n "$CURRENT_GAME_MOUNT_LIST" ] && [ -f "$CURRENT_GAME_MOUNT_LIST" ]; then
    while IFS= read -r mounted_item; do
      [ -n "$mounted_item" ] && umount "$mounted_item" 2>/dev/null || true
    done <"$CURRENT_GAME_MOUNT_LIST"
    rm -f "$CURRENT_GAME_MOUNT_LIST"
    CURRENT_GAME_MOUNT_LIST=""
  fi
  if [ -n "$CURRENT_GAME_MOUNT" ]; then
    umount "$CURRENT_GAME_MOUNT" 2>/dev/null || true
    CURRENT_GAME_MOUNT=""
  fi
}

cleanup() {
  if [ -n "$CURRENT_FRONTEND_PID" ] && kill -0 "$CURRENT_FRONTEND_PID" 2>/dev/null; then
    kill -TERM "$CURRENT_FRONTEND_PID" 2>/dev/null || true
    sleep 1
    kill -KILL "$CURRENT_FRONTEND_PID" 2>/dev/null || true
  fi
  if [ -n "$CURRENT_UINPUT_PID" ] && kill -0 "$CURRENT_UINPUT_PID" 2>/dev/null; then
    kill -TERM "$CURRENT_UINPUT_PID" 2>/dev/null || true
  fi
  kill_runtime_processes
  unmount_current_game_view
}
trap cleanup EXIT INT TERM

test -x "$APP_DIR/rocgalgame_sdl" || { echo "[frontend_sweep] missing frontend"; exit 2; }
test -x "$APP_DIR/cores/krkr/krkr2" || { echo "[frontend_sweep] missing krkr2"; exit 2; }
test -x "$APP_DIR/cores/krkr/krkrsdl2" || { echo "[frontend_sweep] missing krkrsdl2"; exit 2; }
test -x "$APP_DIR/cores/ons/onsyuri" || { echo "[frontend_sweep] missing onsyuri"; exit 2; }
test -f "$HELPER" || { echo "[frontend_sweep] missing helper: $HELPER"; exit 2; }
case "$CORE_FILTER" in
  all|ons|krkr) ;;
  *) echo "[frontend_sweep] CORE_FILTER must be all, ons, or krkr"; exit 2 ;;
esac
case "$FORCE_KRKR_RUNTIME" in
  ""|krkr2|krkrsdl2) ;;
  *) echo "[frontend_sweep] FORCE_KRKR_RUNTIME must be krkr2 or krkrsdl2"; exit 2 ;;
esac
mkdir -p "$TEST_ROOT" "$LOG_DIR"
printf 'id\ttitle\tsource\tstatus\tcore_seen\tcore_early_exit\tfrontend_exit\tcore_log\tfrontend_log\tcaptures\tmax_core_rss_kb\tmax_frontend_rss_kb\tsave_files\tnotes\n' >"$SUMMARY"

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/0-runtime-dir}"
export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-1}"
export SWAYSOCK="${SWAYSOCK:-/run/0-runtime-dir/sway-ipc.0.sock}"

ini_value() {
  ini_file="$1"
  wanted="$2"
  [ -f "$ini_file" ] || return 0
  awk -F= -v wanted="$wanted" '
    {
      sub(/\r$/, "")
      key=$1
      gsub(/^[[:space:]]+|[[:space:]]+$/, "", key)
      key=tolower(key)
      if(key == wanted) {
        value=substr($0, index($0, "=") + 1)
        gsub(/^[[:space:]]+|[[:space:]]+$/, "", value)
        print value
        exit
      }
    }
  ' "$ini_file"
}

is_xp3_archive() {
  candidate="$1"
  [ -f "$candidate" ] || return 1
  header="$(head -c 11 "$candidate" 2>/dev/null | od -An -tx1 | tr -d ' \n')"
  [ "$header" = "5850330d0a200a1a8b6701" ]
}

is_krkr_game() {
  candidate_dir="$1"
  configured_core="$(ini_value "$candidate_dir/game.ini" core | tr '[:upper:]' '[:lower:]')"
  configured_runtime="$(ini_value "$candidate_dir/game.ini" runtime | tr '[:upper:]' '[:lower:]')"
  [ -n "$configured_runtime" ] ||
    configured_runtime="$(ini_value "$candidate_dir/game.ini" krkr_runtime | tr '[:upper:]' '[:lower:]')"
  case "$configured_core" in
    krkr|kirikiri) return 0 ;;
  esac
  case "$configured_runtime" in
    krkrsdl2|krkr2|kirikiroid2) return 0 ;;
  esac
  for marker in startup.tjs Config.tjs config.tjs; do
    [ -f "$candidate_dir/$marker" ] && return 0
  done
  for candidate in "$candidate_dir"/*; do
    [ -f "$candidate" ] || continue
    is_xp3_archive "$candidate" && return 0
  done
  return 1
}

detect_game_core() {
  candidate_dir="$1"
  configured_core="$(ini_value "$candidate_dir/game.ini" core | tr '[:upper:]' '[:lower:]')"
  case "$configured_core" in
    ons|onscripter|onsyuri) printf 'ons\n'; return 0 ;;
    krkr|kirikiri) printf 'krkr\n'; return 0 ;;
  esac
  for marker in 0.txt 00.txt nscript.dat nscript.___ arc.nsa arc.sar; do
    if [ -e "$candidate_dir/$marker" ]; then
      printf 'ons\n'
      return 0
    fi
  done
  if is_krkr_game "$candidate_dir"; then
    printf 'krkr\n'
    return 0
  fi
  return 1
}

expected_krkr_runtime() {
  candidate_dir="$1"
  case "$FORCE_KRKR_RUNTIME" in
    krkr2|krkrsdl2) printf '%s\n' "$FORCE_KRKR_RUNTIME"; return ;;
    "") ;;
    *) echo "[frontend_sweep] invalid FORCE_KRKR_RUNTIME=$FORCE_KRKR_RUNTIME" >&2; return 1 ;;
  esac
  runtime="$(ini_value "$candidate_dir/game.ini" runtime | tr '[:upper:]' '[:lower:]')"
  [ -n "$runtime" ] ||
    runtime="$(ini_value "$candidate_dir/game.ini" krkr_runtime | tr '[:upper:]' '[:lower:]')"
  case "$runtime" in
    krkr2|kirikiroid2|native) printf 'krkr2\n' ;;
    *) printf 'krkrsdl2\n' ;;
  esac
}

stable_case_id() {
  relative_dir="$1"
  if command -v sha256sum >/dev/null 2>&1; then
    digest="$(printf '%s' "$relative_dir" | sha256sum | awk '{print substr($1, 1, 12)}')"
  elif command -v md5sum >/dev/null 2>&1; then
    digest="$(printf '%s' "$relative_dir" | md5sum | awk '{print substr($1, 1, 12)}')"
  else
    digest="fallback-$DISCOVERED"
  fi
  printf 'game-%s\n' "$digest"
}

make_front_root() {
  front_root="$1"
  source_dir="$2"
  title="$3"
  core_kind="$4"
  mkdir -p "$front_root/games" "$front_root/covers" "$front_root/saves" "$front_root/cache" "$front_root/logs"
  for name in rocgalgame_sdl cores fonts sounds lib lib_system_sdl ui.pack mali_platform.config native_keymap.ini rocgalgame.png icon.png; do
    [ -e "$APP_DIR/$name" ] || continue
    ln -s "$APP_DIR/$name" "$front_root/$name" 2>/dev/null || true
  done
  game_dir_name="$title"
  if [ "$core_kind" = "krkr" ] && [ -n "$FORCE_KRKR_RUNTIME" ]; then
    # KRKRSDL2 canonicalizes the project path to lowercase on Linux.
    game_dir_name="$(printf '%s' "$title" | tr '[:upper:]' '[:lower:]')"
  fi
  game_target="$front_root/games/$game_dir_name"
  mkdir -p "$game_target"
  if [ "$core_kind" = "krkr" ] && [ -n "$FORCE_KRKR_RUNTIME" ]; then
    for source_item in "$source_dir"/*; do
      [ -e "$source_item" ] || continue
      source_name="$(basename "$source_item")"
      [ "$source_name" = "game.ini" ] && continue
      target_item="$game_target/$source_name"
      source_name_lower="$(printf '%s' "$source_name" | tr '[:upper:]' '[:lower:]')"
      if [ -d "$source_item" ]; then
        case "$source_name_lower" in
          *save*)
            mkdir -p "$target_item"
            continue
            ;;
        esac
      fi
      if [ -f "$source_item" ]; then
        : >"$target_item"
      else
        mkdir -p "$target_item"
      fi
      if mount --bind "$source_item" "$target_item" 2>/dev/null ||
         mount -o bind "$source_item" "$target_item" 2>/dev/null; then
        mount -o remount,bind,ro "$target_item" 2>/dev/null || true
        printf '%s\n' "$target_item" >>"$CURRENT_GAME_MOUNT_LIST"
      else
        if [ -f "$source_item" ]; then
          rm -f "$target_item"
        else
          rmdir "$target_item" 2>/dev/null || true
        fi
        ln -s "$source_item" "$target_item" 2>/dev/null || true
      fi
    done
    if [ -f "$source_dir/game.ini" ]; then
      awk -F= '
        {
          key=$1
          gsub(/^[[:space:]]+|[[:space:]]+$/, "", key)
          key=tolower(key)
          if (key != "core" && key != "runtime" && key != "krkr_runtime") print
        }
      ' "$source_dir/game.ini" >"$game_target/game.ini"
    fi
    printf 'core=krkr\nruntime=%s\n' "$FORCE_KRKR_RUNTIME" >>"$game_target/game.ini"
    return 0
  fi
  if mount --bind "$source_dir" "$game_target" 2>/dev/null ||
     mount -o bind "$source_dir" "$game_target" 2>/dev/null; then
    CURRENT_GAME_MOUNT="$game_target"
  else
    rmdir "$game_target" 2>/dev/null || true
    if ln -s "$source_dir" "$game_target" 2>/dev/null; then
      :
    elif [ "$ALLOW_GAME_COPY" = "1" ]; then
      mkdir -p "$game_target"
      cp -a "$source_dir"/. "$game_target"/ 2>/dev/null || true
    else
      echo "[frontend_sweep] ERROR: failed to bind or symlink game: $source_dir" >&2
      return 1
    fi
  fi
  cat >"$front_root/native_config.ini" <<EOF
screen_profile=1600x1440
input_profile=gkd350h-ultra
games_root=games
covers_root=covers
saves_root=saves
default_aspect=contain
default_filter=clean
system_language=zh
key_sound=0
system_volume_percent=50
brightness_level=4
auto_sleep_minutes=0
lid_close_screen_off=0
auto_sleep_interval_index=0
virtual_mouse=1
mouse_speed=1080
mouse_accel=1.0
EOF
}

capture_screen() {
  output="$1"
  focus_capture_target
  if command -v grim >/dev/null 2>&1; then
    case "$output" in
      *.ppm) grim -t ppm "$output" >/dev/null 2>&1 && return 0 ;;
      *) grim "$output" >/dev/null 2>&1 && return 0 ;;
    esac
  fi
  if [ -r /dev/fb0 ] && command -v ffmpeg >/dev/null 2>&1; then
    ffmpeg -hide_banner -loglevel error -f fbdev -framerate 1 -i /dev/fb0 \
      -frames:v 1 -y "$output" >/dev/null 2>&1 && return 0
  fi
  return 1
}

focus_capture_target() {
  if command -v swaymsg >/dev/null 2>&1 && [ -S "$SWAYSOCK" ]; then
    focused=0
    core_pid="$(current_core_pid "$expected_runtime")"
    if [ -n "$core_pid" ]; then
      if swaymsg -q -s "$SWAYSOCK" "[pid=$core_pid] focus" >/dev/null 2>&1; then
        focused=1
        swaymsg -q -s "$SWAYSOCK" "[pid=$core_pid] fullscreen enable" >/dev/null 2>&1 || true
      fi
    fi
    if [ -n "${CURRENT_FRONTEND_PID:-}" ]; then
      if [ "$focused" -ne 1 ] &&
         swaymsg -q -s "$SWAYSOCK" "[pid=$CURRENT_FRONTEND_PID] focus" >/dev/null 2>&1; then
        focused=1
        swaymsg -q -s "$SWAYSOCK" "[pid=$CURRENT_FRONTEND_PID] fullscreen enable" >/dev/null 2>&1 || true
      fi
    fi
    if [ "$focused" -ne 1 ]; then
      swaymsg -q -s "$SWAYSOCK" '[title="ROCgalgame"]' focus >/dev/null 2>&1 || true
      swaymsg -q -s "$SWAYSOCK" '[title="krkr2"]' focus >/dev/null 2>&1 || true
    fi
    sleep 0.2
  fi
}

capture_tree() {
  output="$1"
  focus_capture_target
  if command -v swaymsg >/dev/null 2>&1 && [ -S "$SWAYSOCK" ]; then
    swaymsg -s "$SWAYSOCK" -t get_tree >"$output" 2>/dev/null || true
  fi
}

request_core_capture() {
  request="$1"
  output="$2"
  tmp="$request.tmp"
  printf '%s\n' "$output" >"$tmp"
  mv "$tmp" "$request"
}

capture_metrics() {
  # Keep the acceptance check dependency-free on the target image. The core
  # capture hook writes binary P6 files, so process pixels instead of trusting
  # file size or process liveness.
  python3 - "$1" <<'PY'
import hashlib
import sys

path = sys.argv[1]
with open(path, "rb") as stream:
    raw = stream.read()
index = 0
tokens = []
while len(tokens) < 4:
    while index < len(raw) and raw[index] in b" \t\r\n":
        index += 1
    if index < len(raw) and raw[index] == 35:
        end = raw.find(b"\n", index)
        index = len(raw) if end < 0 else end + 1
        continue
    end = index
    while end < len(raw) and raw[end] not in b" \t\r\n":
        end += 1
    tokens.append(raw[index:end])
    index = end
if tokens[0] != b"P6":
    raise SystemExit("unsupported capture format")
width, height, maximum = (int(tokens[1]), int(tokens[2]), int(tokens[3]))
if width <= 0 or height <= 0 or maximum != 255:
    raise SystemExit("invalid capture dimensions")
if index >= len(raw) or raw[index] not in b" \t\r\n":
    raise SystemExit("missing PPM pixel separator")
if raw[index] == 13 and index + 1 < len(raw) and raw[index + 1] == 10:
    index += 2
else:
    index += 1
pixels = raw[index:index + width * height * 3]
if len(pixels) != width * height * 3:
    raise SystemExit("truncated capture")
nonblack = sum(1 for pos in range(0, len(pixels), 3)
               if max(pixels[pos:pos + 3]) > 8)
ratio = nonblack / float(width * height)
print(f"{ratio:.8f} {hashlib.sha256(pixels).hexdigest()}")
PY
}

has_terminal_error() {
  for candidate_log in "$@"; do
    [ -f "$candidate_log" ] || continue
    if tr -d '\000' <"$candidate_log" | grep -Eqi \
      'Unhandled exception|Segmentation fault|SIGSEGV|std::terminate|core dumped|Script exception raised|Cannot load Plugin|PSBArray bad'; then
      return 0
    fi
  done
  return 1
}

validate_render_captures() {
  capture_ok=0
  nonblack_ok=0
  frame_diff_ok=0
  late_hash=""
  final_hash=""
  final_ratio="0"
  last_capture_second=""
  for capture_second in $CAPTURE_SECONDS; do
    last_capture_second="$capture_second"
  done
  for capture in "$capture_dir"/screen-*.ppm; do
    [ -s "$capture" ] || continue
    metrics="$(capture_metrics "$capture" 2>/dev/null || true)"
    [ -n "$metrics" ] || continue
    ratio="${metrics%% *}"
    hash="${metrics#* }"
    capture_ok=1
    case "$capture" in
      *final.ppm) final_hash="$hash"; final_ratio="$ratio" ;;
      *"screen-${last_capture_second}s.ppm") late_hash="$hash" ;;
    esac
  done
  if awk -v value="$final_ratio" -v minimum="$MIN_NONBLACK_RATIO" \
    'BEGIN { exit !(value >= minimum) }'; then
    nonblack_ok=1
  fi
  if [ -n "$late_hash" ] && [ -n "$final_hash" ] && [ "$late_hash" != "$final_hash" ]; then
    frame_diff_ok=1
  fi
}

unlock_device_if_needed() {
  [ "$UNLOCK_BEFORE_CASE" = "1" ] || return 0
  python3 "$HELPER" --device-name gkd_atom_joypad \
    --sequence "sleep:0.5,tap:a:0.2,sleep:0.4,tap:a:0.2,sleep:0.4,tap:a:0.2" \
    --tail-seconds 0.5 >/dev/null 2>&1 || true
}

hide_iux_if_needed() {
  [ "$HIDE_IUX_BEFORE_CASE" = "1" ] || return 0
  if command -v swaymsg >/dev/null 2>&1 && [ -S "$SWAYSOCK" ]; then
    swaymsg -q -s "$SWAYSOCK" '[app_id="iux"] move scratchpad' >/dev/null 2>&1 || true
  fi
}

wait_for_frontend_window() {
  [ -n "${CURRENT_FRONTEND_PID:-}" ] || return 0
  command -v swaymsg >/dev/null 2>&1 || return 0
  [ -S "$SWAYSOCK" ] || return 0
  wait_ticks=0
  while [ "$wait_ticks" -lt 50 ]; do
    if swaymsg -s "$SWAYSOCK" -t get_tree 2>/dev/null | grep -q "\"pid\": $CURRENT_FRONTEND_PID"; then
      return 0
    fi
    sleep 0.1
    wait_ticks=$((wait_ticks + 1))
  done
  echo "[frontend_sweep] WARN frontend window not observed pid=${CURRENT_FRONTEND_PID:-0}" >&2
  return 0
}

write_pointer_request_if_due() {
  elapsed="$1"
  request="$2"
  for item in $POINTER_CLICK_SECONDS; do
    second="${item%%:*}"
    rest="${item#*:}"
    x="${rest%%:*}"
    rest="${rest#*:}"
    y="${rest%%:*}"
    [ "$elapsed" = "$second" ] || continue
    if [ "${expected_runtime:-}" = "krkrsdl2" ] &&
       command -v swaymsg >/dev/null 2>&1 && [ -S "$SWAYSOCK" ]; then
      swaymsg -q -s "$SWAYSOCK" "seat seat0 cursor set $x $y" >/dev/null 2>&1 || true
      if [ -f "$POINTER_HELPER" ]; then
        python3 "$POINTER_HELPER" >/dev/null 2>&1 || true
      else
        swaymsg -q -s "$SWAYSOCK" "seat seat0 cursor press button1" >/dev/null 2>&1 || true
        sleep 0.15
        swaymsg -q -s "$SWAYSOCK" "seat seat0 cursor release button1" >/dev/null 2>&1 || true
      fi
      continue
    fi
    tmp="$request.tmp"
    printf '%s %s 1\n' "$x" "$y" >"$tmp"
    mv "$tmp" "$request"
  done
}

run_case() {
  id="$1"
  title="$2"
  source_dir="$3"
  core_kind="$4"
  if [ "$core_kind" = "krkr" ]; then
    expected_runtime="$(expected_krkr_runtime "$source_dir")"
    nav_index=1
  else
    expected_runtime=onsyuri
    nav_index=0
  fi
  case "$CASE_FILTER" in
    "") ;;
    *",$id,"*) ;;
    *) echo "[frontend_sweep] SKIP id=$id title=$title"; return ;;
  esac
  if [ -n "$TITLE_REGEX" ] && ! printf '%s\n' "$title" | grep -Eq "$TITLE_REGEX"; then
    echo "[frontend_sweep] SKIP title_filter id=$id title=$title"
    return
  fi
  if [ -n "$TITLE_EXCLUDE_REGEX" ] && printf '%s\n' "$title" | grep -Eq "$TITLE_EXCLUDE_REGEX"; then
    echo "[frontend_sweep] SKIP title_exclude id=$id title=$title"
    return
  fi
  if [ "$MAX_CASES" -gt 0 ] && [ "$RUN_COUNT" -ge "$MAX_CASES" ]; then
    return
  fi
  RUN_COUNT=$((RUN_COUNT + 1))

  kill_stale_uinput_helpers
  kill_runtime_processes
  unmount_current_game_view
  hide_iux_if_needed
  unlock_device_if_needed

  case_dir="$TEST_ROOT/$id"
  front_root="$case_dir/root"
  capture_dir="$LOG_DIR/$id-captures"
  frontend_log="$LOG_DIR/$id.frontend.log"
  uinput_log="$LOG_DIR/$id.uinput.log"
  memory_log="$LOG_DIR/$id.memory.tsv"
  ready_file="$case_dir/uinput.ready"
  core_capture_request="$case_dir/core-capture.request"
  pointer_request="$case_dir/pointer.request"
  mkdir -p "$case_dir" "$capture_dir"
  : >"$core_capture_request"
  : >"$pointer_request"
  CURRENT_GAME_MOUNT_LIST="$case_dir/game-view-mounts.list"
  : >"$CURRENT_GAME_MOUNT_LIST"
  make_front_root "$front_root" "$source_dir" "$title" "$core_kind"
  if [ "$?" -ne 0 ]; then
    unmount_current_game_view
    FAILED_COUNT=$((FAILED_COUNT + 1))
    printf '%s\t%s\t%s\t%s\t0\t0\t0\t\t%s\t%s\t0\t0\t0\t%s\n' \
      "$id" "$title" "$source_dir" "setup_failed" "$frontend_log" \
      "$capture_dir" "failed_to_prepare_front_root" >>"$SUMMARY"
    echo "[frontend_sweep] DONE id=$id title=$title status=setup_failed"
    return
  fi

  sequence="$INPUT_SEQUENCE"

  echo "[frontend_sweep] START id=$id core=$core_kind runtime=$expected_runtime title=$title"
  python3 "$HELPER" --device-name gkd_atom_joypad --ready-file "$ready_file" --sequence "$sequence" \
    >"$uinput_log" 2>&1 &
  CURRENT_UINPUT_PID=$!
  wait_ready=0
  while [ ! -f "$ready_file" ] && [ "$wait_ready" -lt 30 ]; do
    sleep 0.1
    wait_ready=$((wait_ready + 1))
  done

  env \
    XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" \
    WAYLAND_DISPLAY="$WAYLAND_DISPLAY" \
    SWAYSOCK="$SWAYSOCK" \
    GDK_BACKEND="${GDK_BACKEND:-wayland}" \
    SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-wayland}" \
    SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-alsa}" \
    SDL_NOMOUSE=1 \
    ROCGALGAME_ROOT="$front_root" \
    ROCGALGAME_AUTOLAUNCH_FIRST=1 \
    ROCGALGAME_NAV_INDEX="$nav_index" \
    ROCGALGAME_SCREEN_PROFILE=1600x1440 \
    ROCGALGAME_DEVICE_MODEL=gkd350h-ultra \
    ROCGALGAME_KRKR_DISPLAY_BACKEND="${ROCGALGAME_KRKR_DISPLAY_BACKEND:-wayland}" \
    ROCGALGAME_KRKR_PRESENTATION_CAPTURE_REQUEST="$core_capture_request" \
    ROCGALGAME_KRKR_POINTER_REQUEST="$pointer_request" \
    ROCGALGAME_KRKR_PRESENTATION_PROBE="${ROCGALGAME_KRKR_PRESENTATION_PROBE:-1}" \
    ROCGALGAME_KRKR_SWAP_PROBE="${ROCGALGAME_KRKR_SWAP_PROBE:-1}" \
    ROCGALGAME_KRKR_EGL_PROBE="${ROCGALGAME_KRKR_EGL_PROBE:-1}" \
    MALI_PLATFORM_CONFIG="${MALI_PLATFORM_CONFIG:-$front_root/mali_platform.config}" \
    MALI_WAYLAND_DMABUF_PROTOCOL="${MALI_WAYLAND_DMABUF_PROTOCOL:-1}" \
    LD_LIBRARY_PATH="$APP_DIR/lib_system_sdl:$APP_DIR/lib:/usr/lib32:/usr/lib:/lib:/mnt/vendor/lib" \
    "$APP_DIR/rocgalgame_sdl" >"$frontend_log" 2>&1 &
  CURRENT_FRONTEND_PID=$!
  wait_for_frontend_window

  printf 'elapsed\tfrontend_pid\tcore_pid\tfrontend_rss_kb\tcore_rss_kb\n' >"$memory_log"
  core_seen=0
  core_early_exit=0
  frontend_early_exit=0
  max_core_rss=0
  max_front_rss=0
  last_core_seen=0
  missing_core_ticks=0
  elapsed=0
  while [ "$elapsed" -lt "$RUN_SECONDS" ]; do
    front_pid="$CURRENT_FRONTEND_PID"
    if ! kill -0 "$CURRENT_FRONTEND_PID" 2>/dev/null; then
      front_pid=""
      frontend_early_exit=1
    fi
    core_pid="$(current_core_pid "$expected_runtime")"
    front_rss=0
    core_rss=0
    if [ -n "$front_pid" ]; then
      front_rss="$(awk '/^VmRSS:/ {print $2; exit}' "/proc/$front_pid/status" 2>/dev/null || echo 0)"
    fi
    if [ -n "$core_pid" ]; then
      core_seen=1
      last_core_seen="$elapsed"
      missing_core_ticks=0
      core_rss="$(awk '/^VmRSS:/ {print $2; exit}' "/proc/$core_pid/status" 2>/dev/null || echo 0)"
    elif [ "$core_seen" -eq 1 ]; then
      missing_core_ticks=$((missing_core_ticks + 1))
      if [ "$missing_core_ticks" -ge 3 ] && [ "$elapsed" -lt $((RUN_SECONDS - 8)) ]; then
      core_early_exit=1
      fi
    fi
    case "$front_rss" in ''|*[!0-9]*) front_rss=0 ;; esac
    case "$core_rss" in ''|*[!0-9]*) core_rss=0 ;; esac
    [ "$front_rss" -gt "$max_front_rss" ] && max_front_rss="$front_rss"
    [ "$core_rss" -gt "$max_core_rss" ] && max_core_rss="$core_rss"
    printf '%s\t%s\t%s\t%s\t%s\n' "$elapsed" "${front_pid:-0}" "${core_pid:-0}" "$front_rss" "$core_rss" >>"$memory_log"
    if [ "$frontend_early_exit" -eq 1 ]; then
      echo "[frontend_sweep] FRONTEND_EARLY_EXIT id=$id elapsed=$elapsed"
      break
    fi
    if [ "$core_early_exit" -eq 1 ]; then
      echo "[frontend_sweep] CORE_EARLY_EXIT id=$id elapsed=$elapsed last_core_seen=$last_core_seen"
      break
    fi
    if [ "$RSS_LIMIT_KB" -gt 0 ] && [ "$core_rss" -gt "$RSS_LIMIT_KB" ]; then
      echo "[frontend_sweep] RSS_LIMIT id=$id rss_kb=$core_rss"
      [ -n "$core_pid" ] && kill -TERM "$core_pid" 2>/dev/null || true
      break
    fi
    for capture_at in $CAPTURE_SECONDS; do
      if [ "$elapsed" = "$capture_at" ]; then
        capture_tree "$capture_dir/sway-tree-${elapsed}.json"
        capture_screen "$capture_dir/screen-${elapsed}s.ppm" || true
        request_core_capture "$core_capture_request" "$capture_dir/core-${elapsed}s.ppm"
      fi
    done
    write_pointer_request_if_due "$elapsed" "$pointer_request"
    sleep 1
    elapsed=$((elapsed + 1))
  done

  sleep 2
  capture_tree "$capture_dir/sway-tree-final.json"
  capture_screen "$capture_dir/screen-final.ppm" || true
  request_core_capture "$core_capture_request" "$capture_dir/core-final.ppm"
  sleep 1

  if [ -n "$CURRENT_FRONTEND_PID" ] && kill -0 "$CURRENT_FRONTEND_PID" 2>/dev/null; then
    kill -TERM "$CURRENT_FRONTEND_PID" 2>/dev/null || true
    sleep 1
    kill -KILL "$CURRENT_FRONTEND_PID" 2>/dev/null || true
  fi
  wait "$CURRENT_FRONTEND_PID" 2>/dev/null
  frontend_exit=$?
  CURRENT_FRONTEND_PID=""
  if [ -n "$CURRENT_UINPUT_PID" ] && kill -0 "$CURRENT_UINPUT_PID" 2>/dev/null; then
    kill -TERM "$CURRENT_UINPUT_PID" 2>/dev/null || true
  fi
  wait "$CURRENT_UINPUT_PID" 2>/dev/null || true
  CURRENT_UINPUT_PID=""
  kill_runtime_processes
  unmount_current_game_view

  core_log="$(find "$front_root/logs/$core_kind" -type f -name '*.log' 2>/dev/null | sort | tail -n 1)"
  console_log="$(find "$front_root/saves/krkr" -type f -name 'krkr.console.log' 2>/dev/null | sort | tail -n 1)"
  save_files="$(find "$front_root/saves" -type f -print 2>/dev/null | wc -l | tr -d ' ')"
  notes="core=${core_kind};runtime=${expected_runtime};last_core_seen=${last_core_seen};frontend_early_exit=${frontend_early_exit}"
  status=pass
  if [ "$core_seen" -ne 1 ]; then
    status=no_core_seen
  elif [ "$frontend_early_exit" -eq 1 ]; then
    status=frontend_early_exit
  elif [ "$core_early_exit" -eq 1 ]; then
    status=core_early_exit
  elif has_terminal_error "$core_log" "$console_log" "$frontend_log"; then
    status=exception_in_log
  elif [ "$max_core_rss" -gt "$RSS_LIMIT_KB" ] && [ "$RSS_LIMIT_KB" -gt 0 ]; then
    status=rss_limit
  fi
  gl_ok=1
  fbo_ok=1
  swap_ok=1
  if [ "$expected_runtime" = "krkr2" ]; then
    gl_ok=0
    fbo_ok=0
    swap_ok=0
    if grep -Eq 'GL_VENDOR=[^ ]+ GL_RENDERER=[^ ]+ GL_VERSION=[^ ]+' "$core_log" 2>/dev/null &&
       ! grep -Eqi 'GL_VENDOR=unavailable|GL_RENDERER=unavailable|GL_VERSION=unavailable' "$core_log" 2>/dev/null; then
      gl_ok=1
    fi
    if grep -Eqi 'presentation_probe.*fbo=0x8CD5.*pending=0x0000.*read=0x0000' "$core_log" 2>/dev/null; then
      fbo_ok=1
    fi
    if grep -Eqi 'swap_probe .*fbo=0 .*status=0x8CD5 .*pending=0x0000 .*read=0x0000 .*sample_or=0x(FF|[0-9A-Fa-f]*[1-9A-Fa-f])' "$core_log" 2>/dev/null; then
      swap_ok=1
    fi
  fi
  validate_render_captures
  notes="${notes};gl_ok=${gl_ok};fbo_ok=${fbo_ok};swap_ok=${swap_ok};capture_ok=${capture_ok};nonblack_ok=${nonblack_ok};frame_diff_ok=${frame_diff_ok};final_nonblack_ratio=${final_ratio};console_log=${console_log}"
  if [ "$status" = "pass" ] && [ "$gl_ok" -ne 1 ]; then
    status=gl_context_invalid
  elif [ "$status" = "pass" ] && [ "$fbo_ok" -ne 1 ]; then
    status=fbo_invalid
  elif [ "$status" = "pass" ] && [ "$REQUIRE_SWAP_FRAME" = "1" ] && [ "$swap_ok" -ne 1 ]; then
    status=swap_frame_invalid
  elif [ "$status" = "pass" ] && [ "$capture_ok" -ne 1 ]; then
    status=render_capture_missing
  elif [ "$status" = "pass" ] && [ "$nonblack_ok" -ne 1 ]; then
    status=render_capture_black
  elif [ "$status" = "pass" ] && [ "$REQUIRE_FRAME_DIFF" = "1" ] && [ "$frame_diff_ok" -ne 1 ]; then
    status=render_frame_unchanged
  fi
  [ "$status" = "pass" ] || FAILED_COUNT=$((FAILED_COUNT + 1))
  if command -v swaymsg >/dev/null 2>&1 && [ -S "$SWAYSOCK" ]; then
    [ -f "$capture_dir/sway-tree-final.json" ] || swaymsg -s "$SWAYSOCK" -t get_tree >"$capture_dir/sway-tree-final.json" 2>&1 || true
  fi
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
    "$id" "$title" "$source_dir" "$status" "$core_seen" "$core_early_exit" \
    "$frontend_exit" "${core_log:-}" "$frontend_log" "$capture_dir" \
    "$max_core_rss" "$max_front_rss" "$save_files" "$notes" >>"$SUMMARY"
  echo "[frontend_sweep] DONE id=$id title=$title status=$status core_seen=$core_seen gl_ok=$gl_ok fbo_ok=$fbo_ok swap_ok=$swap_ok nonblack_ok=$nonblack_ok frame_diff_ok=$frame_diff_ok max_core_rss=$max_core_rss captures=$capture_dir"
}

discover_bucket() {
  bucket="$1"
  forced_core="${2:-}"
  [ -d "$bucket" ] || return 0
  for source_dir in "$bucket"/*; do
    [ -d "$source_dir" ] || continue
    if [ -n "$forced_core" ]; then
      core_kind="$forced_core"
    else
      core_kind="$(detect_game_core "$source_dir" || true)"
    fi
    [ -n "$core_kind" ] || continue
    if [ "$CORE_FILTER" != "all" ] && [ "$CORE_FILTER" != "$core_kind" ]; then
      continue
    fi
    DISCOVERED=$((DISCOVERED + 1))
    title="$(basename "$source_dir" | tr '\t\r' '  ')"
    relative_dir="${source_dir#"$GAMES_DIR"/}"
    id="$(stable_case_id "$relative_dir")"
    if [ "$DISCOVER_ONLY" = "1" ]; then
      printf '%s\t%s\t%s\tdiscovered\t0\t0\t0\t\t\t\t0\t0\t0\t\n' "$id" "$title" "$source_dir" >>"$SUMMARY"
      echo "[frontend_sweep] DISCOVER id=$id core=$core_kind title=$title source=$source_dir"
    else
      run_case "$id" "$title" "$source_dir" "$core_kind"
    fi
  done
}

discover_bucket "$GAMES_DIR"
discover_bucket "$GAMES_DIR/ons" ons
discover_bucket "$GAMES_DIR/krkr" krkr

echo "[frontend_sweep] DISCOVERED=$DISCOVERED RUN=$RUN_COUNT"
echo "[frontend_sweep] FAILED=$FAILED_COUNT"
echo "[frontend_sweep] SUMMARY=$SUMMARY"
cat "$SUMMARY"
[ "$FAILED_COUNT" -eq 0 ] || exit 1
