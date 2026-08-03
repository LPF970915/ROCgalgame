#!/bin/bash
set -euo pipefail

BUILD_DIR="${1:-/mnt/d/Works/ROCgalgame/build/gkd350h-glibc234/krkr2}"
LOG_FILE="${2:-/mnt/d/Works/ROCgalgame/GKD350HUltra/logs/glibc234/krkr2_link_retry.log}"

cd "$BUILD_DIR"
nice -n 19 ionice -c 3 taskset -c 0-2 bash CMakeFiles/krkr2.dir/link.txt >"$LOG_FILE" 2>&1
