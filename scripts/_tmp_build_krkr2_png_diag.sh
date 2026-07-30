#!/bin/bash
set -euo pipefail

build=/mnt/d/Works/ROCgalgame/build/gkd350h/krkr2
visual_dir="$build/cpp/core/visual"
visual_objects="$visual_dir/CMakeFiles/core_visual_module.dir"
visual_archive="$visual_dir/libcore_visual_module.a"
visual_archive_tmp=/tmp/libcore_visual_module.png-diag.a
binary="$build/bin/krkr2/krkr2"
backup=/tmp/krkr2-png-diag-build-backup
log=/tmp/krkr2-png-diag-build.log

rm -rf "$backup"
mkdir -p "$backup"
cp -p "$visual_objects/GraphicsLoaderIntf.cpp.o" "$backup/"
cp -p "$visual_objects/LoadPNG.cpp.o" "$backup/"
cp -p "$visual_archive" "$backup/"
cp -p "$binary" "$backup/krkr2"

restore_previous() {
    status=$?
    cp -p "$backup/GraphicsLoaderIntf.cpp.o" "$visual_objects/GraphicsLoaderIntf.cpp.o"
    cp -p "$backup/LoadPNG.cpp.o" "$visual_objects/LoadPNG.cpp.o"
    cp -p "$backup/libcore_visual_module.a" "$visual_archive"
    cp -p "$backup/krkr2" "$binary"
    echo "[png-diag-build] FAILED status=$status; cached objects, archive, and binary restored"
    exit "$status"
}
trap restore_previous ERR

exec > >(tee "$log") 2>&1
start=$(date +%s)
echo "[png-diag-build] progress=0% eta=6-9m pending=2 CXX objects + archive + link"

cd "$build"
make -f cpp/core/visual/CMakeFiles/core_visual_module.dir/build.make \
    cpp/core/visual/CMakeFiles/core_visual_module.dir/GraphicsLoaderIntf.cpp.o
elapsed=$(( $(date +%s) - start ))
echo "[png-diag-build] progress=25% elapsed=${elapsed}s eta~$((elapsed * 3))s"

make -f cpp/core/visual/CMakeFiles/core_visual_module.dir/build.make \
    cpp/core/visual/CMakeFiles/core_visual_module.dir/LoadPNG.cpp.o
elapsed=$(( $(date +%s) - start ))
echo "[png-diag-build] progress=50% elapsed=${elapsed}s eta~${elapsed}s"

cd "$visual_dir"
rm -f "$visual_archive_tmp"
sed "s#libcore_visual_module.a#$visual_archive_tmp#g" \
    CMakeFiles/core_visual_module.dir/link.txt | bash
mv -f "$visual_archive_tmp" "$visual_archive"
elapsed=$(( $(date +%s) - start ))
echo "[png-diag-build] progress=65% elapsed=${elapsed}s eta~$((elapsed * 35 / 65))s"

bash /mnt/d/Works/ROCgalgame/scripts/_tmp_krkr2_link_alpha.sh
elapsed=$(( $(date +%s) - start ))
echo "[png-diag-build] progress=100% elapsed=${elapsed}s eta=0s"
file "$binary"
sha256sum "$binary"
echo "[png-diag-build] log=$log"
