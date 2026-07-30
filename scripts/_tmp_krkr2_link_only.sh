#!/bin/bash
set -euo pipefail

build=/mnt/d/Works/ROCgalgame/build/gkd350h/krkr2
binary="$build/bin/krkr2/krkr2"
backup=/tmp/rocgalgame_krkr2_before_camera_offset

cd "$build"
cp -p "$binary" "$backup"
if ! bash CMakeFiles/krkr2.dir/link.txt; then
  cp -p "$backup" "$binary"
  echo "link failed; previous binary restored" >&2
  exit 1
fi

file "$binary"
stat -c '%y %s %n' "$binary"
