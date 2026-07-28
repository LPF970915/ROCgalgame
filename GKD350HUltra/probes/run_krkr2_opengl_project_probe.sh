#!/bin/sh
set -eu

APP_DIR="${APP_DIR:-/storage/roms/ports/ROCgalgame}"
PROJECT="${PROJECT:?PROJECT must point to an XP3 archive}"
PROJECT_WORKDIR="${PROJECT_WORKDIR:-$(dirname "$PROJECT")}"
PREFERENCE="$PROJECT_WORKDIR/Kirikiroid2Preference.xml"
BACKUP="/tmp/Kirikiroid2Preference.xml.$$.backup"
HAD_PREFERENCE=0

if [ -f "$PREFERENCE" ]; then
  cp -p "$PREFERENCE" "$BACKUP"
  HAD_PREFERENCE=1
fi

restore_preference() {
  if [ "$HAD_PREFERENCE" -eq 1 ]; then
    cp -p "$BACKUP" "$PREFERENCE"
    rm -f "$BACKUP"
  else
    rm -f "$PREFERENCE"
  fi
}
trap restore_preference EXIT HUP INT TERM

cat >"$PREFERENCE" <<'EOF'
<?xml version="1.0"?>
<GlobalPreference>
  <Item key="renderer" value="opengl"/>
</GlobalPreference>
EOF

APP_DIR="$APP_DIR" \
PROJECT="$PROJECT" \
PROJECT_WORKDIR="$PROJECT_WORKDIR" \
TEST_NAME=nekopara-opengl-project-probe \
LOG_DIR="$APP_DIR/logs/nekopara-opengl-project-probe" \
RUN_SECONDS="${RUN_SECONDS:-30}" \
EXPECT_TJS_MARKER= \
REQUIRE_HARDWARE=1 \
ROCGALGAME_KRKR_DISPLAY_BACKEND=xwayland \
ROCGALGAME_KRKR_PRESENTATION_PROBE=1 \
DISPLAY="${DISPLAY:-:3}" \
/bin/sh /tmp/run_krkr2_minimal_test.sh
