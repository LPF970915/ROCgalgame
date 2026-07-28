#!/bin/sh
set -eu

FIFO="${1:?usage: run_krkr2_button_sequence.sh FIFO [delay] [count]}"
START_DELAY="${2:-8}"
COUNT="${3:-3}"

exec 3>"$FIFO"
sleep "$START_DELAY"

index=0
while [ "$index" -lt "$COUNT" ]; do
  printf 'B L 1\n' >&3
  sleep 0.15
  printf 'B L 0\n' >&3
  index=$((index + 1))
  [ "$index" -ge "$COUNT" ] || sleep 3
done

# Keep the writer connected long enough for the input transport to drain.
sleep 5
