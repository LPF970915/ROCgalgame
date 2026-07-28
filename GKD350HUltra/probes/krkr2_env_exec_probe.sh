#!/bin/sh
set -eu

LOG_FILE="${ROCGALGAME_KRKR_ENV_PROBE_LOG:-/tmp/krkr2-env-probe.log}"
DELAY_SECONDS="${ROCGALGAME_KRKR_ENV_PROBE_DELAY:-0}"
REAL_CORE="${ROCGALGAME_KRKR_ENV_PROBE_CORE:-/storage/roms/ports/ROCgalgame/cores/krkr/krkr2}"
PRELOAD="${ROCGALGAME_KRKR_ENV_PROBE_PRELOAD:-}"

{
  echo "argv0=$0"
  index=1
  for argument in "$@"; do
    echo "argv${index}=$argument"
    index=$((index + 1))
  done
  env | sort
} >"$LOG_FILE"

if [ "$DELAY_SECONDS" != "0" ]; then
  sleep "$DELAY_SECONDS"
fi

if [ -n "$PRELOAD" ]; then
  LD_PRELOAD="$PRELOAD${LD_PRELOAD:+:$LD_PRELOAD}"
  export LD_PRELOAD
fi

exec "$REAL_CORE" "$@"
