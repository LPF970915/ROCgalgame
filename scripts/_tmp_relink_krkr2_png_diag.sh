#!/bin/bash
set -euo pipefail

build=/mnt/d/Works/ROCgalgame/build/gkd350h/krkr2
visual_dir="$build/cpp/core/visual"
archive="$visual_dir/libcore_visual_module.a"
archive_tmp=/tmp/libcore_visual_module.png-diag.a
binary="$build/bin/krkr2/krkr2"
backup_archive=/tmp/libcore_visual_module.before-png-diag.a
backup_binary=/tmp/krkr2.before-png-diag-relink
start=$(date +%s)

cp -p "$archive" "$backup_archive"
cp -p "$binary" "$backup_binary"

restore_previous() {
    status=$?
    cp -p "$backup_archive" "$archive"
    cp -p "$backup_binary" "$binary"
    echo "[png-diag-relink] FAILED status=$status; archive and binary restored"
    exit "$status"
}
trap restore_previous ERR

echo "[png-diag-relink] progress=65% eta=3-5m compilation=none"
rm -f "$archive_tmp"
cd "$visual_dir"
sed "s#libcore_visual_module.a#$archive_tmp#g" \
    CMakeFiles/core_visual_module.dir/link.txt | bash
mv -f "$archive_tmp" "$archive"
elapsed=$(( $(date +%s) - start ))
echo "[png-diag-relink] progress=70% elapsed=${elapsed}s eta=2-4m"

bash /mnt/d/Works/ROCgalgame/scripts/_tmp_krkr2_link_alpha.sh
elapsed=$(( $(date +%s) - start ))
echo "[png-diag-relink] progress=100% elapsed=${elapsed}s eta=0s"
file "$binary"
sha256sum "$binary"
