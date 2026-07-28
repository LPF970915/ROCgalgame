#!/bin/sh
set -u

app=/storage/roms/ports/ROCgalgame
probe="$app/cache/krkr2-layerex-probe"
mkdir -p "$probe"
cp /tmp/startup.tjs "$probe/startup.tjs"
printf '%s\n' 'title=KRKR2 layerEx compatibility probe' 'core=krkr' 'entry=.' 'frame_limit=30' 'draw_threads=1' > "$probe/game.ini"
chmod 755 "$probe/startup.tjs" "$probe/game.ini" /tmp/run_krkr2_minimal_test.sh

set +e
APP_DIR="$app" PROJECT="$probe" TEST_NAME=krkr2-layerex-probe RUN_SECONDS=12 EXPECT_TJS_MARKER=PLUGIN_COMPAT_OK /tmp/run_krkr2_minimal_test.sh
status=$?
echo "probe_script_status=$status"
exit 0
