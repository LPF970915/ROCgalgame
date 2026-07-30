#!/bin/bash
set -euo pipefail

probe=/mnt/d/Works/ROCgalgame/GKD350HUltra/probes/xp3_probe.py
game='/mnt/d/Works/ROCgalgame/games/Criss Cross'
root=/tmp/rocgalgame-criss-plugin-scripts
rm -rf "$root"
mkdir -p "$root"

for archive in "$game"/*.xp3; do
  name="$(basename "$archive" .xp3)"
  output="$root/$name"
  mkdir -p "$output"
  python3 "$probe" "$archive" --match 'Plugin.tjs' --extract "$output" \
    >"$root/$name.list" 2>/dev/null || true
done

find "$root" -type f -name '*.tjs' -print0 | while IFS= read -r -d '' file; do
  echo "== $file =="
  strings -el "$file" | grep -i -E 'setEmotePSBDecryptFunc|decrypt|binary|buffer|seed' || true
done
