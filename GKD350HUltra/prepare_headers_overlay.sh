#!/bin/sh
set -eu

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SELF_DIR/.." && pwd)"
SYSROOT="${SYSROOT:-$SELF_DIR/sysroot_device}"
HEADER_SOURCE="${HEADER_SOURCE:-$REPO_ROOT/H700/sysroot_device/usr/include}"

if [ ! -d "$HEADER_SOURCE" ]; then
  echo "[gkd_headers] ERROR: header source missing: $HEADER_SOURCE"
  exit 1
fi

mkdir -p "$SYSROOT/usr/include"
echo "[gkd_headers] source: $HEADER_SOURCE"
echo "[gkd_headers] target: $SYSROOT/usr/include"
rsync -a "$HEADER_SOURCE/" "$SYSROOT/usr/include/"
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
echo "[gkd_headers] done"
