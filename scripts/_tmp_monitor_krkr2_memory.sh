#!/bin/sh
set -eu

STATE_FILE="${1:-/tmp/rocgalgame-interactive-state}"
SAMPLES="${2:-30}"
INTERVAL="${3:-1}"

pid="$(sed -n 's/^pid=//p' "$STATE_FILE")"
root="$(sed -n 's/^root=//p' "$STATE_FILE")"
echo "pid=$pid root=$root"

i=0
while [ "$i" -lt "$SAMPLES" ]; do
  timestamp="$(date +%H:%M:%S)"
  if [ -r "/proc/$pid/status" ]; then
    memory="$(grep -E '^(VmSize|VmRSS|VmSwap):' "/proc/$pid/status" | tr '\n' ' ')"
    echo "$timestamp $memory"
  else
    echo "$timestamp process-exited"
    break
  fi
  i=$((i + 1))
  sleep "$INTERVAL"
done

echo "--- state ---"
cat "$STATE_FILE"
echo "--- log tail ---"
tail -n 120 "$root/krkr2.log"
echo "--- oom tail ---"
dmesg | tail -40
