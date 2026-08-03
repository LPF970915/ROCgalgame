#!/bin/sh
set -eu

STATE_FILE="${1:-/tmp/rocgalgame-interactive-state}"
LIMIT_KB="${2:-600000}"
OUTPUT="${3:-/tmp/krkr2-rss-watchdog.txt}"
ACTION="${WATCHDOG_ACTION:-kill}"

pid="$(sed -n 's/^pid=//p' "$STATE_FILE")"
root="$(sed -n 's/^root=//p' "$STATE_FILE")"
echo "pid=$pid root=$root limit_kb=$LIMIT_KB" >"$OUTPUT"
cat "/proc/$pid/maps" >"${OUTPUT}.maps"

while [ -r "/proc/$pid/status" ]; do
  rss="$(sed -n 's/^VmRSS:[[:space:]]*\([0-9][0-9]*\).*/\1/p' "/proc/$pid/status")"
  swap="$(sed -n 's/^VmSwap:[[:space:]]*\([0-9][0-9]*\).*/\1/p' "/proc/$pid/status")"
  echo "$(date +%H:%M:%S) rss_kb=${rss:-0} swap_kb=${swap:-0}" >>"$OUTPUT"
  if [ -n "$rss" ] && [ "$rss" -ge "$LIMIT_KB" ]; then
    case "$ACTION" in
      stop)
        kill -STOP "$pid"
        echo "stopped=1" >>"$OUTPUT"
        ;;
      *)
        kill -TERM "$pid" 2>/dev/null || true
        sleep 1
        kill -KILL "$pid" 2>/dev/null || true
        echo "killed=1" >>"$OUTPUT"
        ;;
    esac
    cat "/proc/$pid/status" >>"$OUTPUT"
    exit 0
  fi
  sleep 0.2
done

echo "process_exited=1" >>"$OUTPUT"
exit 1
