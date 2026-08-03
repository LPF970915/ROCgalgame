#!/bin/bash
set -euo pipefail

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SELF_DIR/.." && pwd)"

SYSROOT="${SYSROOT:-$SELF_DIR/sysroot_device}"
TOOL_PREFIX="${CROSS_TOOL_PREFIX:-aarch64-linux-gnu}"
CXX_CMD="${CROSS_CXX:-${TOOL_PREFIX}-g++}"
PKG_CMD="${CROSS_PKG_CONFIG:-pkg-config}"
DIST_ROOT="${DIST_ROOT:-$SELF_DIR/dist_lowglibc}"
RUNTIME_DIR="$DIST_ROOT/ROCgalgame"
LOG_DIR="${ROC_NATIVE_LOG_DIR:-$SELF_DIR/logs}"
LOG_FILE="$LOG_DIR/build_$(date +%Y%m%d_%H%M%S).log"
BUILD_JOBS="${ROC_BUILD_JOBS:-1}"
CLEAN_BUILD="${ROC_CLEAN_BUILD:-0}"
BUILD_ROOT="${ROC_BUILD_ROOT:-$REPO_ROOT/build/gkd350h}"
TARGET="$BUILD_ROOT/rocgalgame_sdl"
OBJ_DIR="$BUILD_ROOT/obj"

mkdir -p "$LOG_DIR" "$RUNTIME_DIR"

if [ ! -d "$SYSROOT/usr/include" ] || [ ! -d "$SYSROOT/usr/lib" ]; then
  echo "[gkd_build] ERROR: invalid sysroot: $SYSROOT"
  echo "[gkd_build] Run DEVICE_HOST=root@<ip> $SELF_DIR/sync_sysroot.sh first."
  exit 1
fi

if [ -x "$SELF_DIR/toolchain/bin/${TOOL_PREFIX}-g++" ] && [ -z "${CROSS_CXX:-}" ]; then
  CXX_CMD="$SELF_DIR/toolchain/bin/${TOOL_PREFIX}-g++"
fi

find_pkg_dirs() {
  for d in \
    "$SYSROOT/usr/lib/aarch64-linux-gnu/pkgconfig" \
    "$SYSROOT/usr/lib/pkgconfig" \
    "$SYSROOT/lib/aarch64-linux-gnu/pkgconfig" \
    "$SYSROOT/lib/pkgconfig" \
    "$SYSROOT/usr/share/pkgconfig"; do
    [ -d "$d" ] && printf "%s:" "$d"
  done
}

find_libdir() {
  for d in \
    "$SYSROOT/usr/lib/aarch64-linux-gnu" \
    "$SYSROOT/lib/aarch64-linux-gnu" \
    "$SYSROOT/usr/lib" \
    "$SYSROOT/lib"; do
    [ -d "$d" ] && { printf "%s" "$d"; return 0; }
  done
  return 1
}

PKG_LIBDIR="$(find_pkg_dirs || true)"
PKG_LIBDIR="${PKG_LIBDIR%:}"
LIBDIR="$(find_libdir)"

export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"
export PKG_CONFIG_LIBDIR="$PKG_LIBDIR"
export PKG_CONFIG_PATH=""

cd "$REPO_ROOT"
if [ "$CLEAN_BUILD" = "1" ]; then
  make TARGET="$TARGET" OBJDIR="$OBJ_DIR" clean >/dev/null 2>&1 || true
fi

# Docker and WSL share this object cache but mount the repository at different
# absolute paths. Normalize generated dependency paths so switching builders
# does not invalidate otherwise current objects.
for dep_file in "$OBJ_DIR"/*.d; do
  [ -f "$dep_file" ] || continue
  if grep -Fq '/workspace/' "$dep_file" || {
       [ "$REPO_ROOT" != "/mnt/d/Works/ROCgalgame" ] &&
       grep -Fq '/mnt/d/Works/ROCgalgame/' "$dep_file"
     }; then
    sed -i \
      -e "s#/workspace/#$REPO_ROOT/#g" \
      -e "s#/mnt/d/Works/ROCgalgame/#$REPO_ROOT/#g" \
      "$dep_file"
  fi
done
{
  echo "[gkd_build] repo=$REPO_ROOT"
  echo "[gkd_build] sysroot=$SYSROOT"
  echo "[gkd_build] cxx=$CXX_CMD"
  echo "[gkd_build] pkg_libdir=$PKG_CONFIG_LIBDIR"
  nice -n 15 ionice -c 2 -n 7 make -j"$BUILD_JOBS" \
    TARGET="$TARGET" \
    OBJDIR="$OBJ_DIR" \
    CXX="$CXX_CMD" \
    PKG_CONFIG="$PKG_CMD" \
    IMG_LIBS="-lSDL2_image" \
    TTF_LIBS="-lSDL2_ttf" \
    ALSA_CFLAGS="-I$SYSROOT/usr/include" \
    ALSA_LIBS="-lasound" \
    EXTRA_CXXFLAGS="--sysroot=$SYSROOT -I$SYSROOT/usr/include -I$SYSROOT/usr/include/SDL2 -DROCGALGAME_GKD350H_ULTRA=1" \
    EXTRA_LDFLAGS="--sysroot=$SYSROOT -L$LIBDIR -Wl,-rpath,'\$\$ORIGIN/lib' -Wl,-rpath,'\$\$ORIGIN/lib_system_sdl'" \
    all
} 2>&1 | tee "$LOG_FILE"

mkdir -p "$RUNTIME_DIR"
cp "$TARGET" "$RUNTIME_DIR/rocgalgame_sdl"
bash "$SELF_DIR/sync_runtime_assets.sh"
chmod +x "$RUNTIME_DIR/rocgalgame_sdl" 2>/dev/null || true

echo "[gkd_build] done: $DIST_ROOT"
echo "[gkd_build] log: $LOG_FILE"
