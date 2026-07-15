#!/bin/sh
set -eu

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
DIST_ROOT="${DIST_ROOT:-$SELF_DIR/dist_lowglibc}"
RUNTIME_DIR="$DIST_ROOT/ROCgalgame"
DOWNLOADS_DIR="${DOWNLOADS_DIR:-$SELF_DIR/Downloads}"
VERSION="${ROCGALGAME_VERSION:-0.1}"
PACKAGE_NAME="ROCgalgame_GKD350HUltra_v$VERSION"
STAGING_DIR="$DOWNLOADS_DIR/$PACKAGE_NAME"
ZIP_FILE="$DOWNLOADS_DIR/$PACKAGE_NAME.zip"
TAR_FILE="$DOWNLOADS_DIR/$PACKAGE_NAME.tar.gz"

check_file() {
  path="$1"
  if [ ! -e "$path" ]; then
    echo "[package] ERROR: missing $path"
    exit 1
  fi
}

check_executable() {
  path="$1"
  check_file "$path"
  if [ ! -x "$path" ]; then
    echo "[package] ERROR: not executable $path"
    exit 1
  fi
}

show_elf_info() {
  path="$1"
  if command -v readelf >/dev/null 2>&1; then
    echo "[package] ELF dynamic info: $path"
    readelf -d "$path" 2>/dev/null | grep -E 'NEEDED|RUNPATH|RPATH' || true
  fi
}

echo "[package] build frontend"
"$SELF_DIR/build_low_glibc.sh"

echo "[package] build ONS core"
"$SELF_DIR/build_onsyuri.sh"

check_executable "$DIST_ROOT/ROCgalgame.sh"
check_executable "$RUNTIME_DIR/rocgalgame_sdl"
check_executable "$RUNTIME_DIR/cores/ons/onsyuri"
check_file "$RUNTIME_DIR/native_config.ini"
check_file "$RUNTIME_DIR/native_keymap.ini"
check_file "$RUNTIME_DIR/ui"
check_file "$RUNTIME_DIR/fonts"
check_file "$RUNTIME_DIR/sounds"
check_file "$RUNTIME_DIR/games"
check_file "$RUNTIME_DIR/covers"
check_file "$RUNTIME_DIR/saves"

show_elf_info "$RUNTIME_DIR/rocgalgame_sdl"
show_elf_info "$RUNTIME_DIR/cores/ons/onsyuri"

mkdir -p "$DOWNLOADS_DIR"
rm -rf "$STAGING_DIR"
mkdir -p "$STAGING_DIR"
cp -r "$DIST_ROOT/ROCgalgame.sh" "$DIST_ROOT/ROCgalgame" "$STAGING_DIR/"
rm -f "$STAGING_DIR/ROCgalgame/cache/launch_request.ini"
chmod +x "$STAGING_DIR/ROCgalgame.sh" "$STAGING_DIR/ROCgalgame/rocgalgame_sdl" "$STAGING_DIR/ROCgalgame/cores/ons/onsyuri" 2>/dev/null || true

rm -f "$ZIP_FILE" "$TAR_FILE"
if command -v zip >/dev/null 2>&1; then
  (cd "$STAGING_DIR" && zip -qr "$ZIP_FILE" ROCgalgame.sh ROCgalgame)
  echo "[package] wrote $ZIP_FILE"
else
  echo "[package] zip not found; skipping zip output"
fi
(cd "$STAGING_DIR" && tar -czf "$TAR_FILE" ROCgalgame.sh ROCgalgame)
echo "[package] wrote $TAR_FILE"

rm -rf "$STAGING_DIR"
echo "[package] done"
