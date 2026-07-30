#!/bin/sh
set -eu

bin=/mnt/d/Works/ROCgalgame/build/gkd350h/krkr2/bin/krkr2/krkr2
base=$((0x558e190000))
end=$((0x558e270000))
while [ "$base" -le "$end" ]; do
  printf 'BASE_%x\n' "$base"
  a1=$((0x558e874058 - base))
  a2=$((0x558e873ee4 - base))
  a3=$((0x558e8740b0 - base))
  a4=$((0x558ff36068 - base))
  a5=$((0x558ff49b74 - base))
  a6=$((0x558ff4cec8 - base))
  a7=$((0x558ff4f3fc - base))
  a8=$((0x558ff51914 - base))
  aarch64-linux-gnu-addr2line -f -C -e "$bin" \
    "$(printf '0x%x' "$a1")" "$(printf '0x%x' "$a2")" \
    "$(printf '0x%x' "$a3")" "$(printf '0x%x' "$a4")" \
    "$(printf '0x%x' "$a5")" "$(printf '0x%x' "$a6")" \
    "$(printf '0x%x' "$a7")" "$(printf '0x%x' "$a8")"
  base=$((base + 0x10000))
done
