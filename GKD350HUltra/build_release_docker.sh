#!/bin/bash
set -euo pipefail

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
DIST_ROOT="${DIST_ROOT:-$SELF_DIR/dist_glibc234}"
VERSION="${1:-}"
BUILD_JOBS="${ROC_RELEASE_JOBS:-2}"
FORCE="${ROC_RELEASE_FORCE:-0}"
CORE_HASHES="$SELF_DIR/release_core_hashes.sha256"
PACKAGE_NAME="ROCgalgame ver$VERSION for GKD350H Ultra"
ZIP_FILE="$SELF_DIR/Downloads/$PACKAGE_NAME.zip"
CHECKSUM_FILE="$ZIP_FILE.sha256"

if ! printf '%s' "$VERSION" | grep -Eq '^[0-9]+\.[0-9]{2}$'; then
  echo "[release] ERROR: version must look like 0.01"
  exit 2
fi
if ! printf '%s' "$BUILD_JOBS" | grep -Eq '^[1-9][0-9]*$'; then
  echo "[release] ERROR: ROC_RELEASE_JOBS must be a positive integer"
  exit 2
fi
case "$FORCE" in
  0|1) ;;
  *) echo "[release] ERROR: ROC_RELEASE_FORCE must be 0 or 1"; exit 2 ;;
esac
if { [ -e "$ZIP_FILE" ] || [ -e "$CHECKSUM_FILE" ]; } && [ "$FORCE" != "1" ]; then
  echo "[release] ERROR: release output already exists: $ZIP_FILE"
  exit 1
fi
if [ ! -f "$CORE_HASHES" ]; then
  echo "[release] ERROR: missing core hash manifest: $CORE_HASHES"
  exit 1
fi

verify_reused_cores() {
  echo "[release] verify reused ONS/KRKR cores"
  (cd "$DIST_ROOT" && sha256sum -c "$CORE_HASHES")
}

verify_reused_cores

echo "[release] clean Docker frontend build"
DIST_ROOT="$DIST_ROOT" \
ROC_BUILD_JOBS="$BUILD_JOBS" \
ROC_CLEAN_BUILD=1 \
  "$SELF_DIR/build_low_glibc.sh"

verify_reused_cores

echo "[release] assemble version $VERSION without rebuilding ONS/KRKR"
DIST_ROOT="$DIST_ROOT" \
PACKAGE_BUILD_FRONTEND=0 \
PACKAGE_BUILD_ONS=0 \
PACKAGE_BUILD_KRKR=0 \
PACKAGE_OUTPUT=Zip \
PACKAGE_FORCE="$FORCE" \
ROCGALGAME_VERSION="$VERSION" \
  "$SELF_DIR/build_package.sh"

verify_reused_cores

if [ ! -f "$ZIP_FILE" ]; then
  echo "[release] ERROR: expected archive was not created: $ZIP_FILE"
  exit 1
fi
ZIP_HASH="$(sha256sum "$ZIP_FILE" | awk '{print $1}')"
CHECKSUM_TEMP="$SELF_DIR/Downloads/.$PACKAGE_NAME.zip.sha256.tmp.$$"
printf '%s  %s\n' "$ZIP_HASH" "$(basename "$ZIP_FILE")" > "$CHECKSUM_TEMP"
if [ "$FORCE" = "1" ]; then
  mv -f -- "$CHECKSUM_TEMP" "$CHECKSUM_FILE"
else
  mv -n -- "$CHECKSUM_TEMP" "$CHECKSUM_FILE"
  if [ -e "$CHECKSUM_TEMP" ]; then
    echo "[release] ERROR: checksum file appeared during packaging: $CHECKSUM_FILE"
    exit 1
  fi
fi
RECORDED_HASH="$(awk 'NR == 1 {print $1}' "$CHECKSUM_FILE")"
RECORDED_NAME="$(cut -d ' ' -f 3- "$CHECKSUM_FILE")"
if [ "$RECORDED_HASH" != "$ZIP_HASH" ] || [ "$RECORDED_NAME" != "$(basename "$ZIP_FILE")" ]; then
  echo "[release] ERROR: checksum readback mismatch"
  exit 1
fi
echo "[release] output: $ZIP_FILE"
echo "[release] checksum: $CHECKSUM_FILE"
