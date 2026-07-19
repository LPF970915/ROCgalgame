#!/bin/bash
set -euo pipefail

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
DIST_ROOT="${DIST_ROOT:-$SELF_DIR/dist_lowglibc}"
RUNTIME_DIR="$DIST_ROOT/ROCgalgame"
DOWNLOADS_DIR="${DOWNLOADS_DIR:-$SELF_DIR/Downloads}"
VERSION="${ROCGALGAME_VERSION:-0.01}"
PACKAGE_NAME="ROCgalgame ver$VERSION for GKD350H Ultra"
STAGING_DIR="$DOWNLOADS_DIR/.$PACKAGE_NAME.stage"
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
check_file "$RUNTIME_DIR/ui.pack"
if [ -e "$RUNTIME_DIR/ui" ]; then
  echo "[package] ERROR: plaintext UI directory must not be staged"
  exit 1
fi
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
PORTS_DIR="$STAGING_DIR/roms/ports"
PACKAGE_RUNTIME_DIR="$PORTS_DIR/ROCgalgame"
mkdir -p "$PACKAGE_RUNTIME_DIR"
cp "$DIST_ROOT/ROCgalgame.sh" "$PORTS_DIR/ROCgalgame.sh"
rsync -a --delete \
  --exclude='/games/***' --exclude='/covers/***' --exclude='/saves/***' --exclude='/cache/***' \
  "$RUNTIME_DIR/" "$PACKAGE_RUNTIME_DIR/"
mkdir -p "$PACKAGE_RUNTIME_DIR/games" "$PACKAGE_RUNTIME_DIR/covers" \
  "$PACKAGE_RUNTIME_DIR/saves" "$PACKAGE_RUNTIME_DIR/cache"
chmod +x "$PORTS_DIR/ROCgalgame.sh" "$PACKAGE_RUNTIME_DIR/rocgalgame_sdl" \
  "$PACKAGE_RUNTIME_DIR/cores/ons/onsyuri" "$PACKAGE_RUNTIME_DIR/cores/krkr/krkrsdl2" 2>/dev/null || true

if find "$STAGING_DIR" -type d -name ui -print -quit | grep -q .; then
  echo "[package] ERROR: plaintext UI directory leaked into release staging"
  exit 1
fi

if [ "$PACKAGE_OUTPUT" = "Zip" ] || [ "$PACKAGE_OUTPUT" = "Both" ]; then
  command -v python3 >/dev/null 2>&1 || { echo "[package] ERROR: python3 is required for UTF-8 zip output"; exit 1; }
  rm -f "$ZIP_FILE"
  nice -n 10 python3 "$SELF_DIR/create_release_zip.py" "$STAGING_DIR" "$ZIP_FILE" roms
  python3 "$SELF_DIR/verify_release_zip.py" "$ZIP_FILE"
  echo "[package] wrote $ZIP_FILE"
fi
if [ "$PACKAGE_OUTPUT" = "Tar" ] || [ "$PACKAGE_OUTPUT" = "Both" ]; then
  rm -f "$TAR_FILE"
  (cd "$STAGING_DIR" && nice -n 10 tar -czf "$TAR_FILE" roms)
  echo "[package] wrote $TAR_FILE"
fi

rm -rf "$STAGING_DIR"
echo "[package] done"
