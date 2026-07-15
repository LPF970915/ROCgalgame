#!/usr/bin/env bash
set -euo pipefail

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
PROJECT_ROOT="$(CDPATH= cd -- "$SELF_DIR/.." && pwd)"
ONS_ROOT="${ONS_ROOT:-/d/Works/Tyranor/OnscripterYuri}"
OUT_DIR="$SELF_DIR/build/onsyuri"
OBJ_DIR="$OUT_DIR/obj"
LUA_BUILD_DIR="$OUT_DIR/lua"
TARGET="$OUT_DIR/onsyuri.exe"
RUNTIME_TARGET="$PROJECT_ROOT/cores/ons/onsyuri.exe"

for tool in g++ gcc ar ranlib make pkg-config; do
  command -v "$tool" >/dev/null 2>&1 || { echo "[ERROR] missing tool: $tool"; exit 10; }
done
pkg-config --exists sdl2 SDL2_image SDL2_ttf SDL2_mixer || {
  echo "[ERROR] install the UCRT64 SDL2, SDL2_image, SDL2_ttf and SDL2_mixer packages"
  exit 11
}
test -d "$ONS_ROOT/src/onsyuri" || { echo "[ERROR] invalid ONS_ROOT: $ONS_ROOT"; exit 12; }

mkdir -p "$OBJ_DIR" "$PROJECT_ROOT/cores/ons"
if [ ! -f "$LUA_BUILD_DIR/.windows-ucrt64" ] || ! file "$LUA_BUILD_DIR/lapi.o" 2>/dev/null | grep -q 'x86-64 COFF'; then
  echo "[windows] build Lua"
  rm -rf "$LUA_BUILD_DIR"
  mkdir -p "$LUA_BUILD_DIR"
  cp -r "$ONS_ROOT/src/onsyuri_libretro/deps/lua/." "$LUA_BUILD_DIR/"
  find "$LUA_BUILD_DIR" -maxdepth 1 -type f \( -name '*.o' -o -name '*.a' -o -name 'lua' \) -delete
  make -C "$LUA_BUILD_DIR" liblua.a \
    CC=gcc AR="ar rc" RANLIB=ranlib \
    MYCFLAGS="-std=c99 -DLUA_COMPAT_5_2" MYLIBS="" -j4
  touch "$LUA_BUILD_DIR/.windows-ucrt64"
fi

SOURCES=(
  AnimationInfo.cpp coding2utf16.cpp DirectReader.cpp DirtyRect.cpp FontInfo.cpp
  gbk2utf16.cpp LUAHandler.cpp NsaReader.cpp ONScripter.cpp ONScripter_animation.cpp
  ONScripter_command.cpp ONScripter_effect.cpp ONScripter_effect_breakup.cpp
  ONScripter_event.cpp ONScripter_file.cpp ONScripter_file2.cpp ONScripter_image.cpp
  ONScripter_lut.cpp onscripter_main.cpp ONScripter_rmenu.cpp ONScripter_sound.cpp
  ONScripter_text.cpp Parallel.cpp resize_image.cpp SarReader.cpp ScriptHandler.cpp
  ScriptParser.cpp ScriptParser_command.cpp sjis2utf16.cpp renderer/gles_renderer.cpp
  builtin_dll/layer_oldmovie.cpp builtin_dll/layer_snow.cpp
  builtin_dll/ONScripter_effect_cascade.cpp builtin_dll/ONScripter_effect_trig.cpp
)

CXXFLAGS=(
  -O2 -std=c++11 -Wall -Wno-deprecated-declarations -Wno-unused-result -Wno-write-strings
  -D_WIN32 -DUSE_PARALLEL -DUSE_GLES -DUSE_FILELOG -DUSE_LUA
  -DUSE_BUILTIN_LAYER_EFFECTS -DENABLE_1BYTE_CHAR
  -I"$ONS_ROOT/src/onsyuri" -I"$ONS_ROOT/src/onsyuri_libretro/deps" -I"$LUA_BUILD_DIR"
  -I/ucrt64/include -I/ucrt64/include/SDL2 -I/ucrt64/include/freetype2
  -I/ucrt64/include/harfbuzz -I/ucrt64/include/glib-2.0
  -I/ucrt64/lib/glib-2.0/include -I/ucrt64/include/libpng16 -I/ucrt64/include/opus
)

OBJECTS=()
for source in "${SOURCES[@]}"; do
  src="$ONS_ROOT/src/onsyuri/$source"
  obj="$OBJ_DIR/${source%.cpp}.o"
  mkdir -p "$(dirname "$obj")"
  if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
    echo "[windows] compile $source"
    g++ "${CXXFLAGS[@]}" -c "$src" -o "$obj"
  fi
  OBJECTS+=("$obj")
done

echo "[windows] link $TARGET"
g++ "${OBJECTS[@]}" "$LUA_BUILD_DIR/liblua.a" -o "$TARGET" \
  -L/ucrt64/lib -lmingw32 -lSDL2main -lSDL2_image -lSDL2_ttf -lSDL2_mixer -lSDL2 \
  -ljpeg -lbz2 -static-libgcc -static-libstdc++ -Wl,-subsystem,console
cp "$TARGET" "$RUNTIME_TARGET"
echo "[windows] installed: $RUNTIME_TARGET"
