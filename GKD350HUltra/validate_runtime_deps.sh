#!/bin/bash
set -euo pipefail

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
SYSROOT="${SYSROOT:-$SELF_DIR/sysroot_device}"
DIST_ROOT="${DIST_ROOT:-$SELF_DIR/dist_lowglibc}"
RUNTIME="$DIST_ROOT/ROCgalgame"
failed=0
queue_file="$(mktemp)"
seen_file="$(mktemp)"
trap 'rm -f "$queue_file" "$seen_file"' EXIT

for binary in "$RUNTIME/rocgalgame_sdl" \
              "$RUNTIME/cores/ons/onsyuri" \
              "$RUNTIME/cores/krkr/krkrsdl2" \
              "$RUNTIME/cores/krkr/krkr2"; do
  if [ ! -x "$binary" ]; then
    echo "[deps] ERROR: missing executable $binary"
    failed=1
  else
    printf '%s\n' "$binary" >>"$queue_file"
  fi
done

resolve_library() {
  local name="$1" candidate
  candidate="$(find "$RUNTIME" -type f -name "$name" -print -quit 2>/dev/null || true)"
  if [ -z "$candidate" ]; then
    candidate="$(find "$SYSROOT/lib" "$SYSROOT/usr/lib" -name "$name" -size +0c -print -quit 2>/dev/null || true)"
  fi
  printf '%s' "$candidate"
}

index=1
while path="$(sed -n "${index}p" "$queue_file")" && [ -n "$path" ]; do
  index=$((index + 1))
  grep -Fqx "$path" "$seen_file" 2>/dev/null && continue
  printf '%s\n' "$path" >>"$seen_file"
  readelf -h "$path" 2>/dev/null | grep -q 'Machine:.*AArch64' || {
    echo "[deps] ERROR: dependency is not AArch64 ELF: $path"
    failed=1
    continue
  }
  echo "[deps] inspect $path"
  while IFS= read -r lib; do
    [ -n "$lib" ] || continue
    resolved="$(resolve_library "$lib")"
    if [ -z "$resolved" ]; then
      echo "[deps] ERROR: unresolved $lib required by $path"
      failed=1
    else
      printf '%s\n' "$resolved" >>"$queue_file"
    fi
  done < <(readelf -d "$path" 2>/dev/null | sed -n 's/.*Shared library: \[*\([^]]*\)\].*/\1/p')
done

if [ "$failed" -ne 0 ]; then exit 1; fi
echo "[deps] passed: $(wc -l <"$seen_file") ELF files in the recursive dependency closure"
