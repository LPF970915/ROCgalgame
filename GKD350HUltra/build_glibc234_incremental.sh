#!/bin/bash
set -euo pipefail

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SELF_DIR/.." && pwd)"
BUILD_ROOT="$REPO_ROOT/build/gkd350h-glibc234"
SYSROOT="$BUILD_ROOT/sysroot"
DIST_ROOT="$SELF_DIR/dist_glibc234"
TARGET="${1:-All}"
LOG_DIR="$SELF_DIR/logs/glibc234"
VCPKG_ROOT="$BUILD_ROOT/vcpkg"
BUILD_JOBS="${GLIBC234_BUILD_JOBS:-1}"
SAFE_CPU_SET="${GLIBC234_SAFE_CPU_SET:-0}"

case "$BUILD_JOBS" in
  ''|*[!0-9]*) echo "[incremental] ERROR: GLIBC234_BUILD_JOBS must be an integer from 1 to 4"; exit 2 ;;
esac
if [ "$BUILD_JOBS" -lt 1 ] || [ "$BUILD_JOBS" -gt 4 ]; then
  echo "[incremental] ERROR: GLIBC234_BUILD_JOBS must remain between 1 and 4"
  exit 2
fi

case "$TARGET" in Frontend|ONS|KRKRSDL2|KRKR2|All) ;; *)
  echo "usage: $0 {Frontend|ONS|KRKRSDL2|KRKR2|All}"; exit 2 ;;
esac
test -f "$SYSROOT/rocgalgame_glibc234_baseline.txt" || {
  echo "[incremental] ERROR: run the full glibc 2.34 baseline build first"; exit 1;
}

export ROCGALGAME_GLIBC_BASELINE=2.34 MAX_GLIBC=2.34
export VCPKG_BINARY_SOURCES=clear VCPKG_MAX_CONCURRENCY="$BUILD_JOBS" CMAKE_BUILD_PARALLEL_LEVEL="$BUILD_JOBS"
export VCPKG_DOWNLOADS="$SELF_DIR/tools/vcpkg/downloads" VCPKG_DISABLE_METRICS=1
export MAKEFLAGS="-j$BUILD_JOBS" NINJAFLAGS="-j$BUILD_JOBS" OMP_NUM_THREADS="$BUILD_JOBS" OMP_THREAD_LIMIT="$BUILD_JOBS"
export SYSROOT DIST_ROOT ROC_NATIVE_LOG_DIR="$LOG_DIR"
export VCPKG_ROOT
mkdir -p "$LOG_DIR"

build_frontend() {
  ROC_BUILD_ROOT="$BUILD_ROOT/frontend" ROC_BUILD_JOBS="$BUILD_JOBS" ROC_CLEAN_BUILD=0 \
    "$SELF_DIR/build_low_glibc.sh"
  "$SELF_DIR/verify_glibc_compat.sh" "$DIST_ROOT/ROCgalgame/rocgalgame_sdl"
}
build_ons() {
  ONS_ROOT="${ONS_ROOT:-/sources/ons}" ONS_BUILD_DIR="$BUILD_ROOT/onsyuri" \
  ONS_BUILD_JOBS="$BUILD_JOBS" ONS_FORCE_REBUILD=0 ONS_CLEAN_BUILD=0 \
    "$SELF_DIR/build_onsyuri.sh"
  "$SELF_DIR/verify_glibc_compat.sh" "$DIST_ROOT/ROCgalgame/cores/ons/onsyuri"
}
build_krkrsdl2() {
  KRKR_ROOT="${KRKR_ROOT:-/sources/krkrsdl2}" \
  KRKR_FFMPEG_INCLUDE_DIR="${KRKR_FFMPEG_INCLUDE_DIR:-/sources/ffmpeg}" \
  KRKR_BUILD_DIR="$BUILD_ROOT/krkrsdl2" KRKR_BUILD_JOBS="$BUILD_JOBS" KRKR_BUILD_MODE=Fast \
  KRKR_USE_CCACHE=Off KRKR_CONFIRM_HEAVY_BUILD=1 \
    "$SELF_DIR/build_krkr.sh"
  "$SELF_DIR/verify_glibc_compat.sh" "$DIST_ROOT/ROCgalgame/cores/krkr/krkrsdl2"
}
build_krkr2() {
  test -x "$VCPKG_ROOT/vcpkg" || {
    echo "[incremental] ERROR: isolated vcpkg cache is missing; run full baseline build first"; exit 1;
  }
  local krkr2_mode
  if [ -f "$BUILD_ROOT/krkr2/CMakeCache.txt" ] && [ -f "$BUILD_ROOT/krkr2/Makefile" ]; then
    krkr2_mode=FastBuild
  else
    krkr2_mode=Build
  fi
  KRKR2_ROOT="${KRKR2_ROOT:-/sources/krkr2}" \
  KRKR2_BUILD_DIR="$BUILD_ROOT/krkr2" KRKR2_PROBE_BUILD_DIR="$BUILD_ROOT/krkr2-toolchain-probe" \
  KRKR2_TARGET_TRIPLET=arm64-linux-gkd-glibc234 KRKR2_BUILD_MODE="$krkr2_mode" \
  KRKR2_BUILD_JOBS="$BUILD_JOBS" KRKR2_CONFIRM_HEAVY_BUILD=1 KRKR2_SAFE_CPU_SET="$SAFE_CPU_SET" \
  KRKR2_PERIODIC_COOLING=1 KRKR2_WORK_SECONDS="${KRKR2_WORK_SECONDS:-180}" \
  KRKR2_COOL_SECONDS="${KRKR2_COOL_SECONDS:-90}" \
    "$SELF_DIR/build_krkr2.sh"
  "$SELF_DIR/verify_glibc_compat.sh" "$DIST_ROOT/ROCgalgame/cores/krkr/krkr2"
}

case "$TARGET" in
  Frontend) build_frontend ;; ONS) build_ons ;; KRKRSDL2) build_krkrsdl2 ;;
  KRKR2) build_krkr2 ;; All) build_frontend; build_ons; build_krkrsdl2; build_krkr2 ;;
esac
DIST_ROOT="$DIST_ROOT" "$SELF_DIR/sync_runtime_assets.sh"
DIST_ROOT="$DIST_ROOT" SYSROOT="$SYSROOT" "$SELF_DIR/verify_glibc_compat.sh"
echo "[incremental] completed: $TARGET"
