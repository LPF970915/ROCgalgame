#!/bin/sh
set -eu

APP_DIR="${APP_DIR:-/storage/roms/ports/ROCgalgame}"
PROJECT="${PROJECT:-/tmp/krkr2-minimal-swap}"
FIFO="${FIFO:-/tmp/rocgalgame-krkr2-pointer-probe.fifo}"
RUN_SECONDS="${RUN_SECONDS:-15}"
TEST_NAME="${TEST_NAME:-krkr2-pointer-probe}"

cleanup() {
  test -z "${writer_pid:-}" || kill "$writer_pid" 2>/dev/null || true
  rm -f "$FIFO"
}
trap cleanup EXIT INT TERM

rm -f "$FIFO"
mkfifo "$FIFO"

(
  exec 3>"$FIFO"
  sequence=1
  while [ "$sequence" -le 1200 ]; do
    printf 'A 0.70 0.20 %s\n' "$sequence" >&3
    sequence=$((sequence + 1))
    sleep 0.016
  done
) &
writer_pid=$!

export ROCGALGAME_KRKR_INPUT_FIFO="$FIFO"
export PROJECT RUN_SECONDS TEST_NAME
/bin/sh "$APP_DIR/run_krkr2_hardware_test.sh"
