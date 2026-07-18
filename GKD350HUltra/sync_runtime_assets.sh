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
for asset_dir in ui fonts sounds; do
  mkdir -p "$RUNTIME_DIR/$asset_dir"
  rsync -a --delete "$REPO_ROOT/$asset_dir/" "$RUNTIME_DIR/$asset_dir/"
done
chmod +x "$DIST_ROOT/ROCgalgame.sh" 2>/dev/null || true
echo "[assets] synchronized launcher, config, UI, fonts, and sounds"
