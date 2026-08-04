#!/bin/bash
set -euo pipefail

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SELF_DIR/.." && pwd)"
BUILD_ROOT="${ONS_BUILD_ROOT:-$REPO_ROOT/build/gkd350h-glibc234}"
ONS_ROOT="${ONS_ROOT:-/mnt/d/Works/Tyranor/OnscripterYuri}"
SYSROOT="${SYSROOT:-$BUILD_ROOT/sysroot}"
TOOL_PREFIX="${CROSS_TOOL_PREFIX:-aarch64-linux-gnu}"
CXX_CMD="${CROSS_CXX:-${TOOL_PREFIX}-g++}"
DIST_ROOT="${DIST_ROOT:-$SELF_DIR/dist_glibc234}"
OUT_DIR="${ONS_BUILD_DIR:-$BUILD_ROOT/onsyuri}"
OBJ_DIR="$OUT_DIR/obj"
INCLUDE_OVERLAY="$OUT_DIR/include_overlay"
PATCHED_SRC_DIR="$OUT_DIR/patched_src"
FILTER_OVERLAY="$SELF_DIR/ons_filter_overlay"
TARGET="$OUT_DIR/onsyuri"
RUNTIME_CORE_DIR="$DIST_ROOT/ROCgalgame/cores/ons"
LOG_DIR="${ROC_NATIVE_LOG_DIR:-$SELF_DIR/logs}"
LOG_FILE="$LOG_DIR/build_onsyuri_$(date +%Y%m%d_%H%M%S).log"
BUILD_JOBS="${ONS_BUILD_JOBS:-1}"
FORCE_REBUILD="${ONS_FORCE_REBUILD:-0}"
CLEAN_BUILD="${ONS_CLEAN_BUILD:-0}"

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
LUA_SOURCE_DIR="$ONS_ROOT/src/onsyuri_libretro/deps/lua"
LUA_BUILD_DIR="$OUT_DIR/lua"
LUA_LIB="$LUA_BUILD_DIR/liblua.a"

FULL_RECOMPILE=0
NEEDS_REBUILD=0
if [ "$FORCE_REBUILD" = "1" ] || [ "$CLEAN_BUILD" = "1" ]; then
  FULL_RECOMPILE=1
  NEEDS_REBUILD=1
elif [ ! -x "$TARGET" ]; then
  # A previous low-power build may have stopped before linking. Reuse the
  # successfully compiled objects and continue with only the missing sources.
  NEEDS_REBUILD=1
elif [ "$SYSROOT/target_info.txt" -nt "$TARGET" ] || \
     [ "$SELF_DIR/build_onsyuri.sh" -nt "$TARGET" ] || \
     [ "$SELF_DIR/patch_onsyuri.py" -nt "$TARGET" ] || \
     [ -n "$(find "$FILTER_OVERLAY" -type f -newer "$TARGET" -print -quit 2>/dev/null)" ]; then
  FULL_RECOMPILE=1
  NEEDS_REBUILD=1
elif [ -n "$(find "$ONS_ROOT/src/onsyuri" "$LUA_SOURCE_DIR" -type f \
    \( -name '*.h' -o -name '*.hpp' \) -newer "$TARGET" -print -quit 2>/dev/null)" ]; then
  # This hand-written build does not maintain compiler dependency files, so a
  # changed header deliberately invalidates all ONS objects.
  FULL_RECOMPILE=1
  NEEDS_REBUILD=1
elif [ -n "$(find "$ONS_ROOT/src/onsyuri" -type f -name '*.cpp' \
    -newer "$TARGET" -print -quit 2>/dev/null)" ] || \
     [ -n "$(find "$LUA_SOURCE_DIR" -type f -name '*.c' -newer "$TARGET" -print -quit 2>/dev/null)" ] || \
     [ "$LUA_LIB" -nt "$TARGET" ] || \
     [ -n "$(find "$OBJ_DIR" -type f -name '*.o' -newer "$TARGET" -print -quit 2>/dev/null)" ]; then
  NEEDS_REBUILD=1
fi

if [ "$NEEDS_REBUILD" = "0" ]; then
  cp "$TARGET" "$RUNTIME_CORE_DIR/onsyuri"
  chmod +x "$RUNTIME_CORE_DIR/onsyuri" 2>/dev/null || true
  echo "[ons_build] reuse unchanged target: $TARGET"
  exit 0
fi

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
glibc234_compat.cpp
builtin_dll/layer_oldmovie.cpp
builtin_dll/layer_snow.cpp
builtin_dll/ONScripter_effect_cascade.cpp
builtin_dll/ONScripter_effect_trig.cpp
"

CXXFLAGS="-O2 -std=c++11 -Wall -Wno-deprecated-declarations -Wno-unused-result -Wno-write-strings --sysroot=$SYSROOT"
CXXFLAGS="$CXXFLAGS -ffile-prefix-map=$ONS_ROOT=/src/onsyuri -ffile-prefix-map=$REPO_ROOT=/workspace/ROCgalgame"
CXXFLAGS="$CXXFLAGS -DLINUX -DUSE_PARALLEL -DUSE_GLES -DUSE_FILELOG -DUSE_LUA -DUSE_BUILTIN_LAYER_EFFECTS -DENABLE_1BYTE_CHAR"
CXXFLAGS="$CXXFLAGS -I$INCLUDE_OVERLAY -I$PATCHED_SRC_DIR -I$ONS_ROOT/src/onsyuri -I$ONS_ROOT/src/onsyuri_libretro/deps -I$ONS_ROOT/src/onsyuri_libretro/deps/SDL_mixer/include -I$SYSROOT/usr/include -I$SYSROOT/usr/include/SDL2"
# GKD's libGLESv2 is a loader shim with no GL exports; ONS reaches GL through
# SDL_GL_* instead. Device SDL/ALSA libraries may themselves require newer
# glibc symbols, which are resolved by the GKD 2.40 runtime, not by the
# glibc 2.34 link baseline.
LDFLAGS="--sysroot=$SYSROOT -L$LIBDIR -Wl,--allow-shlib-undefined -Wl,-rpath,\$ORIGIN/../../lib -Wl,-rpath,\$ORIGIN/../../lib_system_sdl"
LIBS="-lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer -ljpeg -lbz2 -ldl -lpthread -lm"

{
  echo "[ons_build] ons_root=$ONS_ROOT"
  echo "[ons_build] sysroot=$SYSROOT"
  echo "[ons_build] cxx=$CXX_CMD"
  echo "[ons_build] libdir=$LIBDIR"
  if [ ! -f "$LUA_SOURCE_DIR/lua.h" ]; then
    echo "[ons_build] ERROR: missing Lua headers. Run: git -C '$ONS_ROOT' submodule update --init src/onsyuri_libretro/deps/lua"
    exit 1
  fi
  mkdir -p "$LUA_BUILD_DIR"
  rsync -a --no-owner --no-group --delete --exclude='*.o' --exclude='liblua.a' \
    "$LUA_SOURCE_DIR/" "$LUA_BUILD_DIR/"
  echo "[ons_build] build static Lua"
  if [ "$CLEAN_BUILD" = "1" ]; then
    make -C "$LUA_BUILD_DIR" clean >/dev/null 2>&1 || true
  fi
  nice -n 15 ionice -c 2 -n 7 make -C "$LUA_BUILD_DIR" liblua.a \
    CC="${TOOL_PREFIX}-gcc --sysroot=$SYSROOT" \
    AR="${TOOL_PREFIX}-ar rc" \
    RANLIB="${TOOL_PREFIX}-ranlib" \
    MYCFLAGS="-std=c99 -DLUA_USE_LINUX -DLUA_COMPAT_5_2" \
    MYLIBS="" \
    -j"$BUILD_JOBS"
  if [ "$FULL_RECOMPILE" = "1" ]; then
    rm -rf "$OBJ_DIR" "$INCLUDE_OVERLAY" "$PATCHED_SRC_DIR" "$TARGET"
  else
    rm -rf "$INCLUDE_OVERLAY" "$PATCHED_SRC_DIR"
  fi
  mkdir -p "$OBJ_DIR" "$INCLUDE_OVERLAY/SDL2" "$PATCHED_SRC_DIR"
  cp "$ONS_ROOT/src/onsyuri_libretro/deps/SDL_mixer/include/SDL_mixer.h" "$INCLUDE_OVERLAY/SDL2/SDL_mixer.h"
  cp "$SYSROOT/usr/include/SDL2/begin_code.h" "$INCLUDE_OVERLAY/SDL2/begin_code.h"
  cp "$SYSROOT/usr/include/SDL2/close_code.h" "$INCLUDE_OVERLAY/SDL2/close_code.h"
  python3 "$SELF_DIR/patch_onsyuri.py" \
    "$ONS_ROOT/src/onsyuri/ONScripter.cpp" "$PATCHED_SRC_DIR/ONScripter.cpp" \
    "$ONS_ROOT/src/onsyuri/ONScripter.h" "$PATCHED_SRC_DIR/ONScripter.h"
  grep -q 'roc_gkd_fullscreen = fullscreen_mode && isnan(sharpness)' "$PATCHED_SRC_DIR/ONScripter.cpp"
  grep -q 'ROC_ASPECT_FILL_WIDTH' "$PATCHED_SRC_DIR/ONScripter.cpp"
  grep -q 'roc_sentence_top_y_original' "$PATCHED_SRC_DIR/ONScripter.h"
  sed '/const auto hideCursorIfIdle = \[&\]() {/,/    };/ {
    s/if (!g_roc_mouse.cursor_visible || g_roc_mouse.primary_down ||/if (!g_roc_mouse.cursor_visible || g_roc_mouse.primary_down || root_button_link.next ||/
    s/        return true;/        return false;/
  }' "$ONS_ROOT/src/onsyuri/ONScripter_event.cpp" > "$PATCHED_SRC_DIR/ONScripter_event.cpp"
  IDLE_CURSOR_BLOCK="$(sed -n '/const auto hideCursorIfIdle = \[&\]() {/,/    };/p' \
    "$PATCHED_SRC_DIR/ONScripter_event.cpp")"
  if ! printf '%s\n' "$IDLE_CURSOR_BLOCK" | grep -q 'root_button_link.next' || \
     ! printf '%s\n' "$IDLE_CURSOR_BLOCK" | grep -q 'return false;' || \
     printf '%s\n' "$IDLE_CURSOR_BLOCK" | grep -q 'return true;'; then
    echo "[ons_build] ERROR: failed to patch idle virtual-cursor handling"
    exit 1
  fi
  OBJS=""
  COMPILED_OBJECTS=0
  OBJECTS_NEWER_THAN_TARGET=0
  for src in $SRCS; do
    obj="$OBJ_DIR/${src%.cpp}.o"
    source_path="$ONS_ROOT/src/onsyuri/$src"
    if [ -f "$PATCHED_SRC_DIR/$src" ]; then
      source_path="$PATCHED_SRC_DIR/$src"
    fi
    if [ -f "$FILTER_OVERLAY/$src" ]; then
      source_path="$FILTER_OVERLAY/$src"
    fi
    mkdir -p "$(dirname "$obj")"
    if [ "$FULL_RECOMPILE" = "1" ] || [ ! -f "$obj" ] || [ "$source_path" -nt "$obj" ]; then
      echo "[ons_build] compile $src"
      SOURCE_CXXFLAGS="$CXXFLAGS"
      if grep -q '#include "ONScripter.h"' "$source_path"; then
        # The generated header changes ONScripter's class layout. Force every
        # translation unit that uses the class to see that exact header; quote
        # includes otherwise prefer the source tree's unpatched sibling.
        SOURCE_CXXFLAGS="$SOURCE_CXXFLAGS -include $PATCHED_SRC_DIR/ONScripter.h"
      fi
      nice -n 15 ionice -c 2 -n 7 "$CXX_CMD" $SOURCE_CXXFLAGS -c "$source_path" -o "$obj"
      COMPILED_OBJECTS=$((COMPILED_OBJECTS + 1))
    else
      echo "[ons_build] reuse object $src"
    fi
    if [ ! -x "$TARGET" ] || [ "$obj" -nt "$TARGET" ]; then
      OBJECTS_NEWER_THAN_TARGET=1
    fi
    OBJS="$OBJS $obj"
  done
  if [ "$COMPILED_OBJECTS" -gt 0 ] || [ "$OBJECTS_NEWER_THAN_TARGET" = "1" ] || \
     [ ! -x "$TARGET" ] || [ "$LUA_LIB" -nt "$TARGET" ]; then
    echo "[ons_build] link $TARGET"
    nice -n 15 ionice -c 2 -n 7 "$CXX_CMD" $OBJS "$LUA_LIB" -o "$TARGET" $LDFLAGS $LIBS
  else
    echo "[ons_build] reuse linked target $TARGET"
  fi
  cp "$TARGET" "$RUNTIME_CORE_DIR/onsyuri"
  chmod +x "$RUNTIME_CORE_DIR/onsyuri" 2>/dev/null || true
  echo "[ons_build] installed: $RUNTIME_CORE_DIR/onsyuri"
} 2>&1 | tee "$LOG_FILE"

echo "[ons_build] log: $LOG_FILE"
