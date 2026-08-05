#!/bin/bash
set -euo pipefail

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SELF_DIR/.." && pwd)"
KRKR_ROOT="${KRKR_ROOT:-/mnt/d/Works/Tyranor/krkrsdl2}"
DIST_ROOT="${DIST_ROOT:-$SELF_DIR/dist_glibc234}"
KRKR_PORT_LOCK="${KRKR_PORT_LOCK:-$SELF_DIR/krkrsdl2-port.lock}"
FFMPEG_ROOT="${KRKR_FFMPEG_INCLUDE_DIR:-/mnt/d/Works/ROCgalgame-ffmpeg-n6-headers}"
FFMPEG_HEADERS_LOCK="${FFMPEG_HEADERS_LOCK:-$SELF_DIR/ffmpeg-headers.lock}"
BINARY="$DIST_ROOT/ROCgalgame/cores/krkr/krkrsdl2"
METADATA="$BINARY.build-meta"

bash "$SELF_DIR/verify_source_provenance.sh" "$KRKR_ROOT" krkrsdl2 "$KRKR_PORT_LOCK"
bash "$SELF_DIR/verify_source_provenance.sh" "$FFMPEG_ROOT" ffmpeg-headers "$FFMPEG_HEADERS_LOCK"
test -x "$BINARY" || { echo "[krkr_meta] ERROR: missing core binary: $BINARY"; exit 1; }

source_commit="$(git -c safe.directory="$KRKR_ROOT" -C "$KRKR_ROOT" rev-parse HEAD)"
krkrz_commit="$(git -c safe.directory="$KRKR_ROOT/external/krkrz" -C "$KRKR_ROOT/external/krkrz" rev-parse HEAD)"
ffmpeg_commit="$(git -c safe.directory="$FFMPEG_ROOT" -C "$FFMPEG_ROOT" rev-parse HEAD)"
expected_krkrz="$(sed -n 's/^krkrz_commit=//p' "$KRKR_PORT_LOCK" | head -n 1)"
test "$krkrz_commit" = "$expected_krkrz" || {
  echo "[krkr_meta] ERROR: krkrz commit $krkrz_commit does not match lock $expected_krkrz"
  exit 1
}

artifact_sha256="$(sha256sum "$BINARY" | awk '{print $1}')"
repository="$(sed -n 's/^repository=//p' "$KRKR_PORT_LOCK" | head -n 1)"
tmp="$METADATA.tmp.$$"
{
  printf 'schema=1\n'
  printf 'source=krkrsdl2\n'
  printf 'source_commit=%s\n' "$source_commit"
  printf 'krkrz_commit=%s\n' "$krkrz_commit"
  printf 'ffmpeg_headers_commit=%s\n' "$ffmpeg_commit"
  printf 'repository=%s\n' "$repository"
  printf 'source_dirty=0\n'
  printf 'port_lock=%s\n' "$KRKR_PORT_LOCK"
  printf 'artifact_sha256=%s\n' "$artifact_sha256"
} >"$tmp"
mv "$tmp" "$METADATA"
echo "[krkr_meta] wrote=$METADATA artifact_sha256=$artifact_sha256"
