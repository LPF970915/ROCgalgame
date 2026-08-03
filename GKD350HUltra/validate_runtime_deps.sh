#!/bin/bash
set -euo pipefail

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SELF_DIR/.." && pwd)"
SYSROOT="${SYSROOT:-$REPO_ROOT/build/gkd350h-glibc234/sysroot}"
DIST_ROOT="${DIST_ROOT:-$SELF_DIR/dist_glibc234}"
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

declare -A library_index=()
index_library_tree() {
  local root="$1" max_depth="$2" candidate name
  [ -d "$root" ] || return 0
  while IFS= read -r -d '' candidate; do
    name="${candidate##*/}"
    if [ -z "${library_index[$name]+present}" ]; then
      library_index["$name"]="$candidate"
    fi
  done < <(find "$root" -maxdepth "$max_depth" -name '*.so*' \
    \( -type f -o -type l \) -size +0c -print0 2>/dev/null)
}

# Runtime libraries take precedence. Index each tree once because repeatedly
# traversing the sysroot over a Windows/WSL mount makes reuse-only packaging slow.
index_library_tree "$RUNTIME" 5
index_library_tree "$SYSROOT/lib" 4
index_library_tree "$SYSROOT/usr/lib" 5

resolve_library() {
  local name="$1"
  printf '%s' "${library_index[$name]-}"
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
