#!/bin/sh
set -eu

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
SYSROOT="${SYSROOT:-$SELF_DIR/sysroot_device}"
DEVICE_HOST="${DEVICE_HOST:-}"
SSH_PORT="${SSH_PORT:-22}"
SSH_OPTS="${SSH_OPTS:-}"

if [ -z "$DEVICE_HOST" ]; then
  echo "[gkd_sync] ERROR: DEVICE_HOST is required, for example:"
  echo "[gkd_sync]   DEVICE_HOST=root@192.168.31.12 $0"
  exit 2
fi

if ! command -v rsync >/dev/null 2>&1; then
  echo "[gkd_sync] ERROR: local rsync not found"
  exit 1
fi

if ! command -v ssh >/dev/null 2>&1; then
  echo "[gkd_sync] ERROR: local ssh not found"
  exit 1
fi

SSH_BASE="ssh $SSH_OPTS -p $SSH_PORT -o ConnectTimeout=8"
RSYNC_SSH="$SSH_BASE"

echo "[gkd_sync] target: $DEVICE_HOST"
echo "[gkd_sync] sysroot: $SYSROOT"

if ! $SSH_BASE "$DEVICE_HOST" "uname -m && test -d /usr/lib" >/dev/null; then
  echo "[gkd_sync] ERROR: cannot reach target or target is missing /usr/lib"
  exit 1
fi

HAS_REMOTE_RSYNC=0
if $SSH_BASE "$DEVICE_HOST" "command -v rsync >/dev/null 2>&1"; then
  HAS_REMOTE_RSYNC=1
fi

mkdir -p "$SYSROOT/usr" "$SYSROOT/lib"

sync_dir() {
  remote="$1"
  local_dir="$2"
  if ! $SSH_BASE "$DEVICE_HOST" "test -d '$remote'" >/dev/null 2>&1; then
    echo "[gkd_sync] skip missing: $remote"
    return 0
  fi

  echo "[gkd_sync] $remote -> $local_dir"
  mkdir -p "$local_dir"
  if [ "$HAS_REMOTE_RSYNC" -eq 1 ]; then
    rsync -a --delete -e "$RSYNC_SSH" "${DEVICE_HOST}:${remote}/" "$local_dir/"
  else
    echo "[gkd_sync] remote rsync missing, using tar stream"
    rm -rf "$local_dir"
    mkdir -p "$local_dir"
    $SSH_BASE "$DEVICE_HOST" "cd '$remote' && tar -cf - ." | tar -xf - -C "$local_dir"
  fi
}

sync_dir "/lib" "$SYSROOT/lib"
sync_dir "/usr/lib" "$SYSROOT/usr/lib"
sync_dir "/usr/include" "$SYSROOT/usr/include"
sync_dir "/usr/share/pkgconfig" "$SYSROOT/usr/share/pkgconfig"
sync_dir "/usr/lib/pkgconfig" "$SYSROOT/usr/lib/pkgconfig"

$SSH_BASE "$DEVICE_HOST" "uname -a; ldd --version 2>/dev/null | head -n 1 || true" >"$SYSROOT/target_info.txt" || true

echo "[gkd_sync] done: $SYSROOT"
