#!/bin/sh
set -eu

PROBE="/tmp/krkr2_device_interactive_probe.sh"
STATE="/tmp/rocgalgame-interactive-state"
GAME="/storage/games-external/app/ROCgalgame/games/Criss Cross"
OUTPUT="/tmp/criss-krkr2-maps.txt"
old_root="$(sed -n 's/^root=//p' "$STATE" 2>/dev/null || true)"

nohup "$PROBE" "$GAME" data.xp3 criss-map-capture \
  >/tmp/criss-map-capture-probe.out 2>&1 </dev/null &

i=0
while [ "$i" -lt 30 ]; do
  root="$(sed -n 's/^root=//p' "$STATE" 2>/dev/null || true)"
  pid="$(sed -n 's/^pid=//p' "$STATE" 2>/dev/null || true)"
  if [ -n "$root" ] && [ "$root" != "$old_root" ] &&
     [ -n "$pid" ] && [ -r "/proc/$pid/maps" ]; then
    cat "/proc/$pid/maps" >"$OUTPUT"
    kill -KILL "$pid" 2>/dev/null || true
    echo "pid=$pid root=$root maps=$OUTPUT"
    exit 0
  fi
  i=$((i + 1))
  sleep 0.1
done

echo "failed to capture KRKR2 maps" >&2
exit 1
