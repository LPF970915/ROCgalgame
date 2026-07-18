#!/bin/sh
set -eu

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
SYSROOT="${SYSROOT:-$SELF_DIR/sysroot_device}"
DIST_ROOT="${DIST_ROOT:-$SELF_DIR/dist_lowglibc}"
failed=0

for binary in "$DIST_ROOT/ROCgalgame/rocgalgame_sdl" \
              "$DIST_ROOT/ROCgalgame/cores/ons/onsyuri" \
              "$DIST_ROOT/ROCgalgame/cores/krkr/krkrsdl2"; do
  [ -x "$binary" ] || { echo "[deps] ERROR: missing executable $binary"; failed=1; continue; }
  echo "[deps] $binary"
  readelf -d "$binary" 2>/dev/null | grep -E 'NEEDED|RUNPATH|RPATH' || true
  for lib in $(readelf -d "$binary" 2>/dev/null | sed -n 's/.*Shared library: \[*\([^]]*\)\].*/\1/p'); do
    if ! find "$DIST_ROOT/ROCgalgame/lib" "$DIST_ROOT/ROCgalgame/lib_system_sdl" \
              "$SYSROOT/lib" "$SYSROOT/usr/lib" -name "$lib" -print -quit 2>/dev/null | grep -q .; then
      echo "[deps] ERROR: unresolved $lib required by $binary"
      failed=1
    fi
  done
done
exit "$failed"
