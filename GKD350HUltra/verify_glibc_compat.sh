#!/bin/bash
set -euo pipefail

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
DIST_ROOT="${DIST_ROOT:-$SELF_DIR/dist_lowglibc}"
MAX_GLIBC="${MAX_GLIBC:-2.34}"
ALLOW_HOST_PATH_LEAK="${ROCGALGAME_ALLOW_HOST_PATH_LEAK:-0}"
failed=0
checked=0

version_gt() {
  [ "$(printf '%s\n%s\n' "$1" "$2" | sort -V | tail -n 1)" = "$1" ] && [ "$1" != "$2" ]
}

check_elf() {
  local path="$1" enforce_baseline="${2:-0}" machine versions highest
  readelf -h "$path" >/dev/null 2>&1 || return 0
  checked=$((checked + 1))
  machine="$(readelf -h "$path" | sed -n 's/^[[:space:]]*Machine:[[:space:]]*//p')"
  case "$machine" in
    *AArch64*) ;;
    *) echo "[abi] ERROR: non-AArch64 ELF: $path ($machine)"; failed=1; return ;;
  esac
  versions="$(readelf --version-info "$path" 2>/dev/null | grep -oE 'GLIBC_[0-9]+(\.[0-9]+)+' | sed 's/^GLIBC_//' | sort -Vu || true)"
  highest="$(printf '%s\n' "$versions" | sed '/^$/d' | tail -n 1)"
  [ -n "$highest" ] || highest="none"
  echo "[abi] $(realpath --relative-to="$DIST_ROOT" "$path" 2>/dev/null || printf '%s' "$path"): GLIBC_$highest"
  if [ "$enforce_baseline" = "1" ] && [ "$highest" != "none" ] && version_gt "$highest" "$MAX_GLIBC"; then
    echo "[abi] ERROR: requires GLIBC_$highest, baseline is GLIBC_$MAX_GLIBC"
    failed=1
  fi
  if grep -aEq '/workspace/|/mnt/[a-z]/Works/|[A-Za-z]:\\Works\\' "$path"; then
    if [ "$ALLOW_HOST_PATH_LEAK" = "1" ]; then
      echo "[abi] WARNING: historical host build path retained in $path"
    else
      echo "[abi] ERROR: host build path leaked into $path"
      failed=1
    fi
  fi
}

if [ "$#" -gt 0 ]; then
  for path in "$@"; do
    [ -f "$path" ] || { echo "[abi] ERROR: missing $path"; failed=1; continue; }
    check_elf "$path" 1
  done
else
  runtime="$DIST_ROOT/ROCgalgame"
  [ -d "$runtime" ] || { echo "[abi] ERROR: missing runtime: $runtime"; exit 1; }
  for forbidden in libc.so.6 libm.so.6 libpthread.so.0 libdl.so.2 ld-linux-aarch64.so.1; do
    if find "$runtime" -name "$forbidden" -print -quit | grep -q .; then
      echo "[abi] ERROR: forbidden glibc runtime bundled: $forbidden"
      failed=1
    fi
  done
  while IFS= read -r -d '' path; do check_elf "$path" 0; done < <(find "$runtime" -type f -print0)
  for path in \
    "$runtime/rocgalgame_sdl" \
    "$runtime/cores/ons/onsyuri" \
    "$runtime/cores/krkr/krkrsdl2" \
    "$runtime/cores/krkr/krkr2"; do
    [ -f "$path" ] || { echo "[abi] ERROR: missing $path"; failed=1; continue; }
    check_elf "$path" 1
  done
fi

[ "$checked" -gt 0 ] || { echo "[abi] ERROR: no ELF files checked"; exit 1; }
if [ "$failed" -ne 0 ]; then exit 1; fi
echo "[abi] passed: $checked AArch64 ELF checks; self-built executables <= GLIBC_$MAX_GLIBC"
