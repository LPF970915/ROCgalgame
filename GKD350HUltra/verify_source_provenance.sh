#!/bin/bash
set -euo pipefail

ROOT="${1:-}"
LABEL="${2:-source}"
ALLOW_DIRTY="${ALLOW_DIRTY_SOURCE:-0}"
LOCK_FILE="${3:-}"

if [ -z "$ROOT" ] || [ -z "$LABEL" ]; then
  echo "usage: $0 SOURCE_ROOT LABEL [LOCK_FILE]" >&2
  exit 2
fi
if [ ! -d "$ROOT" ]; then
  echo "[provenance] ERROR: $LABEL source root does not exist: $ROOT" >&2
  exit 1
fi
if ! git -c safe.directory="$ROOT" -C "$ROOT" rev-parse --show-toplevel >/dev/null 2>&1; then
  echo "[provenance] ERROR: $LABEL source is not a git checkout: $ROOT" >&2
  exit 1
fi

commit="$(git -c safe.directory="$ROOT" -C "$ROOT" rev-parse HEAD)"
if [ -n "$LOCK_FILE" ]; then
  if [ ! -s "$LOCK_FILE" ]; then
    echo "[provenance] ERROR: lock file does not exist: $LOCK_FILE" >&2
    exit 1
  fi
  expected_commit="$(sed -n 's/^source_commit=//p' "$LOCK_FILE" | head -n 1)"
  expected_repository="$(sed -n 's/^repository=//p' "$LOCK_FILE" | head -n 1)"
  test "$expected_commit" = "$commit" || {
    echo "[provenance] ERROR: $LABEL commit $commit does not match lock $expected_commit" >&2
    exit 1
  }
  test -n "$expected_repository" || {
    echo "[provenance] ERROR: lock file has no repository entry" >&2
    exit 1
  }
  remote_urls="$(git -c safe.directory="$ROOT" -C "$ROOT" remote get-url --all 2>/dev/null || true)"
  case "$remote_urls" in
    *2468785842/krkr2.git*)
      echo "[provenance] ERROR: $LABEL still has the upstream KRKR2 remote; remove it before building" >&2
      exit 1
      ;;
  esac
fi
# These sources are checked out by Windows Git with core.autocrlf=true and
# built through WSL. Match that checkout policy so line endings do not turn a
# few real edits into a false full-tree dirty report.
dirty_count="$(git -c safe.directory="$ROOT" -c core.autocrlf=true -C "$ROOT" status --porcelain=v1 | wc -l | tr -d ' ')"
printf '[provenance] %s root=%s commit=%s dirty_files=%s\n' \
  "$LABEL" "$ROOT" "$commit" "$dirty_count"
if [ "$dirty_count" -ne 0 ] && [ "$ALLOW_DIRTY" != "1" ]; then
  echo "[provenance] ERROR: $LABEL source checkout is dirty; commit the changes or set ALLOW_DIRTY_SOURCE=1 for an explicitly non-reproducible build" >&2
  git -c safe.directory="$ROOT" -c core.autocrlf=true -C "$ROOT" status --short >&2
  exit 1
fi
