#!/bin/bash
set -euo pipefail

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
DIST_ROOT="${DIST_ROOT:-$SELF_DIR/dist_lowglibc}"
VERSION="${1:-}"
BUILD_JOBS="${ROC_RELEASE_JOBS:-2}"
CORE_HASHES="$SELF_DIR/release_core_hashes.sha256"

if ! printf '%s' "$VERSION" | grep -Eq '^[0-9]+\.[0-9]{2}$'; then
  echo "[release] ERROR: version must look like 0.01"
  exit 2
fi
if ! printf '%s' "$BUILD_JOBS" | grep -Eq '^[1-9][0-9]*$'; then
  echo "[release] ERROR: ROC_RELEASE_JOBS must be a positive integer"
  exit 2
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
ROCGALGAME_VERSION="$VERSION" \
  "$SELF_DIR/build_package.sh"

verify_reused_cores

PACKAGE_NAME="ROCgalgame ver$VERSION for GKD350H Ultra"
ZIP_FILE="$SELF_DIR/Downloads/$PACKAGE_NAME.zip"
if [ ! -f "$ZIP_FILE" ]; then
  echo "[release] ERROR: expected archive was not created: $ZIP_FILE"
  exit 1
fi
(cd "$(dirname "$ZIP_FILE")" && sha256sum "$(basename "$ZIP_FILE")" > "$(basename "$ZIP_FILE").sha256")
echo "[release] output: $ZIP_FILE"
echo "[release] checksum: $ZIP_FILE.sha256"
