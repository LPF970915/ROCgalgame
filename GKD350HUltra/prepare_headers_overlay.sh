#!/bin/sh
set -eu

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SELF_DIR/.." && pwd)"
SYSROOT="${SYSROOT:-$SELF_DIR/sysroot_device}"
HEADER_SOURCE="${HEADER_SOURCE:-$REPO_ROOT/H700/sysroot_device/usr/include}"
KRKR_ROOT="${KRKR_ROOT:-/mnt/d/Works/Tyranor/krkrsdl2}"
SDL_HEADER_SOURCE="${SDL_HEADER_SOURCE:-$KRKR_ROOT/external/SDL/include}"

if [ ! -d "$HEADER_SOURCE" ]; then
  if [ -d /usr/aarch64-linux-gnu/include ]; then
    HEADER_SOURCE=/usr/aarch64-linux-gnu/include
    echo "[gkd_headers] H700 headers missing; using the installed aarch64 cross headers"
  else
    echo "[gkd_headers] ERROR: no aarch64 header source is available"
    exit 1
  fi
fi
if [ ! -f "$SDL_HEADER_SOURCE/SDL.h" ]; then
  echo "[gkd_headers] ERROR: SDL2 headers missing: $SDL_HEADER_SOURCE"
  exit 1
fi

mkdir -p "$SYSROOT/usr/include"
echo "[gkd_headers] source: $HEADER_SOURCE"
echo "[gkd_headers] target: $SYSROOT/usr/include"
rsync -a "$HEADER_SOURCE/" "$SYSROOT/usr/include/"
mkdir -p "$SYSROOT/usr/include/SDL2"
rsync -a "$SDL_HEADER_SOURCE/" "$SYSROOT/usr/include/SDL2/"
for supplemental_sdl_header in SDL_image.h SDL_ttf.h; do
  if [ -f "/usr/include/SDL2/$supplemental_sdl_header" ]; then
    cp "/usr/include/SDL2/$supplemental_sdl_header" "$SYSROOT/usr/include/SDL2/"
  fi
done
if [ -d /usr/include/alsa ]; then
  mkdir -p "$SYSROOT/usr/include/alsa"
  rsync -a /usr/include/alsa/ "$SYSROOT/usr/include/alsa/"
fi
for supplemental_header in bzlib.h jpeglib.h jmorecfg.h jerror.h jpegint.h; do
  if [ -f "/usr/include/$supplemental_header" ]; then
    cp "/usr/include/$supplemental_header" "$SYSROOT/usr/include/"
  fi
done
for supplemental_dir in GLES2 KHR; do
  if [ -d "/usr/include/$supplemental_dir" ]; then
    mkdir -p "$SYSROOT/usr/include/$supplemental_dir"
    rsync -a "/usr/include/$supplemental_dir/" "$SYSROOT/usr/include/$supplemental_dir/"
  fi
done
if [ -d /usr/include/webp ]; then
  mkdir -p "$SYSROOT/usr/include/webp"
  rsync -a /usr/include/webp/ "$SYSROOT/usr/include/webp/"
fi
if [ -f "$REPO_ROOT/H700/sysroot_device/usr/lib/aarch64-linux-gnu/libzip/include/zipconf.h" ]; then
  cp "$REPO_ROOT/H700/sysroot_device/usr/lib/aarch64-linux-gnu/libzip/include/zipconf.h" \
    "$SYSROOT/usr/include/zipconf.h"
fi
mkdir -p "$SYSROOT/usr/lib"
for linker_file in libc_nonshared.a libpthread_nonshared.a crt1.o Scrt1.o crti.o crtn.o; do
  source_path="$(aarch64-linux-gnu-gcc -print-file-name="$linker_file")"
  if [ -f "$source_path" ]; then
    cp "$source_path" "$SYSROOT/usr/lib/$linker_file"
  fi
done
mkdir -p "$SYSROOT/usr/lib/pkgconfig"
cat >"$SYSROOT/usr/lib/pkgconfig/sdl2.pc" <<'EOF'
prefix=/usr
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: sdl2
Description: Simple DirectMedia Layer (GKD350HUltra sysroot)
Version: 2.32.0
Libs: -L${libdir} -lSDL2
Cflags: -I${includedir}/SDL2 -D_REENTRANT
EOF
echo "[gkd_headers] done"
