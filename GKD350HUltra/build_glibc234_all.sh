#!/bin/bash
set -euo pipefail

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SELF_DIR/.." && pwd)"
BUILD_ROOT="$REPO_ROOT/build/gkd350h-glibc234"
SYSROOT="$BUILD_ROOT/sysroot"
DIST_ROOT="$SELF_DIR/dist_glibc234"
LOG_DIR="$SELF_DIR/logs/glibc234"
VCPKG_SOURCE_ROOT="$SELF_DIR/tools/vcpkg"
VCPKG_ROOT="$BUILD_ROOT/vcpkg"
RESUME_BUILD="${RESUME_BUILD:-0}"
BUILD_JOBS="${GLIBC234_BUILD_JOBS:-1}"
SAFE_CPU_SET="${GLIBC234_SAFE_CPU_SET:-0}"

case "$BUILD_JOBS" in
  ''|*[!0-9]*) echo "[glibc234] ERROR: GLIBC234_BUILD_JOBS must be an integer from 1 to 4"; exit 2 ;;
esac
if [ "$BUILD_JOBS" -lt 1 ] || [ "$BUILD_JOBS" -gt 4 ]; then
  echo "[glibc234] ERROR: GLIBC234_BUILD_JOBS must remain between 1 and 4"
  exit 2
fi

export ROCGALGAME_GLIBC_BASELINE=2.34
export MAX_GLIBC=2.34
VCPKG_BINARY_CACHE="${VCPKG_BINARY_CACHE:-$VCPKG_ROOT/binary-cache}"
mkdir -p "$VCPKG_BINARY_CACHE"
export VCPKG_BINARY_SOURCES="${VCPKG_BINARY_SOURCES:-clear;files,$VCPKG_BINARY_CACHE,readwrite}"
export VCPKG_DOWNLOADS="$VCPKG_SOURCE_ROOT/downloads" VCPKG_DISABLE_METRICS=1
export VCPKG_MAX_CONCURRENCY="$BUILD_JOBS" CMAKE_BUILD_PARALLEL_LEVEL="$BUILD_JOBS" MAKEFLAGS="-j$BUILD_JOBS" NINJAFLAGS="-j$BUILD_JOBS"
export OMP_NUM_THREADS="$BUILD_JOBS" OMP_THREAD_LIMIT="$BUILD_JOBS" OPENBLAS_NUM_THREADS="$BUILD_JOBS" MKL_NUM_THREADS="$BUILD_JOBS" NUMEXPR_NUM_THREADS="$BUILD_JOBS"
export SYSROOT DIST_ROOT ROC_NATIVE_LOG_DIR="$LOG_DIR"
export VCPKG_ROOT
mkdir -p "$LOG_DIR"

prepare_isolated_vcpkg() {
  [ -x "$VCPKG_SOURCE_ROOT/vcpkg" ] || {
    echo "[glibc234] ERROR: missing vcpkg source root: $VCPKG_SOURCE_ROOT"; exit 1;
  }
  if [ ! -x "$VCPKG_ROOT/vcpkg" ]; then
    echo "[glibc234] initialize isolated vcpkg root: $VCPKG_ROOT"
    mkdir -p "$VCPKG_ROOT"
    rsync -a --delete \
      --exclude='/buildtrees/' --exclude='/packages/' --exclude='/downloads/' \
      --exclude='/binary-cache/' \
      "$VCPKG_SOURCE_ROOT/" "$VCPKG_ROOT/"
  fi
}

case "$DIST_ROOT" in "$SELF_DIR/dist_glibc234") ;; *) exit 2 ;; esac
case "$RESUME_BUILD" in 0|1) ;; *) echo "[glibc234] ERROR: RESUME_BUILD must be 0 or 1"; exit 2 ;; esac
if [ -f "$SYSROOT/rocgalgame_glibc234_baseline.txt" ]; then
  REUSE_GLIBC234_SYSROOT=1
else
  REUSE_GLIBC234_SYSROOT=0
fi
if [ "$RESUME_BUILD" = "0" ]; then
  rm -rf "$DIST_ROOT"
fi
BASE_SYSROOT="${BASE_SYSROOT:-/sources/h700-sysroot}" \
  DEVICE_SYSROOT="${DEVICE_SYSROOT:-$SELF_DIR/sysroot_device}" \
  REUSE_BASELINE="${REUSE_GLIBC234_SYSROOT:-0}" \
  OUTPUT_SYSROOT="$SYSROOT" "$SELF_DIR/prepare_glibc234_sysroot.sh"

echo "[glibc234] stage 1/4: frontend (resume=$RESUME_BUILD)"
ROC_BUILD_ROOT="$BUILD_ROOT/frontend" ROC_BUILD_JOBS="$BUILD_JOBS" ROC_CLEAN_BUILD="$((1 - RESUME_BUILD))" \
  "$SELF_DIR/build_low_glibc.sh"
"$SELF_DIR/verify_glibc_compat.sh" "$DIST_ROOT/ROCgalgame/rocgalgame_sdl"

echo "[glibc234] stage 2/4: ONScripterYuri (resume=$RESUME_BUILD)"
ONS_ROOT="${ONS_ROOT:-/sources/ons}" ONS_BUILD_DIR="$BUILD_ROOT/onsyuri" \
ONS_BUILD_JOBS="$BUILD_JOBS" ONS_FORCE_REBUILD="$((1 - RESUME_BUILD))" ONS_CLEAN_BUILD="$((1 - RESUME_BUILD))" \
  "$SELF_DIR/build_onsyuri.sh"
"$SELF_DIR/verify_glibc_compat.sh" "$DIST_ROOT/ROCgalgame/cores/ons/onsyuri"

if [ "$RESUME_BUILD" = "1" ] && [ -f "$BUILD_ROOT/krkrsdl2/CMakeCache.txt" ]; then
  KRKRSDL2_MODE=Fast
else
  KRKRSDL2_MODE=Full
fi
echo "[glibc234] stage 3/4: KRKRSDL2 (mode=$KRKRSDL2_MODE)"
KRKR_ROOT="${KRKR_ROOT:-/sources/krkrsdl2}" \
KRKR_FFMPEG_INCLUDE_DIR="${KRKR_FFMPEG_INCLUDE_DIR:-/sources/ffmpeg}" \
KRKR_BUILD_DIR="$BUILD_ROOT/krkrsdl2" KRKR_BUILD_JOBS="$BUILD_JOBS" KRKR_BUILD_MODE="$KRKRSDL2_MODE" \
KRKR_USE_CCACHE=Auto KRKR_CONFIRM_HEAVY_BUILD=1 \
  "$SELF_DIR/build_krkr.sh"
"$SELF_DIR/verify_glibc_compat.sh" "$DIST_ROOT/ROCgalgame/cores/krkr/krkrsdl2"

if [ "$RESUME_BUILD" = "1" ] && [ -f "$BUILD_ROOT/krkr2/CMakeCache.txt" ] && \
   [ -f "$BUILD_ROOT/krkr2/Makefile" ]; then
  KRKR2_MODE=FastBuild
else
  KRKR2_MODE=Full
fi
echo "[glibc234] stage 4/4: KRKR2 (mode=$KRKR2_MODE) with periodic cooling"
prepare_isolated_vcpkg
KRKR2_ROOT="${KRKR2_ROOT:-/sources/krkr2}" \
KRKR2_BUILD_DIR="$BUILD_ROOT/krkr2" KRKR2_PROBE_BUILD_DIR="$BUILD_ROOT/krkr2-toolchain-probe" \
KRKR2_TARGET_TRIPLET=arm64-linux-gkd-glibc234 KRKR2_BUILD_MODE="$KRKR2_MODE" \
KRKR2_BUILD_JOBS="$BUILD_JOBS" KRKR2_CONFIRM_HEAVY_BUILD=1 KRKR2_SAFE_CPU_SET="$SAFE_CPU_SET" \
KRKR2_PERIODIC_COOLING=1 KRKR2_WORK_SECONDS="${KRKR2_WORK_SECONDS:-180}" \
KRKR2_COOL_SECONDS="${KRKR2_COOL_SECONDS:-90}" \
  "$SELF_DIR/build_krkr2.sh"
"$SELF_DIR/verify_glibc_compat.sh" "$DIST_ROOT/ROCgalgame/cores/krkr/krkr2"

DIST_ROOT="$DIST_ROOT" "$SELF_DIR/sync_runtime_assets.sh"
DIST_ROOT="$DIST_ROOT" SYSROOT="$SYSROOT" "$SELF_DIR/verify_glibc_compat.sh"
DIST_ROOT="$DIST_ROOT" SYSROOT="$SYSROOT" "$SELF_DIR/validate_runtime_deps.sh"
echo "[glibc234] all isolated builds and static gates passed: $DIST_ROOT"
