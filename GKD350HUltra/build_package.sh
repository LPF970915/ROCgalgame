#!/bin/bash
set -euo pipefail

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
DIST_ROOT="${DIST_ROOT:-$SELF_DIR/dist_lowglibc}"
RUNTIME_DIR="$DIST_ROOT/ROCgalgame"
DOWNLOADS_DIR="${DOWNLOADS_DIR:-$SELF_DIR/Downloads}"
VERSION="${ROCGALGAME_VERSION:-0.1}"
PACKAGE_NAME="ROCgalgame_GKD350HUltra_v$VERSION"
STAGING_DIR="$DOWNLOADS_DIR/$PACKAGE_NAME"
ZIP_FILE="$DOWNLOADS_DIR/$PACKAGE_NAME.zip"
TAR_FILE="$DOWNLOADS_DIR/$PACKAGE_NAME.tar.gz"
BUILD_FRONTEND="${PACKAGE_BUILD_FRONTEND:-1}"
BUILD_ONS="${PACKAGE_BUILD_ONS:-1}"
BUILD_KRKR="${PACKAGE_BUILD_KRKR:-1}"
PACKAGE_OUTPUT="${PACKAGE_OUTPUT:-Stage}"

case "$PACKAGE_OUTPUT" in
  Stage|Zip|Tar|Both) ;;
  *) echo "[package] ERROR: PACKAGE_OUTPUT must be Stage, Zip, Tar, or Both"; exit 2 ;;
esac

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

if [ "$BUILD_FRONTEND" = "1" ]; then
  echo "[package] build frontend"
  "$SELF_DIR/build_low_glibc.sh"
else
  echo "[package] reuse frontend"
fi

if [ "$BUILD_ONS" = "1" ]; then
  echo "[package] build ONS core"
  "$SELF_DIR/build_onsyuri.sh"
else
  echo "[package] reuse ONS core"
fi

if [ "$BUILD_KRKR" = "1" ]; then
  echo "[package] build KRKR core"
  "$SELF_DIR/build_krkr.sh"
else
  echo "[package] reuse KRKR core"
fi

# UI/config changes must reach dist even when all binaries are reused.
bash "$SELF_DIR/sync_runtime_assets.sh"

check_executable "$DIST_ROOT/ROCgalgame.sh"
check_executable "$RUNTIME_DIR/rocgalgame_sdl"
check_executable "$RUNTIME_DIR/cores/ons/onsyuri"
check_executable "$RUNTIME_DIR/cores/krkr/krkrsdl2"
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
show_elf_info "$RUNTIME_DIR/cores/krkr/krkrsdl2"
"$SELF_DIR/validate_runtime_deps.sh"

if [ "$PACKAGE_OUTPUT" = "Stage" ]; then
  echo "[package] staged and validated without archive: $DIST_ROOT"
  exit 0
fi

mkdir -p "$DOWNLOADS_DIR"
rm -rf "$STAGING_DIR"
mkdir -p "$STAGING_DIR/ROCgalgame"
cp "$DIST_ROOT/ROCgalgame.sh" "$STAGING_DIR/ROCgalgame.sh"
rsync -a --delete \
  --exclude='/games/***' --exclude='/covers/***' --exclude='/saves/***' --exclude='/cache/***' \
  "$RUNTIME_DIR/" "$STAGING_DIR/ROCgalgame/"
mkdir -p "$STAGING_DIR/ROCgalgame/games" "$STAGING_DIR/ROCgalgame/covers" \
  "$STAGING_DIR/ROCgalgame/saves" "$STAGING_DIR/ROCgalgame/cache"
chmod +x "$STAGING_DIR/ROCgalgame.sh" "$STAGING_DIR/ROCgalgame/rocgalgame_sdl" \
  "$STAGING_DIR/ROCgalgame/cores/ons/onsyuri" "$STAGING_DIR/ROCgalgame/cores/krkr/krkrsdl2" 2>/dev/null || true

if [ "$PACKAGE_OUTPUT" = "Zip" ] || [ "$PACKAGE_OUTPUT" = "Both" ]; then
  command -v zip >/dev/null 2>&1 || { echo "[package] ERROR: zip is required for $PACKAGE_OUTPUT output"; exit 1; }
  rm -f "$ZIP_FILE"
  (cd "$STAGING_DIR" && nice -n 10 zip -qr "$ZIP_FILE" ROCgalgame.sh ROCgalgame)
  echo "[package] wrote $ZIP_FILE"
fi
if [ "$PACKAGE_OUTPUT" = "Tar" ] || [ "$PACKAGE_OUTPUT" = "Both" ]; then
  rm -f "$TAR_FILE"
  (cd "$STAGING_DIR" && nice -n 10 tar -czf "$TAR_FILE" ROCgalgame.sh ROCgalgame)
  echo "[package] wrote $TAR_FILE"
fi

rm -rf "$STAGING_DIR"
echo "[package] done"
