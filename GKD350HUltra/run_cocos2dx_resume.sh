#!/bin/bash
set -euo pipefail

BUILD_DIR="${COCOS2DX_BUILD_DIR:-/mnt/d/Works/ROCgalgame/build/gkd350h-glibc234/vcpkg/buildtrees/cocos2dx/arm64-linux-gkd-glibc234-rel}"
NINJA="${NINJA:-/mnt/d/Works/ROCgalgame/build/gkd350h-glibc234/vcpkg/downloads/tools/ninja-1.13.2-linux/ninja}"
LOG_FILE="${COCOS2DX_LOG_FILE:-/mnt/d/Works/ROCgalgame/GKD350HUltra/logs/glibc234/cocos2dx_gles2_resume.log}"
WORK_SECONDS="${KRKR2_WORK_SECONDS:-300}"
COOL_SECONDS="${KRKR2_COOL_SECONDS:-60}"
SAFE_CPU_SET="${KRKR2_SAFE_CPU_SET:-0}"

cd "$BUILD_DIR"
: > "$LOG_FILE"

targets=("$@")
if [ "${#targets[@]}" -eq 0 ]; then
  targets=(install)
fi

setsid nice -n 19 ionice -c 3 taskset -c "$SAFE_CPU_SET" \
  "$NINJA" -v -j1 "${targets[@]}" >>"$LOG_FILE" 2>&1 &
command_pid=$!

(
  while kill -0 "$command_pid" 2>/dev/null; do
    sleep "$WORK_SECONDS"
    kill -0 "$command_pid" 2>/dev/null || exit 0
    echo "[cocos_resume] cooling pause: ${COOL_SECONDS}s after ${WORK_SECONDS}s" >>"$LOG_FILE"
    ps -o pid= --sid "$command_pid" | xargs -r kill -STOP || true
    sleep "$COOL_SECONDS"
    ps -o pid= --sid "$command_pid" | xargs -r kill -CONT || true
    echo "[cocos_resume] cooling complete; resuming" >>"$LOG_FILE"
  done
) &
controller_pid=$!

set +e
wait "$command_pid"
status=$?
set -e

kill "$controller_pid" 2>/dev/null || true
ps -o pid= --sid "$command_pid" | xargs -r kill -CONT || true
wait "$controller_pid" 2>/dev/null || true
exit "$status"
