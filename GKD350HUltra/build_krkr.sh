#!/bin/bash
set -euo pipefail

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SELF_DIR/.." && pwd)"
KRKR_ROOT="${KRKR_ROOT:-/mnt/d/Works/Tyranor/krkrsdl2}"
SYSROOT="${SYSROOT:-$SELF_DIR/sysroot_device}"
BUILD_DIR="${KRKR_BUILD_DIR:-$REPO_ROOT/build/gkd350h/krkrsdl2}"
DIST_ROOT="${DIST_ROOT:-$SELF_DIR/dist_lowglibc}"
RUNTIME_CORE_DIR="$DIST_ROOT/ROCgalgame/cores/krkr"
LOG_DIR="${ROC_NATIVE_LOG_DIR:-$SELF_DIR/logs}"
LOG_FILE="$LOG_DIR/build_krkr_$(date +%Y%m%d_%H%M%S).log"
TOOLCHAIN="$SELF_DIR/toolchain/aarch64-gkd.cmake"
BUILD_JOBS="${KRKR_BUILD_JOBS:-1}"
CMAKE_BIN="${CMAKE_BIN:-}"
BUILD_MODE="${KRKR_BUILD_MODE:-Fast}"
USE_CCACHE="${KRKR_USE_CCACHE:-Auto}"

# Compatibility with older wrappers. New callers should use KRKR_BUILD_MODE.
if [ "${KRKR_CLEAN_BUILD:-0}" = "1" ]; then
  BUILD_MODE="Full"
elif [ "${KRKR_FORCE_CONFIGURE:-0}" = "1" ] && [ "$BUILD_MODE" = "Fast" ]; then
  BUILD_MODE="Configure"
fi

case "$BUILD_MODE" in
  Fast|Configure|Full) ;;
  *) echo "[krkr_build] ERROR: KRKR_BUILD_MODE must be Fast, Configure, or Full"; exit 2 ;;
esac
case "$USE_CCACHE" in
  Auto|On|Off) ;;
  *) echo "[krkr_build] ERROR: KRKR_USE_CCACHE must be Auto, On, or Off"; exit 2 ;;
esac

if [ "${KRKR_CONFIRM_HEAVY_BUILD:-0}" != "1" ]; then
  echo "[krkr_build] REFUSED: KRKR compilation previously coincided with unexpected Windows power-offs."
  echo "[krkr_build] Check cooling/power first, then set KRKR_CONFIRM_HEAVY_BUILD=1 explicitly."
  exit 3
fi

if [ -z "$CMAKE_BIN" ]; then
  if command -v cmake >/dev/null 2>&1; then
    CMAKE_BIN="$(command -v cmake)"
  elif [ -x "$SELF_DIR/tools/cmake/bin/cmake" ]; then
    CMAKE_BIN="$SELF_DIR/tools/cmake/bin/cmake"
  else
    echo "[krkr_build] ERROR: cmake is required in WSL or at $SELF_DIR/tools/cmake/bin/cmake"
    exit 1
  fi
fi
command -v aarch64-linux-gnu-g++ >/dev/null 2>&1 || { echo "[krkr_build] ERROR: aarch64-linux-gnu-g++ is required"; exit 1; }
test -f "$KRKR_ROOT/CMakeLists.txt" || { echo "[krkr_build] ERROR: invalid KRKR_ROOT: $KRKR_ROOT"; exit 1; }
grep -q 'OPTION_USE_SYSTEM_SDL2' "$KRKR_ROOT/CMakeLists.txt" || {
  echo "[krkr_build] ERROR: KRKR source patch missing. See $SELF_DIR/patches/README.md"
  exit 1
}
grep -q 'configured.length() > 4' "$KRKR_ROOT/external/krkrz/visual/FontSystem.cpp" || {
  echo "[krkr_build] ERROR: font patch missing. See $SELF_DIR/patches/README.md"
  exit 1
}
grep -q 'TVPAutoMountProjectXP3Archives' "$KRKR_ROOT/external/krkrz/base/StorageIntf.cpp" || {
  echo "[krkr_build] ERROR: XP3 auto-mount patch missing. See $SELF_DIR/patches/README.md"
  exit 1
}
grep -q 'return TVPProjectDir' "$KRKR_ROOT/src/core/base/sdl2/StorageImpl.cpp" || {
  echo "[krkr_build] ERROR: project app-path patch missing. See $SELF_DIR/patches/README.md"
  exit 1
}
if [ ! -d "$SYSROOT/usr/include" ] || [ ! -d "$SYSROOT/usr/lib" ]; then
  echo "[krkr_build] ERROR: invalid sysroot: $SYSROOT"
  echo "[krkr_build] Restore SSH, then run: DEVICE_HOST=root@<ip> $SELF_DIR/sync_sysroot.sh"
  exit 1
fi
WEBP_INCLUDE_DIR="$SYSROOT/usr/include"
WEBP_LIBRARY="${KRKR_WEBP_LIBRARY:-}"
if [ ! -f "$WEBP_INCLUDE_DIR/webp/decode.h" ]; then
  echo "[krkr_build] ERROR: WebP headers are missing from the sysroot."
  echo "[krkr_build] Run $SELF_DIR/prepare_headers_overlay.sh first."
  exit 1
fi
if [ -z "$WEBP_LIBRARY" ]; then
  for candidate in \
    "$SYSROOT/usr/lib/libwebp.so" \
    "$SYSROOT/usr/lib/aarch64-linux-gnu/libwebp.so" \
    "$SYSROOT/lib/aarch64-linux-gnu/libwebp.so" \
    "$SYSROOT/lib/compat/compat/libwebp.so.6" \
    "$SYSROOT/usr/lib/compat/compat/libwebp.so.6"; do
    if [ -f "$candidate" ]; then
      WEBP_LIBRARY="$candidate"
      break
    fi
  done
fi
if [ -z "$WEBP_LIBRARY" ] || [ ! -f "$WEBP_LIBRARY" ]; then
  echo "[krkr_build] ERROR: no ARM64 libwebp was found in the sysroot."
  echo "[krkr_build] Sync it from the device/PortsMaster or set KRKR_WEBP_LIBRARY explicitly."
  exit 1
fi
WEBP_LINK_LIBRARY="$SYSROOT/usr/lib/$(basename "$WEBP_LIBRARY")"
if [ "$WEBP_LIBRARY" != "$WEBP_LINK_LIBRARY" ]; then
  cp -L "$WEBP_LIBRARY" "$WEBP_LINK_LIBRARY"
fi
WEBP_LIBRARY="$WEBP_LINK_LIBRARY"

FFMPEG_INCLUDE_DIR="${KRKR_FFMPEG_INCLUDE_DIR:-/mnt/d/Works/Tyranor/FFmpeg-n6.0}"
FFMPEG_CONFIG_INCLUDE_DIR="${KRKR_FFMPEG_CONFIG_INCLUDE_DIR:-$SELF_DIR/ffmpeg_headers_overlay}"
if [ ! -f "$FFMPEG_INCLUDE_DIR/libavcodec/avcodec.h" ] || \
   [ ! -f "$FFMPEG_INCLUDE_DIR/libavformat/avformat.h" ] || \
   [ ! -f "$FFMPEG_CONFIG_INCLUDE_DIR/libavutil/avconfig.h" ]; then
  echo "[krkr_build] ERROR: FFmpeg 6 headers are missing."
  echo "[krkr_build] Expected source headers at $FFMPEG_INCLUDE_DIR and avconfig.h at $FFMPEG_CONFIG_INCLUDE_DIR"
  exit 1
fi

find_ffmpeg_library() {
  name="$1"
  for candidate in \
    "$SYSROOT/usr/lib/lib${name}.so" \
    "$SYSROOT/usr/lib/aarch64-linux-gnu/lib${name}.so" \
    "$SYSROOT/lib/aarch64-linux-gnu/lib${name}.so"; do
    if [ -f "$candidate" ]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done
  return 1
}

AVCODEC_LIBRARY="$(find_ffmpeg_library avcodec || true)"
AVFORMAT_LIBRARY="$(find_ffmpeg_library avformat || true)"
AVUTIL_LIBRARY="$(find_ffmpeg_library avutil || true)"
SWRESAMPLE_LIBRARY="$(find_ffmpeg_library swresample || true)"
SWSCALE_LIBRARY="$(find_ffmpeg_library swscale || true)"
for ffmpeg_library in "$AVCODEC_LIBRARY" "$AVFORMAT_LIBRARY" "$AVUTIL_LIBRARY" "$SWRESAMPLE_LIBRARY" "$SWSCALE_LIBRARY"; do
  if [ -z "$ffmpeg_library" ] || [ ! -f "$ffmpeg_library" ]; then
    echo "[krkr_build] ERROR: FFmpeg 6 runtime/development symlinks are incomplete in $SYSROOT/usr/lib"
    exit 1
  fi
done

find_pkg_dirs() {
  for d in "$SYSROOT/usr/lib/aarch64-linux-gnu/pkgconfig" "$SYSROOT/usr/lib/pkgconfig" \
           "$SYSROOT/lib/aarch64-linux-gnu/pkgconfig" "$SYSROOT/lib/pkgconfig" "$SYSROOT/usr/share/pkgconfig"; do
    [ -d "$d" ] && printf '%s:' "$d"
  done
}
export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"
export PKG_CONFIG_LIBDIR="$(find_pkg_dirs)"
export PKG_CONFIG_LIBDIR="${PKG_CONFIG_LIBDIR%:}"
export PKG_CONFIG_PATH=""

if [ "$BUILD_MODE" = "Full" ]; then
  case "$BUILD_DIR" in
    ""|"/") echo "[krkr_build] ERROR: unsafe build directory: $BUILD_DIR"; exit 1 ;;
  esac
  echo "[krkr_build] clean build directory: $BUILD_DIR"
  rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR" "$RUNTIME_CORE_DIR" "$LOG_DIR"

CCACHE_ARGS=()
if [ "$USE_CCACHE" != "Off" ]; then
  if command -v ccache >/dev/null 2>&1; then
    CCACHE_BIN="$(command -v ccache)"
    CCACHE_ARGS=(
      "-DCMAKE_C_COMPILER_LAUNCHER=$CCACHE_BIN"
      "-DCMAKE_CXX_COMPILER_LAUNCHER=$CCACHE_BIN"
    )
  elif [ "$USE_CCACHE" = "On" ]; then
    echo "[krkr_build] ERROR: KRKR_USE_CCACHE=On but ccache is unavailable"
    exit 1
  fi
fi

NEED_CONFIGURE=0
if [ "$BUILD_MODE" != "Fast" ] || [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
  NEED_CONFIGURE=1
elif [ "$USE_CCACHE" = "On" ] && [ "${#CCACHE_ARGS[@]}" -gt 0 ] && \
     ! grep -q '^CMAKE_CXX_COMPILER_LAUNCHER:.*ccache' "$BUILD_DIR/CMakeCache.txt"; then
  echo "[krkr_build] ccache was explicitly requested; performing a non-clean configure once"
  NEED_CONFIGURE=1
fi

{
  echo "[krkr_build] root=$KRKR_ROOT"
  echo "[krkr_build] mode=$BUILD_MODE"
  echo "[krkr_build] sysroot=$SYSROOT"
  echo "[krkr_build] pkg_config=$PKG_CONFIG_LIBDIR"
  echo "[krkr_build] cmake=$CMAKE_BIN"
  echo "[krkr_build] webp=$WEBP_LIBRARY"
  echo "[krkr_build] ffmpeg_headers=$FFMPEG_INCLUDE_DIR"
  echo "[krkr_build] avcodec=$AVCODEC_LIBRARY"
  echo "[krkr_build] swscale=$SWSCALE_LIBRARY"
  if [ "${#CCACHE_ARGS[@]}" -gt 0 ]; then
    echo "[krkr_build] ccache=${CCACHE_ARGS[0]#*=}"
  else
    echo "[krkr_build] ccache=disabled/unavailable"
  fi
  if [ "$NEED_CONFIGURE" = "1" ]; then
    echo "[krkr_build] configure build tree"
    nice -n 10 "$CMAKE_BIN" -S "$KRKR_ROOT" -B "$BUILD_DIR" \
      -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
      -DGKD_SYSROOT="$SYSROOT" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_FLAGS_RELEASE="-O2 -DNDEBUG" \
      -DCMAKE_CXX_FLAGS_RELEASE="-O2 -DNDEBUG" \
      -DCMAKE_BUILD_RPATH='$ORIGIN/../../lib_system_sdl;$ORIGIN/../../lib' \
      -DCMAKE_INSTALL_RPATH='$ORIGIN/../../lib_system_sdl;$ORIGIN/../../lib' \
      -DOPTION_USE_SYSTEM_SDL2=ON \
      -DOPTION_ENABLE_CANVAS=OFF \
      -DOPTION_ENABLE_EXTERNAL_PLUGINS=ON \
      -DOPTION_ENABLE_ASYNC_IMAGE_LOAD=ON \
      -DKRKRSDL2_ENABLE_PRECOMPILED_HEADERS=ON \
      -DKRKRSDL2_WEBP_INCLUDE_DIR="$WEBP_INCLUDE_DIR" \
      -DKRKRSDL2_WEBP_LIBRARY="$WEBP_LIBRARY" \
      -DKRKRSDL2_FFMPEG_INCLUDE_DIR="$FFMPEG_INCLUDE_DIR" \
      -DKRKRSDL2_FFMPEG_CONFIG_INCLUDE_DIR="$FFMPEG_CONFIG_INCLUDE_DIR" \
      -DKRKRSDL2_AVCODEC_LIBRARY="$AVCODEC_LIBRARY" \
      -DKRKRSDL2_AVFORMAT_LIBRARY="$AVFORMAT_LIBRARY" \
      -DKRKRSDL2_AVUTIL_LIBRARY="$AVUTIL_LIBRARY" \
      -DKRKRSDL2_SWRESAMPLE_LIBRARY="$SWRESAMPLE_LIBRARY" \
      -DKRKRSDL2_SWSCALE_LIBRARY="$SWSCALE_LIBRARY" \
      "${CCACHE_ARGS[@]}"
  else
    echo "[krkr_build] fast path: reuse configured build tree without manual configure: $BUILD_DIR"
  fi
  echo "[krkr_build] jobs=$BUILD_JOBS (kept conservative after host power-off incidents)"
  nice -n 10 "$CMAKE_BIN" --build "$BUILD_DIR" --config Release --parallel "$BUILD_JOBS"
  cp "$BUILD_DIR/krkrsdl2" "$RUNTIME_CORE_DIR/krkrsdl2"
  chmod +x "$RUNTIME_CORE_DIR/krkrsdl2"
  mkdir -p "$DIST_ROOT/ROCgalgame/lib"
  cp -L "$WEBP_LIBRARY" "$DIST_ROOT/ROCgalgame/lib/$(basename "$WEBP_LIBRARY")"
  echo "[krkr_build] installed=$RUNTIME_CORE_DIR/krkrsdl2"
  readelf -d "$RUNTIME_CORE_DIR/krkrsdl2" | grep -E 'NEEDED|RUNPATH|RPATH' || true
} 2>&1 | tee "$LOG_FILE"
echo "[krkr_build] log=$LOG_FILE"
