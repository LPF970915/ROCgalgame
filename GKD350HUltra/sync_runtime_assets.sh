#!/bin/bash
set -euo pipefail

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SELF_DIR/.." && pwd)"
DIST_ROOT="${DIST_ROOT:-$SELF_DIR/dist_lowglibc}"
RUNTIME_DIR="$DIST_ROOT/ROCgalgame"

mkdir -p "$RUNTIME_DIR/cache" "$RUNTIME_DIR/cores/ons" "$RUNTIME_DIR/cores/krkr" \
  "$RUNTIME_DIR/games" "$RUNTIME_DIR/covers" "$RUNTIME_DIR/saves"
cp "$REPO_ROOT/ROCgalgame.sh" "$DIST_ROOT/ROCgalgame.sh"
cp "$REPO_ROOT/native_config.ini" "$REPO_ROOT/native_keymap.ini" "$RUNTIME_DIR/"
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
