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

cleanup() {
  if [ -n "$CURRENT_PID" ] && kill -0 "$CURRENT_PID" 2>/dev/null; then
    kill -TERM "$CURRENT_PID" 2>/dev/null || true
    sleep 1
    kill -KILL "$CURRENT_PID" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

test -x "$CORE" || { echo "[sweep] missing core: $CORE"; exit 2; }
mkdir -p "$TEST_ROOT" "$LOG_DIR"
printf 'id\ttitle\tentry\tstatus\talive\twindow\tstartup\trenderer\texceptions\tplugin_errors\tsave_files\texit_code\tlog\n' >"$SUMMARY"

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

run_case() {
  id="$1"
  title="$2"
  entry="$3"
  case ",${CASE_FILTER:-}," in
    ',,') ;;
    *",$id,"*) ;;
    *) echo "[sweep] SKIP id=$id"; return ;;
  esac
  source_dir="$GAMES_DIR/$title"
  case_dir="$TEST_ROOT/$id"
  log_file="$LOG_DIR/$id.log"
  tree_file="$LOG_DIR/$id.sway-tree.json"

  echo "[sweep] START id=$id title=$title entry=$entry"
  if [ ! -d "$source_dir" ] || [ ! -e "$source_dir/$entry" ]; then
    printf '%s\t%s\t%s\tmissing_entry\t0\t0\t0\tmissing\t0\t0\t0\t127\t%s\n' \
      "$id" "$title" "$entry" "$log_file" >>"$SUMMARY"
    echo "[sweep] DONE id=$id status=missing_entry"
    return
  fi

  mkdir -p "$case_dir/savedata"
  for source_item in "$source_dir"/*; do
    [ -e "$source_item" ] || continue
    copy_or_link_item "$source_item" "$case_dir"
  done

  cd "$case_dir" || return
  "$CORE" "$case_dir/$entry" >"$log_file" 2>&1 &
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
  save_files="$(find "$case_dir/savedata" -type f -print 2>/dev/null | wc -l | tr -d ' ')"

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

  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
    "$id" "$title" "$entry" "$status" "$alive" "$window" "$startup" \
    "$renderer" "$exceptions" "$plugin_errors" "$save_files" \
    "$exit_code" "$log_file" >>"$SUMMARY"
  echo "[sweep] DONE id=$id status=$status alive=$alive window=$window startup=$startup renderer=$renderer exceptions=$exceptions plugin_errors=$plugin_errors save_files=$save_files exit_code=$exit_code"
}

run_case neko0 'NEKOPARA Vol.0' 'data.xp3'
run_case neko2 'NEKOPARA Vol.2' 'data.xp3'
run_case haramase_isekai 'もっと！孕ませ！炎のおっぱい異世界エロ魔法学園！' 'data.xp3'
run_case mofuku '丧服萝莉紧缚奇谭 美少女性奴隶调教' 'data.xp3'
run_case senren '千恋万花' 'data.xp3'
run_case amae_mama '向妈妈撒娇吧！' 'data.bin'
run_case hmaho '吹弹！丰盈！波涛汹涌！异世界魔法学园！' 'data.xp3'
run_case kisaragi '如月真绫的指导' '运行游戏.xp3'
run_case momoiro '桃色恋恋 ～与姐妹相系的H关系～' 'data.xp3'
run_case app_gakuen '超工口APP学园／全部！怀孕！超色情爆乳▼APP学园！' 'data.xp3'

echo "[sweep] SUMMARY=$SUMMARY"
cat "$SUMMARY"
