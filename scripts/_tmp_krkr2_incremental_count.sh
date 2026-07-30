#!/bin/bash
set -euo pipefail

cd /mnt/d/Works/Tyranor/krkr2
for file in \
  cpp/plugins/motionplayer/Player.h \
  cpp/plugins/motionplayer/PlayerCore.cpp \
  cpp/plugins/motionplayer/EmotePlayer.h \
  cpp/plugins/motionplayer/EmotePlayer.cpp; do
  timestamp="$(git -c safe.directory='*' log -1 --format=%cI -- "$file")"
  test -n "$timestamp"
  touch -d "$timestamp" "$file"
done

cd /mnt/d/Works/ROCgalgame/build/gkd350h/krkr2
make -n krkr2 \
  | grep -E 'Building CXX object|Linking CXX executable' \
  | tee /tmp/roc_krkr2_pending_steps.txt \
  | wc -l
