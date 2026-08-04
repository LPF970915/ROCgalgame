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
BUILD_JOBS="${GLIBC234_BUILD_JOBS:-3}"
SAFE_CPU_SET="${GLIBC234_SAFE_CPU_SET:-0-2}"
CHECK_ONLY="${GLIBC234_CHECK_ONLY:-0}"

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
case "$CHECK_ONLY" in 0|1) ;; *)
  echo "[incremental] ERROR: GLIBC234_CHECK_ONLY must be 0 or 1"; exit 2 ;;
esac
test -f "$SYSROOT/rocgalgame_glibc234_baseline.txt" || {
  echo "[incremental] ERROR: run the full glibc 2.34 baseline build first"; exit 1;
}

export ROCGALGAME_GLIBC_BASELINE=2.34 MAX_GLIBC=2.34
VCPKG_BINARY_CACHE="${VCPKG_BINARY_CACHE:-$VCPKG_ROOT/binary-cache}"
mkdir -p "$VCPKG_BINARY_CACHE"
export VCPKG_BINARY_SOURCES="${VCPKG_BINARY_SOURCES:-clear;files,$VCPKG_BINARY_CACHE,readwrite}"
export VCPKG_MAX_CONCURRENCY="$BUILD_JOBS" CMAKE_BUILD_PARALLEL_LEVEL="$BUILD_JOBS"
export VCPKG_DOWNLOADS="$VCPKG_ROOT/downloads" VCPKG_DISABLE_METRICS=1
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
  KRKR_USE_CCACHE=Auto KRKR_CONFIRM_HEAVY_BUILD=1 \
    "$SELF_DIR/build_krkr.sh"
  "$SELF_DIR/verify_glibc_compat.sh" "$DIST_ROOT/ROCgalgame/cores/krkr/krkrsdl2"
}
build_krkr2() {
  local krkr2_build="$BUILD_ROOT/krkr2"
  local krkr2_root="${KRKR2_ROOT:-/sources/krkr2}"
  local link_file="$krkr2_build/CMakeFiles/krkr2.dir/link.txt"
  local required dependency missing=0
  for required in \
    "$VCPKG_ROOT/vcpkg" \
    "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
    "$krkr2_build/CMakeCache.txt" \
    "$krkr2_build/Makefile" \
    "$link_file" \
    "$krkr2_build/vcpkg_installed/vcpkg/status" \
    "$krkr2_build/vcpkg_installed/arm64-linux-gkd-glibc234" \
    "$krkr2_root/CMakeLists.txt" \
    "$SELF_DIR/toolchain/aarch64-gkd-krkr2.cmake" \
    "$SELF_DIR/vcpkg-triplets/arm64-linux-gkd-glibc234.cmake"; do
    if [ ! -e "$required" ]; then
      echo "[incremental] ERROR: KRKR2 fast-build dependency is missing: $required"
      missing=1
    fi
  done
  [ "$missing" -eq 0 ] || {
    echo "[incremental] Refusing to configure or rebuild the dependency graph automatically."
    exit 1
  }
  bash "$SELF_DIR/verify_source_provenance.sh" "$krkr2_root" krkr2
  grep -Fq 'VCPKG_TARGET_TRIPLET:STRING=arm64-linux-gkd-glibc234' \
    "$krkr2_build/CMakeCache.txt" || {
      echo "[incremental] ERROR: cached KRKR2 triplet is not the glibc 2.34 baseline"; exit 1;
    }
  cached_build_dir="$(sed -n 's/^CMAKE_CACHEFILE_DIR:INTERNAL=//p' "$krkr2_build/CMakeCache.txt" | head -n 1)"
  cached_source_dir="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$krkr2_build/CMakeCache.txt" | head -n 1)"
  if [ "$cached_build_dir" != "$krkr2_build" ]; then
    echo "[incremental] ERROR: KRKR2 CMake cache belongs to $cached_build_dir, expected $krkr2_build"
    echo "[incremental] Configure in the fixed container workspace before using FastBuild."
    exit 1
  fi
  if [ "$cached_source_dir" != "$krkr2_root" ]; then
    echo "[incremental] ERROR: KRKR2 CMake cache source is $cached_source_dir, expected $krkr2_root"
    exit 1
  fi
  while IFS= read -r dependency; do
    case "$dependency" in
      */compat/fmod_stub/libfmod_stub.a|*/compat/fmod_stub/glibc234_math_abi.o)
        continue
        ;;
      /*) ;;
      *) dependency="$krkr2_build/$dependency" ;;
    esac
    if [ ! -f "$dependency" ]; then
      echo "[incremental] ERROR: cached KRKR2 link dependency is missing: $dependency"
      missing=1
    fi
  done < <(tr ' ' '\n' < "$link_file" | grep -E '\.(a|o|so)(\.[0-9.]+)?$' | sort -u)
  [ "$missing" -eq 0 ] || {
    echo "[incremental] Refusing to start FastBuild with an incomplete link cache."
    exit 1
  }
  echo "[incremental] KRKR2 glibc 2.34 fast-build cache is complete"
  if [ "$CHECK_ONLY" = "1" ]; then
    echo "[incremental] check-only requested; no compile or link command was run"
    return
  fi
  local krkr2_mode
  krkr2_mode=FastBuild
  KRKR2_ROOT="$krkr2_root" \
  KRKR2_BUILD_DIR="$BUILD_ROOT/krkr2" KRKR2_PROBE_BUILD_DIR="$BUILD_ROOT/krkr2-toolchain-probe" \
  KRKR2_TARGET_TRIPLET=arm64-linux-gkd-glibc234 KRKR2_BUILD_MODE="$krkr2_mode" \
  KRKR2_BUILD_JOBS="$BUILD_JOBS" KRKR2_CONFIRM_HEAVY_BUILD=1 KRKR2_SAFE_CPU_SET="$SAFE_CPU_SET" \
  KRKR2_PERIODIC_COOLING=0 KRKR2_WORK_SECONDS="${KRKR2_WORK_SECONDS:-300}" \
  KRKR2_COOL_SECONDS="${KRKR2_COOL_SECONDS:-240}" \
    "$SELF_DIR/build_krkr2.sh"
  "$SELF_DIR/verify_glibc_compat.sh" "$DIST_ROOT/ROCgalgame/cores/krkr/krkr2"
}

if [ "$CHECK_ONLY" = "1" ] && [ "$TARGET" != "KRKR2" ]; then
  echo "[incremental] ERROR: check-only currently supports Target=KRKR2"; exit 2
fi

case "$TARGET" in
  Frontend) build_frontend ;; ONS) build_ons ;; KRKRSDL2) build_krkrsdl2 ;;
  KRKR2) build_krkr2 ;; All) build_frontend; build_ons; build_krkrsdl2; build_krkr2 ;;
esac
if [ "$CHECK_ONLY" = "0" ]; then
  DIST_ROOT="$DIST_ROOT" "$SELF_DIR/sync_runtime_assets.sh"
  DIST_ROOT="$DIST_ROOT" SYSROOT="$SYSROOT" "$SELF_DIR/verify_glibc_compat.sh"
fi
echo "[incremental] completed: $TARGET"
