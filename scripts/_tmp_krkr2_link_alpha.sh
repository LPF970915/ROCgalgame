#!/bin/bash
set -euo pipefail

build=/mnt/d/Works/ROCgalgame/build/gkd350h/krkr2
link="$build/CMakeFiles/krkr2.dir/link.txt"
binary="$build/bin/krkr2/krkr2"
backup=/tmp/rocgalgame_krkr2_before_alpha_link
patched=/tmp/rocgalgame_krkr2_link_with_alpha.txt

cd "$build"
grep -Fq 'CMakeFiles/krkr2.dir/cpp/plugins/AlphaMovie.cpp.o' "$link" && {
  echo 'AlphaMovie object already present'
  exit 0
}

sed 's#CMakeFiles/krkr2.dir/cpp/plugins/fstat/main.cpp.o#CMakeFiles/krkr2.dir/cpp/plugins/AlphaMovie.cpp.o CMakeFiles/krkr2.dir/cpp/plugins/fstat/main.cpp.o#' "$link" > "$patched"
cp -p "$binary" "$backup"
if ! bash "$patched"; then
  cp -p "$backup" "$binary"
  echo 'link failed; previous binary restored' >&2
  exit 1
fi
file "$binary"
nm -C "$binary" >/tmp/rocgalgame_krkr2_symbols.txt
grep -Fq 'ncbRegistNativeClass<AlphaMovie>' /tmp/rocgalgame_krkr2_symbols.txt
stat -c '%y %s %n' "$binary"
