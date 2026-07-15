#!/bin/sh
set -eu

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SELF_DIR/.." && pwd)"
ONS_ROOT="${ONS_ROOT:-/mnt/d/Works/Tyranor/OnscripterYuri}"
SYSROOT="${SYSROOT:-$SELF_DIR/sysroot_device}"
TOOL_PREFIX="${CROSS_TOOL_PREFIX:-aarch64-linux-gnu}"
CXX_CMD="${CROSS_CXX:-${TOOL_PREFIX}-g++}"
DIST_ROOT="${DIST_ROOT:-$SELF_DIR/dist_lowglibc}"
OUT_DIR="$REPO_ROOT/build/gkd350h/onsyuri"
OBJ_DIR="$OUT_DIR/obj"
INCLUDE_OVERLAY="$OUT_DIR/include_overlay"
TARGET="$OUT_DIR/onsyuri"
RUNTIME_CORE_DIR="$DIST_ROOT/ROCgalgame/cores/ons"
LOG_DIR="${ROC_NATIVE_LOG_DIR:-$SELF_DIR/logs}"
LOG_FILE="$LOG_DIR/build_onsyuri_$(date +%Y%m%d_%H%M%S).log"

if [ -x "$SELF_DIR/toolchain/bin/${TOOL_PREFIX}-g++" ] && [ -z "${CROSS_CXX:-}" ]; then
  CXX_CMD="$SELF_DIR/toolchain/bin/${TOOL_PREFIX}-g++"
fi

if [ ! -d "$ONS_ROOT/src/onsyuri" ]; then
  echo "[ons_build] ERROR: invalid ONS_ROOT: $ONS_ROOT"
  exit 1
fi
if [ ! -d "$SYSROOT/usr/include/SDL2" ] || [ ! -d "$SYSROOT/usr/lib" ]; then
  echo "[ons_build] ERROR: invalid sysroot: $SYSROOT"
  exit 1
fi

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

LIBDIR="$(find_libdir)"
mkdir -p "$OBJ_DIR" "$RUNTIME_CORE_DIR" "$LOG_DIR"
LUA_DIR="$ONS_ROOT/src/onsyuri_libretro/deps/lua"
LUA_LIB="$LUA_DIR/liblua.a"

SRCS="
AnimationInfo.cpp
coding2utf16.cpp
DirectReader.cpp
DirtyRect.cpp
FontInfo.cpp
gbk2utf16.cpp
LUAHandler.cpp
NsaReader.cpp
ONScripter.cpp
ONScripter_animation.cpp
ONScripter_command.cpp
ONScripter_effect.cpp
ONScripter_effect_breakup.cpp
ONScripter_event.cpp
ONScripter_file.cpp
ONScripter_file2.cpp
ONScripter_image.cpp
ONScripter_lut.cpp
onscripter_main.cpp
ONScripter_rmenu.cpp
ONScripter_sound.cpp
ONScripter_text.cpp
Parallel.cpp
resize_image.cpp
SarReader.cpp
ScriptHandler.cpp
ScriptParser.cpp
ScriptParser_command.cpp
sjis2utf16.cpp
renderer/gles_renderer.cpp
builtin_dll/layer_oldmovie.cpp
builtin_dll/layer_snow.cpp
builtin_dll/ONScripter_effect_cascade.cpp
builtin_dll/ONScripter_effect_trig.cpp
"

CXXFLAGS="-O2 -std=c++11 -Wall -Wno-deprecated-declarations -Wno-unused-result -Wno-write-strings --sysroot=$SYSROOT"
CXXFLAGS="$CXXFLAGS -DLINUX -DUSE_PARALLEL -DUSE_GLES -DUSE_FILELOG -DUSE_LUA -DUSE_BUILTIN_LAYER_EFFECTS -DENABLE_1BYTE_CHAR"
CXXFLAGS="$CXXFLAGS -I$INCLUDE_OVERLAY -I$ONS_ROOT/src/onsyuri -I$ONS_ROOT/src/onsyuri_libretro/deps -I$ONS_ROOT/src/onsyuri_libretro/deps/SDL_mixer/include -I$SYSROOT/usr/include -I$SYSROOT/usr/include/SDL2 -I/usr/include -I/usr/include/SDL2"
LDFLAGS="--sysroot=$SYSROOT -L$LIBDIR -Wl,-rpath,\$ORIGIN/../../lib -Wl,-rpath,\$ORIGIN/../../lib_system_sdl"
LIBS="-lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer -ljpeg -lbz2 -ldl -lpthread -lm"

{
  echo "[ons_build] ons_root=$ONS_ROOT"
  echo "[ons_build] sysroot=$SYSROOT"
  echo "[ons_build] cxx=$CXX_CMD"
  echo "[ons_build] libdir=$LIBDIR"
  if [ ! -f "$LUA_DIR/lua.h" ]; then
    echo "[ons_build] ERROR: missing Lua headers. Run: git -C '$ONS_ROOT' submodule update --init src/onsyuri_libretro/deps/lua"
    exit 1
  fi
  echo "[ons_build] build static Lua"
  make -C "$LUA_DIR" clean >/dev/null 2>&1 || true
  make -C "$LUA_DIR" liblua.a \
    CC="${TOOL_PREFIX}-gcc --sysroot=$SYSROOT" \
    AR="${TOOL_PREFIX}-ar rc" \
    RANLIB="${TOOL_PREFIX}-ranlib" \
    MYCFLAGS="-std=c99 -DLUA_USE_LINUX -DLUA_COMPAT_5_2" \
    MYLIBS="" \
    -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"
  rm -rf "$OBJ_DIR" "$INCLUDE_OVERLAY" "$TARGET"
  mkdir -p "$OBJ_DIR" "$INCLUDE_OVERLAY/SDL2"
  cp "$ONS_ROOT/src/onsyuri_libretro/deps/SDL_mixer/include/SDL_mixer.h" "$INCLUDE_OVERLAY/SDL2/SDL_mixer.h"
  cp "$SYSROOT/usr/include/SDL2/begin_code.h" "$INCLUDE_OVERLAY/SDL2/begin_code.h"
  cp "$SYSROOT/usr/include/SDL2/close_code.h" "$INCLUDE_OVERLAY/SDL2/close_code.h"
  OBJS=""
  for src in $SRCS; do
    obj="$OBJ_DIR/${src%.cpp}.o"
    mkdir -p "$(dirname "$obj")"
    echo "[ons_build] compile $src"
    "$CXX_CMD" $CXXFLAGS -c "$ONS_ROOT/src/onsyuri/$src" -o "$obj"
    OBJS="$OBJS $obj"
  done
  echo "[ons_build] link $TARGET"
  "$CXX_CMD" $OBJS "$LUA_LIB" -o "$TARGET" $LDFLAGS $LIBS
  cp "$TARGET" "$RUNTIME_CORE_DIR/onsyuri"
  chmod +x "$RUNTIME_CORE_DIR/onsyuri" 2>/dev/null || true
  echo "[ons_build] installed: $RUNTIME_CORE_DIR/onsyuri"
} 2>&1 | tee "$LOG_FILE"

echo "[ons_build] log: $LOG_FILE"
