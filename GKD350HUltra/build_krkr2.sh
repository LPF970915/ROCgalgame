#!/bin/bash
set -euo pipefail

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SELF_DIR/.." && pwd)"
BUILD_ROOT="${KRKR2_BUILD_ROOT:-$REPO_ROOT/build/gkd350h-glibc234}"
KRKR2_ROOT="${KRKR2_ROOT:-/mnt/d/Works/ROCgalgame-krkr2-port}"
SYSROOT="${SYSROOT:-$BUILD_ROOT/sysroot}"
TOOLCHAIN="$SELF_DIR/toolchain/aarch64-gkd-krkr2.cmake"
TRIPLET_DIR="$SELF_DIR/vcpkg-triplets"
OVERLAY_PORTS="$SELF_DIR/vcpkg-ports;$KRKR2_ROOT/vcpkg/ports"
TRIPLET="${KRKR2_TARGET_TRIPLET:-arm64-linux-gkd-glibc234}"
PROBE_SOURCE="$SELF_DIR/probes/krkr2_toolchain"
PROBE_BUILD_DIR="${KRKR2_PROBE_BUILD_DIR:-$BUILD_ROOT/krkr2-toolchain-probe}"
BUILD_DIR="${KRKR2_BUILD_DIR:-$BUILD_ROOT/krkr2}"
DIST_ROOT="${DIST_ROOT:-$SELF_DIR/dist_glibc234}"
RUNTIME_CORE_DIR="$DIST_ROOT/ROCgalgame/cores/krkr"
KRKR2_PORT_LOCK="${KRKR2_PORT_LOCK:-$REPO_ROOT/GKD350HUltra/krkr2-port.lock}"
LOG_DIR="${ROC_NATIVE_LOG_DIR:-$SELF_DIR/logs}"
FMOD_STUB_SOURCE="$SELF_DIR/compat/fmod_stub.cpp"
FMOD_STUB_BUILD_DIR="$BUILD_DIR/compat/fmod_stub"
MODE="${KRKR2_BUILD_MODE:-Probe}"
BUILD_JOBS="${KRKR2_BUILD_JOBS:-3}"
SAFE_CPU_SET="${KRKR2_SAFE_CPU_SET:-0-2}"
WORK_SECONDS="${KRKR2_WORK_SECONDS:-300}"
COOL_SECONDS="${KRKR2_COOL_SECONDS:-240}"
PERIODIC_COOLING="${KRKR2_PERIODIC_COOLING:-1}"
NICE_LEVEL="${KRKR2_NICE_LEVEL:-15}"
IO_PRIORITY="${KRKR2_IO_PRIORITY:-7}"
CMAKE_BIN="${CMAKE_BIN:-$SELF_DIR/tools/cmake/bin/cmake}"
WAYLAND_PKG_CONFIG_DIR="$SELF_DIR/pkgconfig-wayland"
USE_CCACHE="${KRKR2_USE_CCACHE:-Auto}"
LINKER="${KRKR2_LINKER:-Auto}"
CCACHE_DIR="${KRKR2_CCACHE_DIR:-$BUILD_ROOT/ccache/krkr2}"
BINARY_CACHE_ONLY="${KRKR2_BINARY_CACHE_ONLY:-0}"
MANIFEST_INSTALL="${KRKR2_MANIFEST_INSTALL:-1}"
DEPENDENCY_LOCK="${KRKR2_DEPENDENCY_LOCK:-$REPO_ROOT/GKD350HUltra/krkr2-vcpkg-dependencies.lock.json}"
MAX_RECOMPILE="${KRKR2_MAX_RECOMPILE:-20}"
CHECK_ONLY="${KRKR2_CHECK_ONLY:-0}"

export GKD_SYSROOT="$SYSROOT"
export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"
export PKG_CONFIG_LIBDIR="$WAYLAND_PKG_CONFIG_DIR:$SYSROOT/usr/lib/pkgconfig:$SYSROOT/usr/lib/aarch64-linux-gnu/pkgconfig:$SYSROOT/usr/share/pkgconfig"

case "$BUILD_JOBS" in
  ''|*[!0-9]*) echo "[krkr2_build] ERROR: KRKR2_BUILD_JOBS must be an integer from 1 to 4"; exit 2 ;;
esac
if [ "$BUILD_JOBS" -lt 1 ] || [ "$BUILD_JOBS" -gt 4 ]; then
  echo "[krkr2_build] ERROR: KRKR2_BUILD_JOBS must remain between 1 and 4"
  exit 2
fi

case "$MODE" in
  Probe|Configure|Build|FastBuild|Full) ;;
  *) echo "[krkr2_build] ERROR: KRKR2_BUILD_MODE must be Probe, Configure, Build, FastBuild, or Full"; exit 2 ;;
esac
case "$USE_CCACHE" in Auto|On|Off) ;; *)
  echo "[krkr2_build] ERROR: KRKR2_USE_CCACHE must be Auto, On, or Off"; exit 2 ;;
esac
case "$LINKER" in Auto|mold|lld|bfd) ;; *)
  echo "[krkr2_build] ERROR: KRKR2_LINKER must be Auto, mold, lld, or bfd"; exit 2 ;;
esac
case "$BINARY_CACHE_ONLY" in 0|1) ;; *)
  echo "[krkr2_build] ERROR: KRKR2_BINARY_CACHE_ONLY must be 0 or 1"; exit 2 ;;
esac
case "$MANIFEST_INSTALL" in 0|1) ;; *)
  echo "[krkr2_build] ERROR: KRKR2_MANIFEST_INSTALL must be 0 or 1"; exit 2 ;;
esac
case "$MAX_RECOMPILE" in
  ''|*[!0-9]*) echo "[krkr2_build] ERROR: KRKR2_MAX_RECOMPILE must be a non-negative integer"; exit 2 ;;
esac
case "$CHECK_ONLY" in 0|1) ;; *)
  echo "[krkr2_build] ERROR: KRKR2_CHECK_ONLY must be 0 or 1"; exit 2 ;;
esac

test -x "$CMAKE_BIN" || { echo "[krkr2_build] ERROR: bundled CMake is missing: $CMAKE_BIN"; exit 1; }
command -v aarch64-linux-gnu-g++ >/dev/null 2>&1 || {
  echo "[krkr2_build] ERROR: aarch64-linux-gnu-g++ is required"
  exit 1
}
command -v make >/dev/null 2>&1 || { echo "[krkr2_build] ERROR: make is required"; exit 1; }
command -v taskset >/dev/null 2>&1 || { echo "[krkr2_build] ERROR: taskset is required"; exit 1; }
command -v ionice >/dev/null 2>&1 || { echo "[krkr2_build] ERROR: ionice is required"; exit 1; }
command -v setsid >/dev/null 2>&1 || { echo "[krkr2_build] ERROR: setsid is required for cooling pauses"; exit 1; }
command -v flock >/dev/null 2>&1 || { echo "[krkr2_build] ERROR: flock is required to serialize build-tree access"; exit 1; }
test -f "$TOOLCHAIN" || { echo "[krkr2_build] ERROR: toolchain is missing: $TOOLCHAIN"; exit 1; }
test -d "$SYSROOT/usr/include" || { echo "[krkr2_build] ERROR: invalid sysroot: $SYSROOT"; exit 1; }
test -f "$WAYLAND_PKG_CONFIG_DIR/wayland-client.pc" || {
  echo "[krkr2_build] ERROR: Wayland pkg-config overlay is missing"
  exit 1
}

case "$WORK_SECONDS:$COOL_SECONDS" in
  *[!0-9:]*) echo "[krkr2_build] ERROR: cooling intervals must be integer seconds"; exit 2 ;;
esac
if [ "$WORK_SECONDS" -lt 60 ] || [ "$WORK_SECONDS" -gt 1800 ]; then
  echo "[krkr2_build] ERROR: KRKR2_WORK_SECONDS must remain between 60 and 1800"
  exit 2
fi
if [ "$COOL_SECONDS" -lt 15 ] || [ "$COOL_SECONDS" -gt 900 ]; then
  echo "[krkr2_build] ERROR: KRKR2_COOL_SECONDS must remain between 15 and 900"
  exit 2
fi

case "$PERIODIC_COOLING" in
  0|1) ;;
  *) echo "[krkr2_build] ERROR: KRKR2_PERIODIC_COOLING must be 0 or 1"; exit 2 ;;
esac

case "$NICE_LEVEL:$IO_PRIORITY" in
  *[!0-9:]*) echo "[krkr2_build] ERROR: scheduler priorities must be integers"; exit 2 ;;
esac
if [ "$NICE_LEVEL" -gt 19 ] || [ "$IO_PRIORITY" -gt 7 ]; then
  echo "[krkr2_build] ERROR: scheduler priorities are outside their supported ranges"
  exit 2
fi

run_low_load() {
  if [ "$PERIODIC_COOLING" = "0" ]; then
    setsid --wait nice -n "$NICE_LEVEL" ionice -c 2 -n "$IO_PRIORITY" \
      taskset -c "$SAFE_CPU_SET" "$@"
    return
  fi

  setsid nice -n "$NICE_LEVEL" ionice -c 2 -n "$IO_PRIORITY" \
    taskset -c "$SAFE_CPU_SET" "$@" &
  local command_pid=$!
  local command_status controller_pid

  (
    local timer_pid=""
    trap 'test -z "$timer_pid" || kill "$timer_pid" 2>/dev/null || true; exit 0' TERM INT
    while kill -0 "$command_pid" 2>/dev/null; do
      sleep "$WORK_SECONDS" &
      timer_pid=$!
      wait "$timer_pid" || exit 0
      timer_pid=""
      kill -0 "$command_pid" 2>/dev/null || exit 0

      echo "[krkr2_build] cooling pause: ${COOL_SECONDS}s after ${WORK_SECONDS}s at full ${BUILD_JOBS}-job speed"
      ps -o pid= --sid "$command_pid" | xargs -r kill -STOP || true
      sleep "$COOL_SECONDS" &
      timer_pid=$!
      wait "$timer_pid" || exit 0
      timer_pid=""
      ps -o pid= --sid "$command_pid" | xargs -r kill -CONT || true
      echo "[krkr2_build] cooling pause complete; resuming at full ${BUILD_JOBS}-job speed"
    done
  ) &
  controller_pid=$!

  set +e
  wait "$command_pid"
  command_status=$?
  set -e

  kill "$controller_pid" 2>/dev/null || true
  ps -o pid= --sid "$command_pid" | xargs -r kill -CONT || true
  wait "$controller_pid" 2>/dev/null || true
  return "$command_status"
}

mkdir -p "$LOG_DIR"
LOG_FILE="$LOG_DIR/build_krkr2_${MODE}_$(date +%Y%m%d_%H%M%S).log"

if [ "$MODE" != "Probe" ]; then
  mkdir -p "$BUILD_ROOT"
  BUILD_LOCK="$BUILD_ROOT/.krkr2-build.lock"
  exec 9>"$BUILD_LOCK"
  if ! flock -n 9; then
    echo "[krkr2_build] ERROR: another KRKR2 build already owns $BUILD_LOCK"
    echo "[krkr2_build] Wait for it to finish or inspect the owning WSL process before retrying."
    exit 4
  fi
fi

run_probe() {
  mkdir -p "$PROBE_BUILD_DIR"
  run_low_load "$CMAKE_BIN" --fresh -S "$PROBE_SOURCE" -B "$PROBE_BUILD_DIR" -G "Unix Makefiles" \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DGKD_SYSROOT="$SYSROOT" \
    -DCMAKE_BUILD_TYPE=Release
  run_low_load "$CMAKE_BIN" --build "$PROBE_BUILD_DIR" --parallel 1
  PROBE_BINARY="$PROBE_BUILD_DIR/krkr2_toolchain_probe"
  test -x "$PROBE_BINARY" || { echo "[krkr2_build] ERROR: probe binary was not produced"; exit 1; }
  file "$PROBE_BINARY"
  file "$PROBE_BINARY" | grep -qE 'ARM aarch64|aarch64' || {
    echo "[krkr2_build] ERROR: probe is not an AArch64 ELF"
    exit 1
  }
  readelf -h "$PROBE_BINARY" | grep -E 'Class:|Machine:'
  echo "[krkr2_build] probe passed: $PROBE_BINARY"
}

if [ "$MODE" = "Probe" ]; then
  run_probe 2>&1 | tee "$LOG_FILE"
  echo "[krkr2_build] log=$LOG_FILE"
  exit 0
fi

if [ "${KRKR2_CONFIRM_HEAVY_BUILD:-0}" != "1" ]; then
  echo "[krkr2_build] REFUSED: KrKr2 configure can build its large vcpkg dependency graph."
  echo "[krkr2_build] Set KRKR2_CONFIRM_HEAVY_BUILD=1 after checking cooling and power."
  exit 3
fi

test -f "$KRKR2_ROOT/CMakeLists.txt" || { echo "[krkr2_build] ERROR: invalid KRKR2_ROOT: $KRKR2_ROOT"; exit 1; }
bash "$SELF_DIR/verify_source_provenance.sh" "$KRKR2_ROOT" krkr2 "$KRKR2_PORT_LOCK"
KRKR2_SOURCE_COMMIT="$(git -c safe.directory="$KRKR2_ROOT" -C "$KRKR2_ROOT" rev-parse HEAD)"
VCPKG_ROOT="${VCPKG_ROOT:-$BUILD_ROOT/vcpkg}"
VCPKG_BINARY_CACHE="${VCPKG_BINARY_CACHE:-$VCPKG_ROOT/binary-cache}"
mkdir -p "$VCPKG_BINARY_CACHE"
export VCPKG_BINARY_SOURCES="${VCPKG_BINARY_SOURCES:-clear;files,$VCPKG_BINARY_CACHE,readwrite}"
VCPKG_TOOLCHAIN="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
test -f "$VCPKG_TOOLCHAIN" || {
  echo "[krkr2_build] ERROR: vcpkg is missing: $VCPKG_TOOLCHAIN"
  echo "[krkr2_build] Restore the glibc 2.34 baseline cache or set VCPKG_ROOT."
  exit 1
}

if [ "$MODE" = "Full" ]; then
  case "$BUILD_DIR" in
    "$REPO_ROOT"/build/gkd350h-glibc234/krkr2|"$REPO_ROOT"/build/gkd350h-glibc234/krkr2/*) ;;
    *) echo "[krkr2_build] ERROR: refusing to clean unexpected directory: $BUILD_DIR"; exit 1 ;;
  esac
  rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR" "$RUNTIME_CORE_DIR"
export VCPKG_ROOT
export GKD_SYSROOT="$SYSROOT"
export VCPKG_MAX_CONCURRENCY="$BUILD_JOBS"
export CMAKE_BUILD_PARALLEL_LEVEL="$BUILD_JOBS"
export MAKEFLAGS="-j$BUILD_JOBS"
export NINJAFLAGS="-j$BUILD_JOBS"
export OMP_NUM_THREADS="$BUILD_JOBS"
export OMP_THREAD_LIMIT="$BUILD_JOBS"
export OPENBLAS_NUM_THREADS="$BUILD_JOBS"
export MKL_NUM_THREADS="$BUILD_JOBS"
export NUMEXPR_NUM_THREADS="$BUILD_JOBS"

CMAKE_ACCEL_ARGS=()
CCACHE_BIN=""
if [ "$USE_CCACHE" != "Off" ]; then
  CCACHE_BIN="$(command -v ccache 2>/dev/null || true)"
  if [ -n "$CCACHE_BIN" ]; then
    mkdir -p "$CCACHE_DIR"
    export CCACHE_DIR CCACHE_BASEDIR="$KRKR2_ROOT"
    export CCACHE_COMPILERCHECK=content
    CMAKE_ACCEL_ARGS+=(
      "-DCMAKE_C_COMPILER_LAUNCHER=$CCACHE_BIN"
      "-DCMAKE_CXX_COMPILER_LAUNCHER=$CCACHE_BIN"
    )
  elif [ "$USE_CCACHE" = "On" ]; then
    echo "[krkr2_build] ERROR: KRKR2_USE_CCACHE=On but ccache is unavailable"
    exit 1
  fi
fi

LINKER_FLAG=""
case "$LINKER" in
  Auto)
    if command -v mold >/dev/null 2>&1; then LINKER=mold
    elif command -v ld.lld >/dev/null 2>&1; then LINKER=lld
    else LINKER=bfd
    fi
    ;;
esac
case "$LINKER" in
  mold)
    if ! command -v aarch64-linux-gnu-ld.mold >/dev/null 2>&1; then
      ln -sf "$(command -v mold)" /usr/local/bin/aarch64-linux-gnu-ld.mold
    fi
    LINKER_FLAG="-fuse-ld=mold"
    ;;
  lld)
    if ! command -v aarch64-linux-gnu-ld.lld >/dev/null 2>&1; then
      ln -sf "$(command -v ld.lld)" /usr/local/bin/aarch64-linux-gnu-ld.lld
    fi
    LINKER_FLAG="-fuse-ld=lld"
    ;;
  bfd) LINKER_FLAG="" ;;
esac

prepare_sysroot_x11_pkgconfig_shims() {
  local installed="$BUILD_DIR/vcpkg_installed/$TRIPLET"
  local pcdir spec module display library version
  for pcdir in "$installed/lib/pkgconfig" "$installed/debug/lib/pkgconfig"; do
    mkdir -p "$pcdir"
    for spec in \
      'x11|X11|X11|1.8.7' \
      'xau|Xau|Xau|1.0.9' \
      'xdmcp|Xdmcp|Xdmcp|1.1.3' \
      'xcb|XCB|xcb|1.15' \
      'xext|Xext|Xext|1.3.4' \
      'xfixes|Xfixes|Xfixes|6.0.0' \
      'xi|Xi|Xi|1.8' \
      'xrandr|Xrandr|Xrandr|1.5.2' \
      'xrender|Xrender|Xrender|0.9.10' \
      'xtst|Xtst|Xtst|1.2.3' \
      'glu|GLU|GLU|9.0.2'; do
      IFS='|' read -r module display library version <<< "$spec"
      printf '%s\n' \
        "prefix=$SYSROOT" \
        'exec_prefix=${prefix}' \
        'libdir=${prefix}/lib' \
        'includedir=${prefix}/usr/include' \
        "Name: $display" \
        "Description: $display from the GKD350HUltra target sysroot" \
        "Version: $version" \
        "Libs: -L\${libdir} -l$library" \
        'Cflags: -I${includedir}' \
        > "$pcdir/$module.pc"
    done
  done
}

prepare_sysroot_x11_pkgconfig_shims

verify_rocgalgame_source_patches() {
  local app_delegate="$KRKR2_ROOT/cpp/core/environ/cocos2d/AppDelegate.cpp"
  grep -Fq 'GLContextAttrs glContextAttrs = { 8, 8, 8, 8, 24, 0 };' "$app_delegate" || {
    echo "[krkr2_build] ERROR: KRKR2 Mali ARGB surface patch is missing"
    exit 1
  }
  local wayland_surface_patch="$KRKR2_ROOT/vcpkg/ports/cocos2dx/patch/rocgalgame-wayland-transparent-framebuffer.patch"
  test -f "$wayland_surface_patch" &&
    grep -Fq 'GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE' "$wayland_surface_patch" || {
      echo "[krkr2_build] ERROR: KRKR2 Wayland transparent framebuffer patch is missing"
      exit 1
    }
  local scene="$KRKR2_ROOT/cpp/core/environ/cocos2d/MainScene.cpp"
  local transport="$KRKR2_ROOT/cpp/core/environ/cocos2d/RocgalgameInputTransport.cpp"
  local render_utils="$KRKR2_ROOT/cpp/core/environ/RenderUtils.h"
  grep -q "type == 'A'" "$transport" || {
    echo "[krkr2_build] ERROR: latest-axis frontend bridge patch is missing"
    exit 1
  }
  grep -Fq 'if(fd >= 0 && (descriptor.revents & (POLLHUP | POLLERR))) {' "$transport" || {
    echo "[krkr2_build] ERROR: FIFO HUP backoff patch is missing"
    exit 1
  }
  grep -Fq 'deliveredAxisSequence = 0;' "$transport" || {
    echo "[krkr2_build] ERROR: FIFO reconnect sequence reset patch is missing"
    exit 1
  }
  grep -Fq 'std::min(delta, 0.1f)' "$scene" || {
    echo "[krkr2_build] ERROR: engine-delta pointer integration patch is missing"
    exit 1
  }
  grep -Fq 'TVPGetPostUpdateEvent()' "$render_utils" || {
    echo "[krkr2_build] ERROR: shared post-update FBO restore patch is missing"
    exit 1
  }
  grep -Fq 'TVPRunPostUpdateEvent();' "$scene" || {
    echo "[krkr2_build] ERROR: per-frame FBO restore callback is missing"
    exit 1
  }
  grep -Fq 'logger->info("[rocgalgame] perf fps=' "$scene" || {
    echo "[krkr2_build] ERROR: KRKR2 release performance diagnostics patch is missing"
    exit 1
  }
  grep -Fq 'logger->info("[rocgalgame] frontend input bridge ready:' "$scene" || {
    echo "[krkr2_build] ERROR: KRKR2 release input diagnostics patch is missing"
    exit 1
  }
  grep -q 'if(!rocgalgameRuntime)' "$scene" || {
    echo "[krkr2_build] ERROR: console redraw suppression patch is missing"
    exit 1
  }
  local ogl_renderer="$KRKR2_ROOT/cpp/core/visual/ogl/RenderManager_ogl.cpp"
  grep -Fq '_RestoreGLStatues();' "$ogl_renderer" || {
    echo "[krkr2_build] ERROR: KRKR2 GPU presentation restore patch is missing"
    exit 1
  }
  grep -Fq 'update(this, texture)' "$ogl_renderer" || {
    echo "[krkr2_build] ERROR: KRKR2 adapter texture ownership patch is missing"
    exit 1
  }
  grep -Fq 'ROCGALGAME_KRKR_MALI_FAST_PATHS' "$ogl_renderer" || {
    echo "[krkr2_build] ERROR: KRKR2 Mali safe-render patch is missing"
    exit 1
  }
  grep -Fq 'ROCGALGAME_KRKR_PRESENTATION_PROBE' "$ogl_renderer" || {
    echo "[krkr2_build] ERROR: KRKR2 presentation probe patch is missing"
    exit 1
  }
  grep -Fq 'ROCGALGAME_KRKR_PRESENTATION_CAPTURE_REQUEST' "$ogl_renderer" || {
    echo "[krkr2_build] ERROR: KRKR2 presentation capture patch is missing"
    exit 1
  }
  local main_scene="$KRKR2_ROOT/cpp/core/environ/cocos2d/MainScene.cpp"
  grep -Fq 'TVPProcessPresentationCaptureRequest();' "$main_scene" || {
    echo "[krkr2_build] ERROR: KRKR2 presentation capture tick is missing"
    exit 1
  }
  grep -Fq 'ROCGALGAME_KRKR_POINTER_REQUEST' "$main_scene" || {
    echo "[krkr2_build] ERROR: KRKR2 pointer request test hook is missing"
    exit 1
  }
  local motion_internal="$KRKR2_ROOT/cpp/plugins/motionplayer/PlayerInternal.h"
  local motion_update_internal="$KRKR2_ROOT/cpp/plugins/motionplayer/PlayerUpdateLayersInternal.h"
  grep -Fq 'bool decodePixels = true' "$motion_internal" || {
    echo "[krkr2_build] ERROR: KRKR2 motion source metadata-only patch is missing"
    exit 1
  }
  grep -Fq 'ignoredPixels, srcOX, srcOY, nullptr,' "$motion_update_internal" || {
    echo "[krkr2_build] ERROR: KRKR2 motion geometry metadata-only call is missing"
    exit 1
  }
  local motion_source_cache="$KRKR2_ROOT/cpp/plugins/motionplayer/SourceCache.cpp"
  grep -Fq 'textureCacheColors' "$motion_source_cache" || {
    echo "[krkr2_build] ERROR: KRKR2 motion texture cache patch is missing"
    exit 1
  }
  grep -Fq 'entry.backingBitmap.reset();' "$motion_source_cache" || {
    echo "[krkr2_build] ERROR: KRKR2 motion texture CPU release patch is missing"
    exit 1
  }
  grep -Fq 'missingBitmapSentinel' "$motion_source_cache" || {
    echo "[krkr2_build] ERROR: KRKR2 motion missing-source cache patch is missing"
    exit 1
  }
  [ "$(grep -Fc 'entry->sourceObject.Type() == tvtInteger' "$motion_source_cache")" -ge 2 ] || {
    echo "[krkr2_build] ERROR: KRKR2 motion missing-source fast-hit patch is missing"
    exit 1
  }
  local motion_render_targets="$KRKR2_ROOT/cpp/plugins/motionplayer/PlayerRenderTargets.cpp"
  grep -Fq 'if(!sourceTexture) {' "$motion_render_targets" || {
    echo "[krkr2_build] ERROR: KRKR2 motion missing-source queue patch is missing"
    exit 1
  }
  local motion_plugin="$KRKR2_ROOT/cpp/plugins/motionplayer/main.cpp"
  grep -Fq 'setEmoteCameraOffsetCompat' "$motion_plugin" || {
    echo "[krkr2_build] ERROR: KRKR2 Motion setCameraOffset compatibility patch is missing"
    exit 1
  }
  local motion_variable="$KRKR2_ROOT/cpp/plugins/motionplayer/PlayerVariable.cpp"
  grep -Fq 'parameterPropagation.visited.insert(this)' "$motion_variable" || {
    echo "[krkr2_build] ERROR: KRKR2 Motion parameter cycle guard is missing"
    exit 1
  }
  local motion_resource_manager="$KRKR2_ROOT/cpp/plugins/motionplayer/ResourceManager.cpp"
  grep -Fq 'SetMotionPSBDecryptCallback(_decryptFunc);' "$motion_resource_manager" || {
    echo "[krkr2_build] ERROR: KRKR2 Motion PSB decrypt callback bridge is missing"
    exit 1
  }
  local render_manager="$KRKR2_ROOT/cpp/core/visual/RenderManager.cpp"
  grep -Fq 'ROCGALGAME_KRKR_RENDERER' "$render_manager" || {
    echo "[krkr2_build] ERROR: ROCgalgame OpenGL renderer default patch is missing"
    exit 1
  }
  local psb_plugin="$KRKR2_ROOT/cpp/plugins/psbfile/main.cpp"
  grep -Fq 'registerRootResources(path, *self);' "$psb_plugin" || {
    echo "[krkr2_build] ERROR: KRKR2 PSB load-safety patch is missing"
    exit 1
  }
  local psb_file="$KRKR2_ROOT/cpp/plugins/psbfile/PSBFile.cpp"
  grep -Fq 'ignoring invalid PSB v4 extra chunk offsets' "$psb_file" || {
    echo "[krkr2_build] ERROR: KRKR2 PSB v4 compatibility patch is missing"
    exit 1
  }
  grep -Fq 'tTJSVariant motionDecryptCallback;' "$psb_file" || {
    echo "[krkr2_build] ERROR: KRKR2 PSB decrypt callback lifetime fix is missing"
    exit 1
  }
  local fstat_plugin="$KRKR2_ROOT/cpp/plugins/fstat/main.cpp"
  grep -Fq 'TVPGetLocallyAccessibleName(normalized)' "$fstat_plugin" || {
    echo "[krkr2_build] ERROR: KRKR2 fstat missing-file delete patch is missing"
    exit 1
  }
  local tjs_executor="$KRKR2_ROOT/cpp/core/tjs2/tjsInterCodeExec.cpp"
  grep -Fq 'invalid TJS VM instruction pointer:' "$tjs_executor" || {
    echo "[krkr2_build] ERROR: KRKR2 TJS bytecode-bounds patch is missing"
    exit 1
  }
  local tjs_regexp="$KRKR2_ROOT/cpp/core/tjs2/tjsRegExp.cpp"
  grep -Fq 'TJSNormalizeLegacyRegExpHexEscapes' "$tjs_regexp" || {
    echo "[krkr2_build] ERROR: KRKR2 TJS legacy RegExp hex patch is missing"
    exit 1
  }
  local alpha_movie_plugin="$KRKR2_ROOT/cpp/plugins/AlphaMovie.cpp"
  grep -Fq 'NCB_MODULE_NAME TJS_W("AlphaMovie.dll")' "$alpha_movie_plugin" || {
    echo "[krkr2_build] ERROR: KRKR2 AlphaMovie compatibility plugin is missing"
    exit 1
  }
  local text_stream="$KRKR2_ROOT/cpp/core/base/TextStream.cpp"
  grep -Fq 'constexpr size_t cipherHeaderSize = 5;' "$text_stream" || {
    echo "[krkr2_build] ERROR: KRKR2 encrypted-text header patch is missing"
    exit 1
  }
  local sys_init="$KRKR2_ROOT/cpp/core/base/impl/SysInitImpl.cpp"
  grep -Fq 'std::getenv("ROCGALGAME_KRKR_SAVE_PATH")' "$sys_init" || {
    echo "[krkr2_build] ERROR: KRKR2 isolated-save-path patch is missing"
    exit 1
  }
  local linux_main="$KRKR2_ROOT/platforms/linux/main.cpp"
  grep -Fq 'for(int i = 2; i < argc; ++i)' "$linux_main" || {
    echo "[krkr2_build] ERROR: KRKR2 Linux command-line patch is missing"
    exit 1
  }
}

prepare_fmod_stub() {
  local fmod_root="$BUILD_DIR/vcpkg_installed/$TRIPLET/share/cocos2dx/linux-specific/fmod"
  local fmod_include="$fmod_root/include"
  local fmod_link_path="vcpkg_installed/$TRIPLET/share/cocos2dx/linux-specific/fmod/prebuilt/64-bit/libfmod.so"
  local stub_object="$FMOD_STUB_BUILD_DIR/fmod_stub.cpp.o"
  local stub_archive="$FMOD_STUB_BUILD_DIR/libfmod_stub.a"
  local math_abi_source="$FMOD_STUB_BUILD_DIR/glibc234_math_abi.c"
  local math_abi_object="$FMOD_STUB_BUILD_DIR/glibc234_math_abi.o"
  local systemd_link_library="$SYSROOT/lib/aarch64-linux-gnu/libsystemd.so.0.32.0"
  local mali_link_library="$SYSROOT/lib/libmali.so"
  local link_file="$BUILD_DIR/CMakeFiles/krkr2.dir/link.txt"
  local patched_link_file="$link_file.rocgalgame.tmp"
  local alpha_movie_object="CMakeFiles/krkr2.dir/cpp/plugins/AlphaMovie.cpp.o"
  local alpha_movie_object_path="$BUILD_DIR/$alpha_movie_object"

  test -f "$FMOD_STUB_SOURCE" || {
    echo "[krkr2_build] ERROR: FMOD stub source is missing: $FMOD_STUB_SOURCE"
    exit 1
  }
  test -e "$mali_link_library" || {
    echo "[krkr2_build] ERROR: Mali GLES implementation is missing: $mali_link_library"
    exit 1
  }
  if [ "$CHECK_ONLY" = "1" ]; then
    test -f "$stub_archive" && [ ! "$FMOD_STUB_SOURCE" -nt "$stub_archive" ] || {
      echo "[krkr2_build] ERROR: cached FMOD compatibility stub is missing or stale"
      exit 1
    }
    test -f "$math_abi_object" || {
      echo "[krkr2_build] ERROR: cached glibc compatibility object is missing"
      exit 1
    }
    test -f "$alpha_movie_object_path" || {
      echo "[krkr2_build] ERROR: cached AlphaMovie object is missing"
      exit 1
    }
    test -f "$systemd_link_library" || {
      echo "[krkr2_build] ERROR: systemd compatibility library is missing"
      exit 1
    }
    test -f "$link_file" &&
      grep -Fq "$stub_archive" "$link_file" &&
      grep -Fq "$math_abi_object" "$link_file" &&
      grep -Fq "$systemd_link_library" "$link_file" &&
      grep -Fq "$alpha_movie_object" "$link_file" &&
      grep -Fq -- '-lSDL2 -lmali' "$link_file" || {
        echo "[krkr2_build] ERROR: cached KRKR2 link command is not fully patched"
        exit 1
      }
    echo "[krkr2_build] verified cached compatibility objects and link command"
    return
  fi

  mkdir -p "$FMOD_STUB_BUILD_DIR"
  if [ ! -f "$stub_archive" ] || [ "$FMOD_STUB_SOURCE" -nt "$stub_archive" ]; then
    test -f "$fmod_include/fmod.hpp" || {
      echo "[krkr2_build] ERROR: rebuilding the FMOD stub requires headers: $fmod_include"
      exit 1
    }
    echo "[krkr2_build] building AArch64 Cocos FMOD compatibility stub"
    aarch64-linux-gnu-g++ --sysroot="$SYSROOT" -std=c++11 -O2 -fPIC \
      -I"$fmod_include" -c "$FMOD_STUB_SOURCE" -o "$stub_object"
    aarch64-linux-gnu-ar rcs "$stub_archive" "$stub_object"
  fi

  if [ -f "$link_file" ]; then
    # Cocos2d-x 3.17 ships only an x86_64 Linux FMOD binary. KRKR2 uses its
    # own OpenAL backend, so keep the Cocos UI audio ABI as a no-op on AArch64.
    # CMake may regenerate link.txt during --check-build-system; keep this
    # operation idempotent so it can be applied again immediately before a
    # retry without corrupting the escaped $ORIGIN RPATH.
    sed \
      -e "s# $fmod_link_path##g" \
      -e "s#:$fmod_root/prebuilt/64-bit##g" \
      -e "s#:$BUILD_DIR/vcpkg_installed/$TRIPLET/lib/pkgconfig/../../lib##g" \
      -e "s# $stub_archive##g" \
      -e "s# $math_abi_object##g" \
      -e 's# [^ ]*/compat/fmod_stub/libfmod_stub\.a##g' \
      -e 's# [^ ]*/compat/fmod_stub/glibc234_math_abi\.o##g' \
      -e 's# [^ ]*/libsystemd\.so\.0\.32\.0##g' \
      -e 's# -Wl,--wrap=hypot##g' \
      -e 's# -Wl,--wrap=hypotf##g' \
      -e "s# $SYSROOT/lib/aarch64-linux-gnu/libpthread.a# -lpthread#g" \
      -e "s# $SYSROOT/lib/aarch64-linux-gnu/librt.a# -lrt#g" \
      -e "s# $SYSROOT/lib/aarch64-linux-gnu/libm.a# -lm#g" \
      -e 's# -lmf# -lm#g' \
      -e 's# -lSDL2##g' \
      -e 's# -lmali##g' \
      -e 's#libz\.aESv2#libz.a -lGLESv2#g' \
      -e 's# -lGL\([[:space:]]\|$\)#\1#g' \
      -e "s# -fuse-ld=gold##g" \
      -e '/^[[:space:]]*$/d' \
      "$link_file" > "$patched_link_file"
    # Normalize a malformed Boost.Random archive suffix emitted by some
    # CMake/Boost combinations; the installed package provides the .a file.
    sed -i 's#libboost_random\.af\b#libboost_random.a#g' "$patched_link_file"
    if ! grep -Fq "$alpha_movie_object" "$patched_link_file"; then
      test -f "$alpha_movie_object_path" || {
        echo "[krkr2_build] ERROR: cached AlphaMovie object is missing: $alpha_movie_object_path"
        rm -f "$patched_link_file"
        exit 1
      }
      sed -i \
        "s# CMakeFiles/krkr2.dir/cpp/plugins/fstat/main.cpp.o# $alpha_movie_object CMakeFiles/krkr2.dir/cpp/plugins/fstat/main.cpp.o#" \
        "$patched_link_file"
      grep -Fq "$alpha_movie_object" "$patched_link_file" || {
        echo "[krkr2_build] ERROR: failed to add AlphaMovie object to cached link command"
        rm -f "$patched_link_file"
        exit 1
      }
      echo "[krkr2_build] added cached AlphaMovie compatibility object to link"
    fi
    # The device libGLESv2 shim has no GL exports of its own. With
    # --no-copy-dt-needed-entries the real Mali implementation must be linked
    # explicitly, after every static Cocos archive that references gl*.
    if [ "${ROCGALGAME_GLIBC_BASELINE:-}" = "2.34" ]; then
      cat >"$math_abi_source" <<'EOF'
extern double rocgalgame_hypot_glibc217(double, double);
extern float rocgalgame_hypotf_glibc217(float, float);
extern long rocgalgame_strtol_glibc217(const char *, char **, int);
__asm__(".symver rocgalgame_hypot_glibc217,hypot@GLIBC_2.17");
__asm__(".symver rocgalgame_hypotf_glibc217,hypotf@GLIBC_2.17");
__asm__(".symver rocgalgame_strtol_glibc217,strtol@GLIBC_2.17");
double __wrap_hypot(double x, double y) { return rocgalgame_hypot_glibc217(x, y); }
float __wrap_hypotf(float x, float y) { return rocgalgame_hypotf_glibc217(x, y); }
long __isoc23_strtol(const char *text, char **end, int base) {
  return rocgalgame_strtol_glibc217(text, end, base);
}
EOF
      aarch64-linux-gnu-gcc --sysroot="$SYSROOT" -O2 -fPIC \
        -c "$math_abi_source" -o "$math_abi_object"
      test -f "$systemd_link_library" || {
        echo "[krkr2_build] ERROR: systemd compatibility library is missing: $systemd_link_library"
        rm -f "$patched_link_file"
        exit 1
      }
      sed -i "\$ s#\$# -Wl,--wrap=hypot -Wl,--wrap=hypotf $math_abi_object $systemd_link_library -lm#" "$patched_link_file"
    fi
    sed -i "\$ s#\$# -lSDL2 -lmali $stub_archive#" "$patched_link_file"
    if cmp -s "$link_file" "$patched_link_file"; then
      rm -f "$patched_link_file"
      echo "[krkr2_build] AArch64 FMOD link patch is already current"
    else
      mv "$patched_link_file" "$link_file"
      echo "[krkr2_build] patched AArch64 link with FMOD compatibility stub"
    fi
  fi
}

build_krkr2_target() {
  if [ "$MODE" = "FastBuild" ]; then
    run_low_load "$CMAKE_BIN" --build "$BUILD_DIR" --target krkr2 \
      --parallel "$BUILD_JOBS"
  else
    run_low_load "$CMAKE_BIN" --build "$BUILD_DIR" --parallel "$BUILD_JOBS"
  fi
}

verify_recompile_budget() {
  local initial_preview preview_file previous_preview previous_compiles current_compiles
  local compile_count depend_command round converged compile_pattern
  initial_preview="$(mktemp)"
  preview_file="$(mktemp)"
  previous_preview="$(mktemp)"
  previous_compiles="$(mktemp)"
  current_compiles="$(mktemp)"
  compile_pattern='(ccache[[:space:]]+)?[^ ]*aarch64-linux-gnu-(gcc|g\+\+).* -c '
  set +e
  "$CMAKE_BIN" --build "$BUILD_DIR" --target krkr2 --parallel "$BUILD_JOBS" -- -n \
    >"$initial_preview" 2>&1
  local preview_status=$?
  set -e
  if [ "$preview_status" -ne 0 ]; then
    cat "$initial_preview"
    rm -f "$initial_preview" "$preview_file" "$previous_preview" \
      "$previous_compiles" "$current_compiles"
    echo "[krkr2_build] ERROR: incremental dry-run failed"
    exit "$preview_status"
  fi
  cp "$initial_preview" "$previous_preview"
  grep -E "$compile_pattern" "$initial_preview" | sort -u >"$previous_compiles" || true
  converged=0
  for round in 1 2 3 4 5 6 7 8; do
    while IFS= read -r depend_command; do
      (cd "$BUILD_DIR" && bash -c "$depend_command")
    done < <(grep -E '(^|[[:space:]])gmake .*CMakeFiles/.+\.dir/depend$' "$previous_preview" | sort -u)
    set +e
    "$CMAKE_BIN" --build "$BUILD_DIR" --target krkr2 --parallel "$BUILD_JOBS" -- -n \
      >"$preview_file" 2>&1
    preview_status=$?
    set -e
    if [ "$preview_status" -ne 0 ]; then
      cat "$preview_file"
      rm -f "$initial_preview" "$preview_file" "$previous_preview" \
        "$previous_compiles" "$current_compiles"
      echo "[krkr2_build] ERROR: post-dependency incremental dry-run failed"
      exit "$preview_status"
    fi
    grep -E "$compile_pattern" "$preview_file" | sort -u >"$current_compiles" || true
    if cmp -s "$previous_compiles" "$current_compiles"; then
      converged=1
      break
    fi
    cp "$preview_file" "$previous_preview"
    cp "$current_compiles" "$previous_compiles"
  done
  if [ "$converged" -ne 1 ]; then
    rm -f "$initial_preview" "$preview_file" "$previous_preview" \
      "$previous_compiles" "$current_compiles"
    echo "[krkr2_build] ERROR: dependency dry-run did not converge"
    exit 5
  fi
  compile_count="$(wc -l <"$current_compiles")"
  compile_count="${compile_count//[[:space:]]/}"
  echo "[krkr2_build] incremental dry-run compile_count=$compile_count limit=$MAX_RECOMPILE"
  if [ "$compile_count" -gt "$MAX_RECOMPILE" ]; then
    head -n 20 "$current_compiles"
    rm -f "$initial_preview" "$preview_file" "$previous_preview" \
      "$previous_compiles" "$current_compiles"
    echo "[krkr2_build] REFUSED: incremental compile exceeds KRKR2_MAX_RECOMPILE"
    exit 5
  fi
  rm -f "$initial_preview" "$preview_file" "$previous_preview" \
    "$previous_compiles" "$current_compiles"
}

configure_krkr2() {
  local linker_flags="${ROCGALGAME_GLIBC_BASELINE:+-Wl,--allow-shlib-undefined}"
  linker_flags="${linker_flags}${LINKER_FLAG:+ $LINKER_FLAG}"
  local vcpkg_install_options=""
  if [ "$BINARY_CACHE_ONLY" = "1" ]; then
    vcpkg_install_options="--only-binarycaching"
  fi
  local manifest_install=ON
  [ "$MANIFEST_INSTALL" = "0" ] && manifest_install=OFF
  if [ "$MANIFEST_INSTALL" = "0" ]; then
    test -f "$DEPENDENCY_LOCK" || {
      echo "[krkr2_build] ERROR: dependency lock is missing: $DEPENDENCY_LOCK"; exit 1;
    }
    test -f "$BUILD_DIR/vcpkg_installed/.rocgalgame-dependency-lock.sha256" || {
      echo "[krkr2_build] ERROR: clean materialized dependency tree marker is missing"; exit 1;
    }
    expected_lock_hash="$(sha256sum "$DEPENDENCY_LOCK" | awk '{print $1}')"
    grep -Fq "$expected_lock_hash  $(basename "$DEPENDENCY_LOCK")" \
      "$BUILD_DIR/vcpkg_installed/.rocgalgame-dependency-lock.sha256" || {
      echo "[krkr2_build] ERROR: materialized dependency tree does not match lock"; exit 1;
    }
  fi
  local cmake_mode=()
  [ -f "$BUILD_DIR/CMakeCache.txt" ] || cmake_mode=(--fresh)
  run_low_load "$CMAKE_BIN" "${cmake_mode[@]}" -S "$KRKR2_ROOT" -B "$BUILD_DIR" -G "Unix Makefiles" \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_TOOLCHAIN" \
    -DGKD_SYSROOT="$SYSROOT" \
    -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DVCPKG_OVERLAY_TRIPLETS="$TRIPLET_DIR" \
    -DVCPKG_OVERLAY_PORTS="$OVERLAY_PORTS" \
    -DVCPKG_TARGET_TRIPLET="$TRIPLET" \
    -DVCPKG_HOST_TRIPLET=x64-linux \
    -DVCPKG_MANIFEST_INSTALL="$manifest_install" \
    -DVCPKG_INSTALL_OPTIONS="$vcpkg_install_options" \
    -DCMAKE_FIND_PACKAGE_TARGETS_GLOBAL=FALSE \
    -DLINUX=ON \
    -DENABLE_TESTS=OFF \
    -DBUILD_TOOLS=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXE_LINKER_FLAGS="$linker_flags" \
    -DCMAKE_BUILD_RPATH='$ORIGIN/../../lib_system_sdl;$ORIGIN/../../lib' \
    -DCMAKE_INSTALL_RPATH='$ORIGIN/../../lib_system_sdl;$ORIGIN/../../lib' \
    "${CMAKE_ACCEL_ARGS[@]}"
}

verify_fast_build_tree() {
  local installed="$BUILD_DIR/vcpkg_installed/$TRIPLET"
  local link_file="$BUILD_DIR/CMakeFiles/krkr2.dir/link.txt"
  local missing_file dependency missing_count
  missing_file="$(mktemp)"

  for dependency in \
    "$installed/include/spdlog/spdlog.h" \
    "$installed/include/fmt/format.h" \
    "$installed/lib/libcocos2d.a" \
    "$installed/lib/libspdlog.a" \
    "$installed/lib/libfmt.a"; do
    [ -f "$dependency" ] || printf '%s\n' "$dependency" >>"$missing_file"
  done

  if [ -f "$link_file" ]; then
    tr ' ' '\n' <"$link_file" |
      sed -n "s#^vcpkg_installed/#$BUILD_DIR/vcpkg_installed/#p" |
      sort -u |
      while IFS= read -r dependency; do
        [ -f "$dependency" ] || printf '%s\n' "$dependency"
      done >>"$missing_file"
  else
    printf '%s\n' "$link_file" >>"$missing_file"
  fi

  missing_count="$(sort -u "$missing_file" | sed '/^$/d' | wc -l | tr -d ' ')"
  if [ "$missing_count" -ne 0 ]; then
    echo "[krkr2_build] ERROR: FastBuild cache is incomplete ($missing_count missing dependency files)"
    sort -u "$missing_file" | sed '/^$/d' | head -n 20
    echo "[krkr2_build] Restore or rebuild vcpkg_installed before using FastBuild."
    rm -f "$missing_file"
    exit 1
  fi
  rm -f "$missing_file"
}

{
  echo "[krkr2_build] mode=$MODE"
  echo "[krkr2_build] root=$KRKR2_ROOT"
  echo "[krkr2_build] build=$BUILD_DIR"
  echo "[krkr2_build] sysroot=$SYSROOT"
  echo "[krkr2_build] vcpkg=$VCPKG_ROOT"
  echo "[krkr2_build] triplet=$TRIPLET"
  echo "[krkr2_build] dependency_jobs=$VCPKG_MAX_CONCURRENCY"
  echo "[krkr2_build] safe_cpu_set=$SAFE_CPU_SET"
  if [ "$PERIODIC_COOLING" = "1" ]; then
    echo "[krkr2_build] cooling_policy=periodic work:${WORK_SECONDS}s,cool:${COOL_SECONDS}s"
  else
    echo "[krkr2_build] cooling_policy=continuous"
  fi
  echo "[krkr2_build] scheduler=nice:${NICE_LEVEL} ionice:best-effort:${IO_PRIORITY}"
  echo "[krkr2_build] ccache=${CCACHE_BIN:-disabled} dir=${CCACHE_DIR}"
  echo "[krkr2_build] linker=$LINKER"
  if [ "$MODE" = "FastBuild" ]; then
    test -f "$BUILD_DIR/CMakeCache.txt" && test -f "$BUILD_DIR/Makefile" || {
      echo "[krkr2_build] ERROR: FastBuild requires an existing configured build tree"
      echo "[krkr2_build] Run Configure once after CMake, triplet, or vcpkg changes."
      exit 1
    }
    verify_fast_build_tree
    echo "[krkr2_build] fast incremental mode: skipping probe, Configure, and vcpkg checks"
    if [ -n "$CCACHE_BIN" ] &&
       ! grep -q '^CMAKE_CXX_COMPILER_LAUNCHER:.*ccache' "$BUILD_DIR/CMakeCache.txt"; then
      echo "[krkr2_build] WARN: this cache predates ccache; run Configure once to enable it"
    fi
    if [ -n "$LINKER_FLAG" ] &&
       ! grep -Fq -- "$LINKER_FLAG" "$BUILD_DIR/CMakeCache.txt"; then
      echo "[krkr2_build] WARN: this cache predates $LINKER; run Configure once to enable it"
    fi
  else
    run_probe
  fi
  if { [ "$MODE" = "Build" ] || [ "$MODE" = "FastBuild" ]; } && \
     [ -f "$BUILD_DIR/CMakeCache.txt" ] && [ -f "$BUILD_DIR/Makefile" ]; then
    echo "[krkr2_build] reusing configured build files: $BUILD_DIR"
  else
    configure_krkr2
  fi
  if [ "$MODE" = "Configure" ]; then
    echo "[krkr2_build] configure completed"
    exit 0
  fi
  verify_rocgalgame_source_patches
  prepare_fmod_stub
  verify_recompile_budget
  if [ "$CHECK_ONLY" = "1" ]; then
    echo "[krkr2_build] check-only completed; no compile or link command was run"
    exit 0
  fi
  echo "[krkr2_build] jobs=$BUILD_JOBS"
  set +e
  build_krkr2_target
  build_status=$?
  set -e
  link_file="$BUILD_DIR/CMakeFiles/krkr2.dir/link.txt"
  stub_archive="$FMOD_STUB_BUILD_DIR/libfmod_stub.a"
  if [ "$build_status" -ne 0 ] && {
       grep -q 'undefined reference to.*FMOD' "$LOG_FILE" 2>/dev/null ||
       { [ -f "$link_file" ] && ! grep -Fq "$stub_archive" "$link_file"; }
     }; then
    echo "[krkr2_build] CMake regenerated the FMOD link command; patching link.txt and retrying"
    prepare_fmod_stub
    build_krkr2_target
  elif [ "$build_status" -ne 0 ]; then
    exit "$build_status"
  fi
  KRKR2_BINARY="$(find "$BUILD_DIR" -type f -name krkr2 -perm -u+x | head -n 1)"
  test -n "$KRKR2_BINARY" || { echo "[krkr2_build] ERROR: krkr2 binary was not produced"; exit 1; }
  KRKR2_RESOURCES="$(dirname "$KRKR2_BINARY")/Resources"
  test -d "$KRKR2_RESOURCES" || {
    echo "[krkr2_build] ERROR: krkr2 Resources were not produced: $KRKR2_RESOURCES"
    exit 1
  }
  cp "$KRKR2_BINARY" "$RUNTIME_CORE_DIR/krkr2"
  chmod +x "$RUNTIME_CORE_DIR/krkr2"
  aarch64-linux-gnu-strip --strip-unneeded "$RUNTIME_CORE_DIR/krkr2"
  if command -v perl >/dev/null 2>&1; then
    perl -0777 -pi -e 's#/workspace/#/srcroot__/#g' "$RUNTIME_CORE_DIR/krkr2"
    perl -0777 -pi -e 's#/sources/#/srcroot/#g' "$RUNTIME_CORE_DIR/krkr2"
  fi
  rm -rf "$RUNTIME_CORE_DIR/Resources"
  cp -a "$KRKR2_RESOURCES" "$RUNTIME_CORE_DIR/Resources"
  file "$RUNTIME_CORE_DIR/krkr2"
  readelf -d "$RUNTIME_CORE_DIR/krkr2" | grep -E 'NEEDED|RUNPATH|RPATH' || true
  meta_tmp="$RUNTIME_CORE_DIR/krkr2.build-meta.tmp.$$"
  {
    printf 'schema=1\n'
    printf 'source=krkr2\n'
    printf 'source_commit=%s\n' "$KRKR2_SOURCE_COMMIT"
    printf 'port_lock=%s\n' "$KRKR2_PORT_LOCK"
    printf 'port_repository=https://github.com/LPF970915/ROCgalgame-krkr2-port.git\n'
    printf 'source_dirty=%s\n' "$(git -c safe.directory="$KRKR2_ROOT" -c core.autocrlf=true -C "$KRKR2_ROOT" status --porcelain=v1 | wc -l | tr -d ' ')"
    printf 'artifact_sha256=%s\n' "$(sha256sum "$RUNTIME_CORE_DIR/krkr2" | awk '{print $1}')"
    printf 'build_root=%s\n' "$BUILD_ROOT"
    printf 'triplet=%s\n' "$TRIPLET"
  } >"$meta_tmp"
  mv "$meta_tmp" "$RUNTIME_CORE_DIR/krkr2.build-meta"
  echo "[krkr2_build] installed=$RUNTIME_CORE_DIR/krkr2"
  echo "[krkr2_build] resources=$RUNTIME_CORE_DIR/Resources"
  [ -z "$CCACHE_BIN" ] || ccache --show-stats || true
} 2>&1 | tee "$LOG_FILE"

echo "[krkr2_build] log=$LOG_FILE"
