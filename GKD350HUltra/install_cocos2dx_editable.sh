#!/bin/bash
set -euo pipefail

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SELF_DIR/.." && pwd)"
BUILD_ROOT="${KRKR2_BUILD_ROOT:-$REPO_ROOT/build/gkd350h-glibc234}"
VCPKG_ROOT="${VCPKG_ROOT:-$BUILD_ROOT/vcpkg}"
KRKR2_ROOT="${KRKR2_ROOT:-/mnt/d/Works/ROCgalgame-krkr2-port}"
SYSROOT="${SYSROOT:-$BUILD_ROOT/sysroot}"
INSTALL_ROOT="${VCPKG_INSTALL_ROOT:-$BUILD_ROOT/krkr2/vcpkg_installed}"
TRIPLET="arm64-linux-gkd-glibc234"
WORK_SECONDS="${KRKR2_WORK_SECONDS:-300}"
COOL_SECONDS="${KRKR2_COOL_SECONDS:-60}"
SAFE_CPU_SET="${KRKR2_SAFE_CPU_SET:-0}"
LOG_FILE="${COCOS2DX_LOG_FILE:-$SELF_DIR/logs/cocos2dx_editable_install.log}"

export GKD_SYSROOT="$SYSROOT"
export VCPKG_MAX_CONCURRENCY=1
export CMAKE_BUILD_PARALLEL_LEVEL=1
export MAKEFLAGS=-j1
export OMP_NUM_THREADS=1

: > "$LOG_FILE"
setsid nice -n 19 ionice -c 3 taskset -c "$SAFE_CPU_SET" \
  "$VCPKG_ROOT/vcpkg" install "cocos2dx:$TRIPLET" --editable \
  --triplet "$TRIPLET" \
  --host-triplet x64-linux \
  --x-install-root="$INSTALL_ROOT" \
  --overlay-triplets="$SELF_DIR/vcpkg-triplets" \
  --overlay-ports="$SELF_DIR/vcpkg-ports" \
  --overlay-ports="$KRKR2_ROOT/vcpkg/ports" >>"$LOG_FILE" 2>&1 &
command_pid=$!

(
  while kill -0 "$command_pid" 2>/dev/null; do
    sleep "$WORK_SECONDS"
    kill -0 "$command_pid" 2>/dev/null || exit 0
    echo "[cocos_editable] cooling pause: ${COOL_SECONDS}s after ${WORK_SECONDS}s" >>"$LOG_FILE"
    ps -o pid= --sid "$command_pid" | xargs -r kill -STOP || true
    sleep "$COOL_SECONDS"
    ps -o pid= --sid "$command_pid" | xargs -r kill -CONT || true
    echo "[cocos_editable] cooling complete; resuming" >>"$LOG_FILE"
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
