#!/bin/bash
set -euo pipefail

for binary in \
  /tmp/rocgalgame_krkr2_before_camera_offset \
  /mnt/d/Works/ROCgalgame/build/gkd350h/krkr2/bin/krkr2/krkr2; do
  echo "== $binary =="
  strings "$binary" | grep -m 8 -E 'AlphaMovie\.dll|AlphaMoviePlayer|AlphaMovie' || true
done

echo '== archive =='
ar t /mnt/d/Works/ROCgalgame/build/gkd350h/krkr2/cpp/plugins/libkrkr2plugin.a \
  | grep -E 'AlphaMovie|PackinOne|gfxEffect' || true
