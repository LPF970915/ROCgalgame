#!/bin/bash
set -euo pipefail

source_binary=/mnt/d/Works/ROCgalgame/build/gkd350h/krkr2/bin/krkr2/krkr2
dist_binary=/mnt/d/Works/ROCgalgame/GKD350HUltra/dist_lowglibc/ROCgalgame/cores/krkr/krkr2

cp "$source_binary" "$dist_binary"
chmod 755 "$dist_binary"
aarch64-linux-gnu-strip --strip-unneeded "$dist_binary"
file "$dist_binary"
sha256sum "$dist_binary"
readelf -d "$dist_binary" | grep -E 'NEEDED|RUNPATH|RPATH'
