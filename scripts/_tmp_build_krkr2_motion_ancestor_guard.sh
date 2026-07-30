#!/bin/bash
set -euo pipefail

build=/mnt/d/Works/ROCgalgame/build/gkd350h/krkr2
motion_dir="$build/cpp/plugins/motionplayer"
object="$motion_dir/CMakeFiles/motionplayer.dir/PlayerMotionLoad.cpp.o"
archive="$motion_dir/libmotionplayer.a"
archive_tmp=/tmp/libmotionplayer.ancestor-cycle-guard.a
binary="$build/bin/krkr2/krkr2"
backup=/tmp/krkr2-motion-ancestor-guard-backup
start=$(date +%s)

rm -rf "$backup"
mkdir -p "$backup"
cp -p "$object" "$backup/PlayerMotionLoad.cpp.o"
cp -p "$archive" "$backup/libmotionplayer.a"
cp -p "$binary" "$backup/krkr2"

restore_previous() {
    status=$?
    cp -p "$backup/PlayerMotionLoad.cpp.o" "$object"
    cp -p "$backup/libmotionplayer.a" "$archive"
    cp -p "$backup/krkr2" "$binary"
    echo "[motion-ancestor-guard] FAILED status=$status; object, archive, and binary restored"
    exit "$status"
}
trap restore_previous ERR

echo "[motion-ancestor-guard] progress=0% eta=7-10m exact-incremental=PlayerMotionLoad.cpp.o+archive+link"
cd "$build"
make -f cpp/plugins/motionplayer/CMakeFiles/motionplayer.dir/build.make \
    cpp/plugins/motionplayer/CMakeFiles/motionplayer.dir/PlayerMotionLoad.cpp.o
elapsed=$(( $(date +%s) - start ))
echo "[motion-ancestor-guard] progress=35% elapsed=${elapsed}s eta~$((elapsed * 2))s"

rm -f "$archive_tmp"
cd "$motion_dir"
sed "s#libmotionplayer.a#$archive_tmp#g" \
    CMakeFiles/motionplayer.dir/link.txt | bash
mv -f "$archive_tmp" "$archive"
elapsed=$(( $(date +%s) - start ))
echo "[motion-ancestor-guard] progress=50% elapsed=${elapsed}s eta~${elapsed}s"

bash /mnt/d/Works/ROCgalgame/scripts/_tmp_krkr2_link_alpha.sh
elapsed=$(( $(date +%s) - start ))
echo "[motion-ancestor-guard] progress=100% elapsed=${elapsed}s eta=0s"
sha256sum "$binary"
