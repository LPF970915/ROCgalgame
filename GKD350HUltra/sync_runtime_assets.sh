#!/bin/bash
set -euo pipefail

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SELF_DIR/.." && pwd)"
DIST_ROOT="${DIST_ROOT:-$SELF_DIR/dist_glibc234}"
RUNTIME_DIR="$DIST_ROOT/ROCgalgame"
SYSROOT="${SYSROOT:-$REPO_ROOT/build/gkd350h-glibc234/sysroot}"
KRKR2_GL_SOURCE="$SELF_DIR/compat/libglvnd-arm64/extracted/usr/lib/aarch64-linux-gnu/libGL.so.1.7.0"

mkdir -p "$RUNTIME_DIR/cache" "$RUNTIME_DIR/cores/ons" "$RUNTIME_DIR/cores/krkr" \
  "$RUNTIME_DIR/games" "$RUNTIME_DIR/covers" "$RUNTIME_DIR/saves"
cp "$REPO_ROOT/ROCgalgame.sh" "$DIST_ROOT/ROCgalgame.sh"
cp "$REPO_ROOT/native_config.ini" "$REPO_ROOT/native_keymap.ini" "$RUNTIME_DIR/"
test -f "$KRKR2_GL_SOURCE" || {
  echo "[assets] ERROR: missing KRKR2 private GLVND library: $KRKR2_GL_SOURCE"
  exit 1
}
mkdir -p "$RUNTIME_DIR/cores/krkr/lib_krkr2"
cp "$KRKR2_GL_SOURCE" "$RUNTIME_DIR/cores/krkr/lib_krkr2/libGL.so.1"

# The KRKR2 ELF is linked against libmali.so.0, while GKD devices expose the
# same vendor blob as libmali.so.1.9.0 (SONAME libmali.so.1). Bundle a private
# filename-compatible copy so the core does not depend on a device-specific
# compatibility symlink in /usr/lib.
MALI_SOURCE=""
for candidate in \
  "$SYSROOT/usr/lib/libmali.so.1.9.0" \
  "$SYSROOT/usr/lib/libmali.so.1" \
  "$SYSROOT/lib/libmali.so.1.9.0"; do
  if [ -f "$candidate" ] && [ -s "$candidate" ]; then
    MALI_SOURCE="$candidate"
    break
  fi
done
if [ -z "$MALI_SOURCE" ]; then
  echo "[assets] ERROR: missing AArch64 Mali runtime blob (libmali.so.1.9.0)"
  echo "[assets] Expected it in $SYSROOT/usr/lib or $SYSROOT/lib"
  exit 1
fi
mkdir -p "$RUNTIME_DIR/lib"
cp -L "$MALI_SOURCE" "$RUNTIME_DIR/lib/libmali.so.0"
rm -f "$RUNTIME_DIR/lib/libmali.so.1.9.0"
printf '%s\n' "${ROCGALGAME_VERSION:-0.01}" > "$RUNTIME_DIR/version.txt"
command -v python3 >/dev/null 2>&1 || { echo "[assets] ERROR: python3 is required to pack UI assets"; exit 1; }
python3 "$REPO_ROOT/scripts/pack_ui_assets.py" "$REPO_ROOT/ui" "$RUNTIME_DIR/ui.pack"
rm -rf "$RUNTIME_DIR/ui"
for asset_dir in fonts sounds; do
  mkdir -p "$RUNTIME_DIR/$asset_dir"
  rsync -a --delete "$REPO_ROOT/$asset_dir/" "$RUNTIME_DIR/$asset_dir/"
done
chmod +x "$DIST_ROOT/ROCgalgame.sh" 2>/dev/null || true
echo "[assets] synchronized launcher, config, encrypted UI pack, fonts, and sounds"
