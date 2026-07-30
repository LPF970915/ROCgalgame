#!/bin/bash
set -euo pipefail

build=/mnt/d/Works/ROCgalgame/build/gkd350h/krkr2
visual_dir="$build/cpp/core/visual"
object="$visual_dir/CMakeFiles/core_visual_module.dir/ogl/RenderManager_ogl.cpp.o"
archive="$visual_dir/libcore_visual_module.a"
archive_tmp=/tmp/libcore_visual_module.oversized-split.a
binary="$build/bin/krkr2/krkr2"
backup=/tmp/krkr2-oversized-split-build-backup
start=$(date +%s)

rm -rf "$backup"
mkdir -p "$backup"
cp -p "$object" "$backup/RenderManager_ogl.cpp.o"
cp -p "$archive" "$backup/libcore_visual_module.a"
cp -p "$binary" "$backup/krkr2"

restore_previous() {
    status=$?
    cp -p "$backup/RenderManager_ogl.cpp.o" "$object"
    cp -p "$backup/libcore_visual_module.a" "$archive"
    cp -p "$backup/krkr2" "$binary"
    echo "[oversized-split-build] FAILED status=$status; object, archive, and binary restored"
    exit "$status"
}
trap restore_previous ERR

echo "[oversized-split-build] progress=0% eta=7-10m pending=1 CXX object + archive + link"
cd "$build"
make -f cpp/core/visual/CMakeFiles/core_visual_module.dir/build.make \
    cpp/core/visual/CMakeFiles/core_visual_module.dir/ogl/RenderManager_ogl.cpp.o
elapsed=$(( $(date +%s) - start ))
echo "[oversized-split-build] progress=40% elapsed=${elapsed}s eta~$((elapsed * 3 / 2))s"

rm -f "$archive_tmp"
cd "$visual_dir"
sed "s#libcore_visual_module.a#$archive_tmp#g" \
    CMakeFiles/core_visual_module.dir/link.txt | bash
mv -f "$archive_tmp" "$archive"
elapsed=$(( $(date +%s) - start ))
echo "[oversized-split-build] progress=50% elapsed=${elapsed}s eta~${elapsed}s"

bash /mnt/d/Works/ROCgalgame/scripts/_tmp_krkr2_link_alpha.sh
elapsed=$(( $(date +%s) - start ))
echo "[oversized-split-build] progress=100% elapsed=${elapsed}s eta=0s"
file "$binary"
sha256sum "$binary"
