#!/bin/bash
set -euo pipefail

root=/tmp/rocgalgame-criss-data
mkdir -p "$root"
python3 /mnt/d/Works/ROCgalgame/GKD350HUltra/probes/xp3_probe.py \
  '/mnt/d/Works/ROCgalgame/games/Criss Cross/data.xp3' \
  --extract "$root" >/tmp/rocgalgame-criss-data-list.txt

find "$root" -type f -print0 | while IFS= read -r -d '' file; do
  matches="$(strings -el "$file" | grep -i -E 'setEmotePSBDecryptFunc|PSBDecrypt|decryptFunc' || true)"
  if [ -n "$matches" ]; then
    printf '%s\n%s\n' "$file" "$matches"
  fi
done
