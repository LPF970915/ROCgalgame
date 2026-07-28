#!/bin/bash
set -euo pipefail

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SELF_DIR/.." && pwd)"
KRKR2_ROOT="${KRKR2_ROOT:-/mnt/d/Works/Tyranor/krkr2}"
SYSROOT="${SYSROOT:-/mnt/d/Works/ROCreader/GKD350HUltra/sysroot_device}"
TOOLCHAIN="$SELF_DIR/toolchain/aarch64-gkd-krkr2.cmake"
TRIPLET_DIR="$SELF_DIR/vcpkg-triplets"
OVERLAY_PORTS="$SELF_DIR/vcpkg-ports;$KRKR2_ROOT/vcpkg/ports"
TRIPLET="arm64-linux-gkd"
PROBE_SOURCE="$SELF_DIR/probes/krkr2_toolchain"
PROBE_BUILD_DIR="${KRKR2_PROBE_BUILD_DIR:-$REPO_ROOT/build/gkd350h/krkr2-toolchain-probe}"
BUILD_DIR="${KRKR2_BUILD_DIR:-$REPO_ROOT/build/gkd350h/krkr2}"
DIST_ROOT="${DIST_ROOT:-$SELF_DIR/dist_lowglibc}"
RUNTIME_CORE_DIR="$DIST_ROOT/ROCgalgame/cores/krkr"
LOG_DIR="${ROC_NATIVE_LOG_DIR:-$SELF_DIR/logs}"
FMOD_STUB_SOURCE="$SELF_DIR/compat/fmod_stub.cpp"
FMOD_STUB_BUILD_DIR="$BUILD_DIR/compat/fmod_stub"
MODE="${KRKR2_BUILD_MODE:-Probe}"
BUILD_JOBS="${KRKR2_BUILD_JOBS:-1}"
SAFE_CPU_SET="${KRKR2_SAFE_CPU_SET:-0}"
WORK_SECONDS="${KRKR2_WORK_SECONDS:-300}"
COOL_SECONDS="${KRKR2_COOL_SECONDS:-60}"
PERIODIC_COOLING="${KRKR2_PERIODIC_COOLING:-0}"
NICE_LEVEL="${KRKR2_NICE_LEVEL:-10}"
IO_PRIORITY="${KRKR2_IO_PRIORITY:-7}"
CMAKE_BIN="${CMAKE_BIN:-$SELF_DIR/tools/cmake/bin/cmake}"
WAYLAND_PKG_CONFIG_DIR="$SELF_DIR/pkgconfig-wayland"

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

test -x "$CMAKE_BIN" || { echo "[krkr2_build] ERROR: bundled CMake is missing: $CMAKE_BIN"; exit 1; }
command -v aarch64-linux-gnu-g++ >/dev/null 2>&1 || {
  echo "[krkr2_build] ERROR: aarch64-linux-gnu-g++ is required"
  exit 1
}
command -v make >/dev/null 2>&1 || { echo "[krkr2_build] ERROR: make is required"; exit 1; }
command -v taskset >/dev/null 2>&1 || { echo "[krkr2_build] ERROR: taskset is required"; exit 1; }
command -v ionice >/dev/null 2>&1 || { echo "[krkr2_build] ERROR: ionice is required"; exit 1; }
command -v setsid >/dev/null 2>&1 || { echo "[krkr2_build] ERROR: setsid is required for cooling pauses"; exit 1; }
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
VCPKG_ROOT="${VCPKG_ROOT:-$SELF_DIR/tools/vcpkg}"
VCPKG_TOOLCHAIN="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
test -f "$VCPKG_TOOLCHAIN" || {
  echo "[krkr2_build] ERROR: vcpkg is missing: $VCPKG_TOOLCHAIN"
  echo "[krkr2_build] Install vcpkg under $SELF_DIR/tools/vcpkg or set VCPKG_ROOT."
  exit 1
}

if [ "$MODE" = "Full" ]; then
  case "$BUILD_DIR" in
    "$REPO_ROOT"/build/gkd350h/krkr2|"$REPO_ROOT"/build/gkd350h/krkr2/*) ;;
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
  grep -Fq 'GLContextAttrs glContextAttrs = { 8, 8, 8, 0, 24, 0 };' "$app_delegate" || {
    echo "[krkr2_build] ERROR: KRKR2 Mali XR24 surface patch is missing"
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
}

prepare_fmod_stub() {
  local fmod_root="$BUILD_DIR/vcpkg_installed/$TRIPLET/share/cocos2dx/linux-specific/fmod"
  local fmod_include="$fmod_root/include"
  local fmod_link_path="vcpkg_installed/$TRIPLET/share/cocos2dx/linux-specific/fmod/prebuilt/64-bit/libfmod.so"
  local stub_object="$FMOD_STUB_BUILD_DIR/fmod_stub.cpp.o"
  local stub_archive="$FMOD_STUB_BUILD_DIR/libfmod_stub.a"
  local mali_link_library="$SYSROOT/lib/libmali.so"
  local link_file="$BUILD_DIR/CMakeFiles/krkr2.dir/link.txt"
  local patched_link_file="$link_file.rocgalgame.tmp"

  test -f "$FMOD_STUB_SOURCE" || {
    echo "[krkr2_build] ERROR: FMOD stub source is missing: $FMOD_STUB_SOURCE"
    exit 1
  }
  test -f "$fmod_include/fmod.hpp" || {
    echo "[krkr2_build] ERROR: Cocos2d-x FMOD headers are missing: $fmod_include"
    exit 1
  }
  test -e "$mali_link_library" || {
    echo "[krkr2_build] ERROR: Mali GLES implementation is missing: $mali_link_library"
    exit 1
  }

  mkdir -p "$FMOD_STUB_BUILD_DIR"
  if [ ! -f "$stub_archive" ] || [ "$FMOD_STUB_SOURCE" -nt "$stub_archive" ]; then
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
      -e 's# -lSDL2##g' \
      -e 's# -lmali##g' \
      -e "s# -fuse-ld=gold##g" \
      -e '/^[[:space:]]*$/d' \
      "$link_file" > "$patched_link_file"
    # The device libGLESv2 shim has no GL exports of its own. With
    # --no-copy-dt-needed-entries the real Mali implementation must be linked
    # explicitly, after every static Cocos archive that references gl*.
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

configure_krkr2() {
  run_low_load "$CMAKE_BIN" --fresh -S "$KRKR2_ROOT" -B "$BUILD_DIR" -G "Unix Makefiles" \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_TOOLCHAIN" \
    -DGKD_SYSROOT="$SYSROOT" \
    -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DVCPKG_OVERLAY_TRIPLETS="$TRIPLET_DIR" \
    -DVCPKG_OVERLAY_PORTS="$OVERLAY_PORTS" \
    -DVCPKG_TARGET_TRIPLET="$TRIPLET" \
    -DVCPKG_HOST_TRIPLET=x64-linux \
    -DLINUX=ON \
    -DENABLE_TESTS=OFF \
    -DBUILD_TOOLS=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_BUILD_RPATH='$ORIGIN/../../lib_system_sdl;$ORIGIN/../../lib' \
    -DCMAKE_INSTALL_RPATH='$ORIGIN/../../lib_system_sdl;$ORIGIN/../../lib'
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
  if [ "$MODE" = "FastBuild" ]; then
    test -f "$BUILD_DIR/CMakeCache.txt" && test -f "$BUILD_DIR/Makefile" || {
      echo "[krkr2_build] ERROR: FastBuild requires an existing configured build tree"
      echo "[krkr2_build] Run Configure once after CMake, triplet, or vcpkg changes."
      exit 1
    }
    echo "[krkr2_build] fast incremental mode: skipping probe, Configure, and vcpkg checks"
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
  rm -rf "$RUNTIME_CORE_DIR/Resources"
  cp -a "$KRKR2_RESOURCES" "$RUNTIME_CORE_DIR/Resources"
  file "$RUNTIME_CORE_DIR/krkr2"
  readelf -d "$RUNTIME_CORE_DIR/krkr2" | grep -E 'NEEDED|RUNPATH|RPATH' || true
  echo "[krkr2_build] installed=$RUNTIME_CORE_DIR/krkr2"
  echo "[krkr2_build] resources=$RUNTIME_CORE_DIR/Resources"
} 2>&1 | tee "$LOG_FILE"

echo "[krkr2_build] log=$LOG_FILE"
