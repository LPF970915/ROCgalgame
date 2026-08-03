#!/bin/bash
set -euo pipefail

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
DIST_ROOT="${DIST_ROOT:-$SELF_DIR/dist_glibc234}"
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
PACKAGE_FORCE="${PACKAGE_FORCE:-0}"

case "$PACKAGE_OUTPUT" in
  Stage|Zip|Tar|Both) ;;
  *) echo "[package] ERROR: PACKAGE_OUTPUT must be Stage, Zip, Tar, or Both"; exit 2 ;;
esac
case "$PACKAGE_FORCE" in
  0|1) ;;
  *) echo "[package] ERROR: PACKAGE_FORCE must be 0 or 1"; exit 2 ;;
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
check_executable "$RUNTIME_DIR/cores/krkr/krkr2"
check_file "$RUNTIME_DIR/cores/krkr/Resources"
check_file "$RUNTIME_DIR/cores/krkr/lib_krkr2/libGL.so.1"
readelf -h "$RUNTIME_DIR/cores/krkr/lib_krkr2/libGL.so.1" |
  grep -Eq 'Machine:[[:space:]]+AArch64' || {
    echo "[package] ERROR: KRKR2 private GLVND library is not AArch64"
    exit 1
  }
readelf -d "$RUNTIME_DIR/cores/krkr/lib_krkr2/libGL.so.1" |
  grep -Eq 'SONAME.*libGL\.so\.1' || {
    echo "[package] ERROR: KRKR2 private GLVND library has the wrong SONAME"
    exit 1
  }
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
show_elf_info "$RUNTIME_DIR/cores/krkr/krkr2"
"$SELF_DIR/validate_runtime_deps.sh"
if [ -n "${ROCGALGAME_GLIBC_BASELINE:-}" ]; then
  MAX_GLIBC="$ROCGALGAME_GLIBC_BASELINE" "$SELF_DIR/verify_glibc_compat.sh"
fi

if [ "$PACKAGE_OUTPUT" = "Stage" ]; then
  echo "[package] staged and validated without archive: $DIST_ROOT"
  exit 0
fi

mkdir -p "$DOWNLOADS_DIR"
DOWNLOADS_ABS="$(readlink -m "$DOWNLOADS_DIR")"
STAGING_ABS="$(readlink -m "$STAGING_DIR")"
EXPECTED_STAGE="$DOWNLOADS_ABS/.$PACKAGE_NAME.stage"
if [ "$STAGING_ABS" != "$EXPECTED_STAGE" ]; then
  echo "[package] ERROR: refusing unsafe staging cleanup: $STAGING_ABS"
  exit 1
fi
if { [ "$PACKAGE_OUTPUT" = "Zip" ] || [ "$PACKAGE_OUTPUT" = "Both" ]; } &&
   [ -e "$ZIP_FILE" ] && [ "$PACKAGE_FORCE" != "1" ]; then
  echo "[package] ERROR: target archive already exists: $ZIP_FILE"
  exit 1
fi
if { [ "$PACKAGE_OUTPUT" = "Tar" ] || [ "$PACKAGE_OUTPUT" = "Both" ]; } &&
   [ -e "$TAR_FILE" ] && [ "$PACKAGE_FORCE" != "1" ]; then
  echo "[package] ERROR: target archive already exists: $TAR_FILE"
  exit 1
fi
rm -rf -- "$STAGING_ABS"
APP_ROOT="$STAGING_DIR/app"
PORTS_DIR="$STAGING_DIR/roms/ports"
PACKAGE_RUNTIME_DIR="$APP_ROOT/ROCgalgame"
mkdir -p "$PACKAGE_RUNTIME_DIR" "$PORTS_DIR"
rsync -a --delete \
  --exclude='/games/***' --exclude='/covers/***' --exclude='/game_covers/***' \
  --exclude='/saves/***' --exclude='/cache/***' --exclude='/logs/***' \
  --exclude='/cores/krkr/*debug*' --exclude='/cores/krkr/krkr2.*' \
  --exclude='/cores/krkr/krkr2-*' \
  "$RUNTIME_DIR/" "$PACKAGE_RUNTIME_DIR/"
cp "$DIST_ROOT/ROCgalgame.sh" "$PACKAGE_RUNTIME_DIR/launch.sh"
cp "$DIST_ROOT/ROCgalgame.sh" "$PORTS_DIR/ROCgalgame.sh"
IUX_ICON="$SELF_DIR/../ui/common/icon.png"
[ -f "$IUX_ICON" ] || { echo "[package] ERROR: missing IUX icon: $IUX_ICON"; exit 1; }
cp "$IUX_ICON" "$PACKAGE_RUNTIME_DIR/rocgalgame.png"
mkdir -p "$PACKAGE_RUNTIME_DIR/games" "$PACKAGE_RUNTIME_DIR/covers" \
  "$PACKAGE_RUNTIME_DIR/saves" "$PACKAGE_RUNTIME_DIR/cache"
python3 - "$PACKAGE_RUNTIME_DIR/config.json" "$VERSION" <<'PY'
import json
import sys

path, version = sys.argv[1:]
config = {
    "software_code": "rocgalgame",
    "title": "ROCgalgame",
    "description": "ROC Galgame launcher",
    "version": version,
    "exec": "launch.sh",
    "workdir": ".",
    "icon": "rocgalgame.png",
}
with open(path, "w", encoding="utf-8", newline="\n") as stream:
    json.dump(config, stream, ensure_ascii=False, indent=2)
    stream.write("\n")
PY
chmod +x "$PACKAGE_RUNTIME_DIR/launch.sh" "$PORTS_DIR/ROCgalgame.sh" \
  "$PACKAGE_RUNTIME_DIR/rocgalgame_sdl" \
  "$PACKAGE_RUNTIME_DIR/cores/ons/onsyuri" "$PACKAGE_RUNTIME_DIR/cores/krkr/krkrsdl2" \
  "$PACKAGE_RUNTIME_DIR/cores/krkr/krkr2" 2>/dev/null || true

if [ -e "$PACKAGE_RUNTIME_DIR/ui" ]; then
  echo "[package] ERROR: plaintext UI directory leaked into release staging"
  exit 1
fi

if [ "$PACKAGE_OUTPUT" = "Zip" ] || [ "$PACKAGE_OUTPUT" = "Both" ]; then
  command -v python3 >/dev/null 2>&1 || { echo "[package] ERROR: python3 is required for UTF-8 zip output"; exit 1; }
  ZIP_TEMP="$STAGING_DIR/$PACKAGE_NAME.zip"
  nice -n 10 python3 "$SELF_DIR/create_release_zip.py" "$STAGING_DIR" "$ZIP_TEMP" app roms
  python3 "$SELF_DIR/verify_release_zip.py" "$ZIP_TEMP"
  if [ "$PACKAGE_FORCE" = "1" ]; then
    mv -f -- "$ZIP_TEMP" "$ZIP_FILE"
  else
    mv -n -- "$ZIP_TEMP" "$ZIP_FILE"
    if [ -e "$ZIP_TEMP" ]; then
      echo "[package] ERROR: target archive appeared during packaging: $ZIP_FILE"
      exit 1
    fi
  fi
  echo "[package] wrote $ZIP_FILE"
fi
if [ "$PACKAGE_OUTPUT" = "Tar" ] || [ "$PACKAGE_OUTPUT" = "Both" ]; then
  rm -f "$TAR_FILE"
  (cd "$STAGING_DIR" && nice -n 10 tar -czf "$TAR_FILE" app roms)
  echo "[package] wrote $TAR_FILE"
fi

rm -rf "$STAGING_DIR"
echo "[package] done"
