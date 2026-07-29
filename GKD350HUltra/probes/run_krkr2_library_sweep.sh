#!/bin/sh
set -u

APP_DIR="${APP_DIR:-/storage/roms/ports/ROCgalgame}"
GAMES_DIR="${GAMES_DIR:-$APP_DIR/games}"
CORE="${CORE:-$APP_DIR/cores/krkr/krkr2}"
RUN_SECONDS="${RUN_SECONDS:-20}"
STAMP="$(date +%Y%m%d-%H%M%S)"
TEST_ROOT="${TEST_ROOT:-/tmp/rocgalgame-krkr2-library-sweep-$STAMP}"
LOG_DIR="${LOG_DIR:-$APP_DIR/logs/krkr2-library-sweep-$STAMP}"
SUMMARY="$LOG_DIR/summary.tsv"
CURRENT_PID=""
DISCOVERED=0
DISCOVER_ONLY="${DISCOVER_ONLY:-0}"

cleanup() {
  if [ -n "$CURRENT_PID" ] && kill -0 "$CURRENT_PID" 2>/dev/null; then
    kill -TERM "$CURRENT_PID" 2>/dev/null || true
    sleep 1
    kill -KILL "$CURRENT_PID" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

if [ "$DISCOVER_ONLY" != "1" ]; then
  test -x "$CORE" || { echo "[sweep] missing core: $CORE"; exit 2; }
fi
mkdir -p "$TEST_ROOT" "$LOG_DIR"
printf 'id\ttitle\tsource\tentry\tstatus\talive\twindow\tstartup\trenderer\texceptions\tplugin_errors\tsave_files\texit_code\tlog\n' >"$SUMMARY"

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
export LD_LIBRARY_PATH="/usr/lib/mali:$APP_DIR/cores/krkr/lib_krkr2:$APP_DIR/lib_system_sdl:$APP_DIR/lib:/usr/lib:/lib:/mnt/vendor/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
unset DISPLAY

copy_or_link_item() {
  source_item="$1"
  destination="$2"
  name="$(basename "$source_item")"
  [ "$name" = "savedata" ] && return 0

  if [ -d "$source_item" ]; then
    size_kb="$(du -sk "$source_item" 2>/dev/null | awk '{print $1}')"
    case "$size_kb" in ''|*[!0-9]*) size_kb=999999;; esac
    if [ "$size_kb" -le 8192 ]; then
      cp -a "$source_item" "$destination/"
    else
      ln -s "$source_item" "$destination/$name"
    fi
    return 0
  fi

  size_bytes="$(stat -c %s "$source_item" 2>/dev/null || echo 999999999)"
  case "$size_bytes" in ''|*[!0-9]*) size_bytes=999999999;; esac
  if [ "$size_bytes" -le 2097152 ]; then
    cp -p "$source_item" "$destination/$name"
  else
    ln -s "$source_item" "$destination/$name"
  fi
}

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
  ini_file="$candidate_dir/game.ini"
  configured_core="$(ini_value "$ini_file" core | tr '[:upper:]' '[:lower:]')"
  configured_runtime="$(ini_value "$ini_file" runtime | tr '[:upper:]' '[:lower:]')"
  [ -n "$configured_runtime" ] ||
    configured_runtime="$(ini_value "$ini_file" krkr_runtime | tr '[:upper:]' '[:lower:]')"
  case "$configured_core" in
    krkr|kirikiri)
      return 0
      ;;
  esac
  case "$configured_runtime" in
    krkrsdl2|krkr2|kirikiroid2)
      return 0
      ;;
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

detect_entry() {
  candidate_dir="$1"
  configured_entry="$(ini_value "$candidate_dir/game.ini" entry)"
  if [ -n "$configured_entry" ]; then
    printf '%s\n' "$configured_entry"
    return
  fi
  if is_xp3_archive "$candidate_dir/data.xp3"; then
    printf '%s\n' 'data.xp3'
    return
  fi
  if [ -f "$candidate_dir/startup.tjs" ]; then
    printf '%s\n' '.'
    return
  fi

  archive_count=0
  only_archive=""
  preferred_count=0
  preferred_archive=""
  for candidate in "$candidate_dir"/*; do
    [ -f "$candidate" ] || continue
    is_xp3_archive "$candidate" || continue
    filename="$(basename "$candidate")"
    lower_filename="$(printf '%s' "$filename" | tr '[:upper:]' '[:lower:]')"
    case "$lower_filename" in patch*) continue ;; esac
    archive_count=$((archive_count + 1))
    only_archive="$filename"
    case "$lower_filename" in
      data.*)
        preferred_count=$((preferred_count + 1))
        preferred_archive="$filename"
        ;;
    esac
  done
  if [ "$preferred_count" -eq 1 ]; then
    printf '%s\n' "$preferred_archive"
  elif [ "$archive_count" -eq 1 ]; then
    printf '%s\n' "$only_archive"
  fi
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

run_case() {
  id="$1"
  title="$2"
  source_dir="$3"
  entry="$4"
  case ",${CASE_FILTER:-}," in
    ',,') ;;
    *",$id,"*) ;;
    *) echo "[sweep] SKIP id=$id"; return ;;
  esac
  case_dir="$TEST_ROOT/$id"
  save_dir="$TEST_ROOT/save-roots/$id"
  log_file="$LOG_DIR/$id.log"
  tree_file="$LOG_DIR/$id.sway-tree.json"

  echo "[sweep] START id=$id title=$title source=$source_dir entry=$entry"
  if [ ! -d "$source_dir" ] || [ ! -e "$source_dir/$entry" ]; then
    printf '%s\t%s\t%s\t%s\tmissing_entry\t0\t0\t0\tmissing\t0\t0\t0\t127\t%s\n' \
      "$id" "$title" "$source_dir" "$entry" "$log_file" >>"$SUMMARY"
    echo "[sweep] DONE id=$id status=missing_entry"
    return
  fi

  mkdir -p "$case_dir" "$save_dir"
  for source_item in "$source_dir"/*; do
    [ -e "$source_item" ] || continue
    copy_or_link_item "$source_item" "$case_dir"
  done

  cd "$case_dir" || return
  if [ "${USE_SAVE_ENV:-1}" = "1" ]; then
    ROCGALGAME_KRKR_SAVE_PATH="$save_dir" \
      "$CORE" "$case_dir/$entry" >"$log_file" 2>&1 &
  else
    "$CORE" "$case_dir/$entry" >"$log_file" 2>&1 &
  fi
  CURRENT_PID=$!
  sleep "$RUN_SECONDS"

  alive=0
  if kill -0 "$CURRENT_PID" 2>/dev/null; then alive=1; fi

  window=0
  if command -v swaymsg >/dev/null 2>&1 && [ -S "$SWAYSOCK" ]; then
    swaymsg -s "$SWAYSOCK" -t get_tree >"$tree_file" 2>&1 || true
    if grep -qi 'krkr2' "$tree_file"; then window=1; fi
  fi

  if [ "$alive" -eq 1 ]; then
    kill -TERM "$CURRENT_PID" 2>/dev/null || true
    sleep 2
    if kill -0 "$CURRENT_PID" 2>/dev/null; then
      kill -KILL "$CURRENT_PID" 2>/dev/null || true
    fi
  fi
  wait "$CURRENT_PID" 2>/dev/null
  exit_code=$?
  CURRENT_PID=""

  startup=0
  if grep -q 'Startup script ended' "$log_file"; then startup=1; fi
  renderer="$(sed -n 's/^.*OpenGL ES renderer:[[:space:]]*//p' "$log_file" | tail -n 1)"
  [ -n "$renderer" ] || renderer=missing
  exceptions="$(grep -Eic 'An exception occurred|Unhandled exception|Segmentation fault|SIGSEGV|std::terminate' "$log_file" 2>/dev/null || true)"
  plugin_errors="$(grep -Eic 'Loading Plugin:.*Failed' "$log_file" 2>/dev/null || true)"
  save_files="$(find "$save_dir" -type f -print 2>/dev/null | wc -l | tr -d ' ')"

  status=pass
  if [ "$alive" -ne 1 ]; then
    status=early_exit
  elif [ "$window" -ne 1 ]; then
    status=no_window
  elif [ "$exceptions" -gt 0 ]; then
    status=alive_with_exception
  elif [ "$startup" -ne 1 ]; then
    status=alive_no_startup_marker
  elif [ "$renderer" != "Mali-G52" ]; then
    status=non_mali_renderer
  fi

  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
    "$id" "$title" "$source_dir" "$entry" "$status" "$alive" "$window" "$startup" \
    "$renderer" "$exceptions" "$plugin_errors" "$save_files" \
    "$exit_code" "$log_file" >>"$SUMMARY"
  echo "[sweep] DONE id=$id status=$status alive=$alive window=$window startup=$startup renderer=$renderer exceptions=$exceptions plugin_errors=$plugin_errors save_files=$save_files exit_code=$exit_code"
}

discover_bucket() {
  bucket="$1"
  [ -d "$bucket" ] || return 0
  for source_dir in "$bucket"/*; do
    [ -d "$source_dir" ] || continue
    is_krkr_game "$source_dir" || continue

    DISCOVERED=$((DISCOVERED + 1))
    title="$(basename "$source_dir" | tr '\t\r' '  ')"
    relative_dir="${source_dir#"$GAMES_DIR"/}"
    id="$(stable_case_id "$relative_dir")"
    entry="$(detect_entry "$source_dir")"

    if [ -z "$entry" ]; then
      log_file="$LOG_DIR/$id.log"
      printf '%s\t%s\t%s\t\tambiguous_entry\t0\t0\t0\tmissing\t0\t0\t0\t127\t%s\n' \
        "$id" "$title" "$source_dir" "$log_file" >>"$SUMMARY"
      echo "[sweep] DISCOVER id=$id title=$title source=$source_dir status=ambiguous_entry"
      continue
    fi

    echo "[sweep] DISCOVER id=$id title=$title source=$source_dir entry=$entry"
    if [ "$DISCOVER_ONLY" = "1" ]; then
      printf '%s\t%s\t%s\t%s\tdiscovered\t0\t0\t0\tmissing\t0\t0\t0\t0\t\n' \
        "$id" "$title" "$source_dir" "$entry" >>"$SUMMARY"
    else
      run_case "$id" "$title" "$source_dir" "$entry"
    fi
  done
}

discover_bucket "$GAMES_DIR"
discover_bucket "$GAMES_DIR/krkr"

if [ "$DISCOVERED" -eq 0 ]; then
  echo "[sweep] no KRKR games discovered under $GAMES_DIR" >&2
  exit 3
fi

echo "[sweep] DISCOVERED=$DISCOVERED"
echo "[sweep] SUMMARY=$SUMMARY"
cat "$SUMMARY"
