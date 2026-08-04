#!/bin/bash
set -euo pipefail

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SELF_DIR/.." && pwd)"
RUNTIME_SOURCE="${ROCGALGAME_RUNTIME_SOURCE:-$REPO_ROOT/GKD350HUltra/dist_glibc234/ROCgalgame}"
H700_LIB_SOURCE="${ROCGALGAME_H700_LIB_SOURCE:-/mnt/d/Works/ROCreader/H700/dist_lowglibc/APPS/ROCreader}"
BUILD_SYSROOT="${ROCGALGAME_BUILD_SYSROOT:-$REPO_ROOT/build/gkd350h-glibc234/sysroot}"
VERSION="${ROCGALGAME_VERSION:-0.01}"
OUTPUT="${ROCGALGAME_OUTPUT:-Stage}"
DIST_ROOT="$SELF_DIR/dist_lowglibc"
STAGE_ROOT="$DIST_ROOT/release_stage"
RUNTIME="$STAGE_ROOT/Roms/APPS/ROCgalgame"
ARCHIVE="$SELF_DIR/Downloads/ROCgalgame ver${VERSION} for H700 34xxSP.zip"

case "$OUTPUT" in Stage|Zip) ;; *) echo "[h700] OUTPUT must be Stage or Zip" >&2; exit 2 ;; esac
for required in "$RUNTIME_SOURCE/rocgalgame_sdl" \
  "$RUNTIME_SOURCE/cores/ons/onsyuri" "$RUNTIME_SOURCE/cores/krkr/krkrsdl2" \
  "$RUNTIME_SOURCE/cores/krkr/krkr2" "$RUNTIME_SOURCE/ui.pack"; do
  [ -f "$required" ] || { echo "[h700] missing runtime input: $required" >&2; exit 1; }
done
krkr2_meta="$RUNTIME_SOURCE/cores/krkr/krkr2.build-meta"
if [ ! -s "$krkr2_meta" ] ||
   ! grep -Eq '^source_commit=[0-9a-f]{40}$' "$krkr2_meta" ||
   ! grep -Eq '^source_dirty=0$' "$krkr2_meta"; then
  echo "[h700] missing clean KRKR2 provenance metadata: $krkr2_meta" >&2
  exit 1
fi

rm -rf "$STAGE_ROOT"
mkdir -p "$RUNTIME" "$STAGE_ROOT/Roms/APPS/Imgs" "$SELF_DIR/Downloads"
cp -a "$RUNTIME_SOURCE"/. "$RUNTIME/"
rm -rf "$RUNTIME/games" "$RUNTIME/covers" "$RUNTIME/saves" "$RUNTIME/cache" \
  "$RUNTIME/native_config.ini" "$RUNTIME/native_keymap.ini" "$RUNTIME/ROCgalgame.log"
mkdir -p "$RUNTIME/games" "$RUNTIME/covers" "$RUNTIME/saves" "$RUNTIME/cache"
cp "$SELF_DIR/native_config.ini" "$RUNTIME/native_config.ini"
cp "$SELF_DIR/native_keymap.ini" "$RUNTIME/native_keymap.ini"
cp "$REPO_ROOT/ROCgalgame.sh" "$RUNTIME/launch.sh"
cp "$REPO_ROOT/ui/common/icon.png" "$STAGE_ROOT/Roms/APPS/Imgs/ROCgalgame.png"
printf '%s\n' "$VERSION" > "$RUNTIME/version.txt"

if [ -d "$H700_LIB_SOURCE/lib" ]; then
  mkdir -p "$RUNTIME/lib"
  cp -a "$H700_LIB_SOURCE/lib"/. "$RUNTIME/lib/"
fi
if [ -d "$H700_LIB_SOURCE/lib_system_sdl" ]; then
  mkdir -p "$RUNTIME/lib_system_sdl"
  cp -a "$H700_LIB_SOURCE/lib_system_sdl"/. "$RUNTIME/lib_system_sdl/"
fi

# Add the FFmpeg SONAMEs required by the freshly built krkrsdl2 core.
for name in libwebp.so.6 libavformat.so.60 libavcodec.so.60 libswresample.so.4 \
            libavutil.so.58 libswscale.so.7; do
  found=""
  for dir in "$BUILD_SYSROOT/usr/lib" "$BUILD_SYSROOT/usr/lib/aarch64-linux-gnu" \
             "$BUILD_SYSROOT/lib" "$BUILD_SYSROOT/lib/aarch64-linux-gnu"; do
    if [ -f "$dir/$name" ]; then found="$dir/$name"; break; fi
  done
  if [ -n "$found" ]; then cp -L "$found" "$RUNTIME/lib/$name"; fi
done

# Resolve the non-glibc transitive closure (for example libdav1d used by
# libavcodec) from the two target sysroots. System glibc stays on the device.
dependency_dirs=("$BUILD_SYSROOT/usr/lib" "$BUILD_SYSROOT/usr/lib/aarch64-linux-gnu"
  "$BUILD_SYSROOT/lib" "$BUILD_SYSROOT/lib/aarch64-linux-gnu"
  "$H700_LIB_SOURCE/lib" "$H700_LIB_SOURCE/lib_system_sdl")
is_system_library() {
  case "$1" in
    libc.so.*|libm.so.*|libpthread.so.*|libdl.so.*|librt.so.*|ld-linux-*.so.*|linux-vdso.so.*) return 0 ;;
    libmali.so.*|libEGL.so.*|libGLES*.so.*|libGL.so.*|libGLX.so.*|libGLdispatch.so.*|\
    libgbm.so.*|libdrm.so.*|libwayland*.so.*) return 0 ;;
  esac
  return 1
}
for pass in 1 2 3 4; do
  while IFS= read -r -d '' elf; do
    { readelf -d "$elf" 2>/dev/null || true; } |
      sed -n 's/.*Shared library: \[\([^]]*\)\].*/\1/p'
  done < <(find "$RUNTIME" -type f -print0) | sort -u | while IFS= read -r name; do
    [ -n "$name" ] || continue
    is_system_library "$name" && continue
    [ -f "$RUNTIME/lib/$name" ] || [ -f "$RUNTIME/lib_system_sdl/$name" ] && continue
    found=""
    for dir in "${dependency_dirs[@]}"; do
      if [ -f "$dir/$name" ]; then found="$dir/$name"; break; fi
    done
    if [ -n "$found" ]; then
      cp -L "$found" "$RUNTIME/lib/$name"
    fi
  done
done

cp "$REPO_ROOT/ROCgalgame.sh" "$STAGE_ROOT/Roms/APPS/ROCgalgame.sh"
chmod +x "$RUNTIME/launch.sh" "$STAGE_ROOT/Roms/APPS/ROCgalgame.sh" \
  "$RUNTIME/rocgalgame_sdl" "$RUNTIME/cores/ons/onsyuri" \
  "$RUNTIME/cores/krkr/krkrsdl2" "$RUNTIME/cores/krkr/krkr2"

VERIFY="$REPO_ROOT/GKD350HUltra/verify_glibc_compat.sh"
if command -v readelf >/dev/null 2>&1; then
  # The reused KRKR2 binary contains historical source paths in metadata;
  # keep the ABI gate strict while treating that known non-runtime issue as a warning.
  MAX_GLIBC=2.34 ROCGALGAME_ALLOW_HOST_PATH_LEAK=1 "$VERIFY" \
    "$RUNTIME/rocgalgame_sdl" "$RUNTIME/cores/ons/onsyuri" \
    "$RUNTIME/cores/krkr/krkr2" "$RUNTIME/cores/krkr/krkrsdl2"
else
  echo "[h700] readelf unavailable; run verify_glibc_compat.sh in WSL before shipping" >&2
fi

python3 - "$STAGE_ROOT" "$ARCHIVE" "$OUTPUT" <<'PY'
import pathlib
import sys
import zipfile

stage = pathlib.Path(sys.argv[1])
archive = pathlib.Path(sys.argv[2])
output = sys.argv[3]
runtime = stage / "Roms" / "APPS" / "ROCgalgame"
required = [
    stage / "Roms" / "APPS" / "ROCgalgame.sh",
    stage / "Roms" / "APPS" / "Imgs" / "ROCgalgame.png",
    runtime / "launch.sh", runtime / "ui.pack",
    runtime / "native_config.ini", runtime / "native_keymap.ini",
]
missing = [str(path) for path in required if not path.is_file()]
if missing:
    raise SystemExit("[h700] package validation missing: " + ", ".join(missing))
if output == "Zip":
    archive.parent.mkdir(parents=True, exist_ok=True)
    if archive.exists():
        archive.unlink()
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as zf:
        for path in sorted(stage.rglob("*")):
            if path.is_file():
                zf.write(path, path.relative_to(stage).as_posix())
print(f"[h700] validated stage: {stage}")
if output == "Zip":
    print(f"[h700] archive: {archive}")
PY
