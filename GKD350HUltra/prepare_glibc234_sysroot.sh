#!/bin/bash
set -euo pipefail

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SELF_DIR/.." && pwd)"
BASE_SYSROOT="${BASE_SYSROOT:?BASE_SYSROOT must point to ROCreader/H700/sysroot_device}"
DEVICE_SYSROOT="${DEVICE_SYSROOT:-$SELF_DIR/sysroot_device}"
OUTPUT_SYSROOT="${OUTPUT_SYSROOT:-$REPO_ROOT/build/gkd350h-glibc234/sysroot}"
REUSE_BASELINE="${REUSE_BASELINE:-0}"

case "$OUTPUT_SYSROOT" in
  "$REPO_ROOT"/build/gkd350h-glibc234/sysroot) ;;
  *) echo "[glibc234] ERROR: unsafe output sysroot: $OUTPUT_SYSROOT"; exit 2 ;;
esac
test -f "$BASE_SYSROOT/lib/aarch64-linux-gnu/libc.so.6" || {
  echo "[glibc234] ERROR: invalid H700 base sysroot: $BASE_SYSROOT"; exit 1;
}
test -f "$DEVICE_SYSROOT/usr/lib/libSDL2-2.0.so.0.3200.6" || {
  echo "[glibc234] ERROR: invalid GKD sysroot: $DEVICE_SYSROOT"; exit 1;
}

case "$REUSE_BASELINE" in 0|1) ;; *) echo "[glibc234] ERROR: REUSE_BASELINE must be 0 or 1"; exit 2 ;; esac
if [ "$REUSE_BASELINE" = "1" ]; then
  test -f "$OUTPUT_SYSROOT/lib/aarch64-linux-gnu/libc.so.6" || {
    echo "[glibc234] ERROR: cannot reuse incomplete baseline: $OUTPUT_SYSROOT"; exit 1;
  }
  echo "[glibc234] reuse existing H700 baseline"
else
  rm -rf "$OUTPUT_SYSROOT"
  mkdir -p "$OUTPUT_SYSROOT"
  echo "[glibc234] copy H700 baseline"
  rsync -a "$BASE_SYSROOT/" "$OUTPUT_SYSROOT/"
fi

copy_headers() {
  local name="$1"
  [ -d "$DEVICE_SYSROOT/usr/include/$name" ] || return 0
  mkdir -p "$OUTPUT_SYSROOT/usr/include/$name"
  rsync -a "$DEVICE_SYSROOT/usr/include/$name/" "$OUTPUT_SYSROOT/usr/include/$name/"
}
for name in SDL2 alsa webp GLES2 KHR EGL wayland xkbcommon; do
  copy_headers "$name"
done

# These device API headers live directly under /usr/include. Keep the list
# narrow so the H700 glibc headers remain the compilation baseline.
for header in bzlib.h; do
  if [ -f "$DEVICE_SYSROOT/usr/include/$header" ]; then
    cp -L "$DEVICE_SYSROOT/usr/include/$header" "$OUTPUT_SYSROOT/usr/include/$header"
  fi
done

copy_device_family() {
  local pattern="$1" source file destination
  for source in "$DEVICE_SYSROOT/usr/lib" "$DEVICE_SYSROOT/lib"; do
    for file in "$source"/$pattern; do
      [ -f "$file" ] && [ -s "$file" ] || continue
      destination="$OUTPUT_SYSROOT/usr/lib/$(basename "$file")"
      rm -f "$destination"
      cp -L "$file" "$destination"
    done
  done
}

# Device APIs are overlaid, but glibc, CRT, libstdc++, libgcc and the loader
# remain from the H700/Ubuntu 22.04 baseline.
for family in \
  'libSDL2*.so*' 'libasound.so*' 'libbz2.so*' 'libjpeg.so*' \
  'libwebp.so*' 'libavcodec.so*' 'libavformat.so*' 'libavutil.so*' \
  'libswresample.so*' 'libswscale.so*' 'libwayland-*.so*' \
  'libxkbcommon.so*' 'libdav1d.so*' 'libEGL.so*' 'libGLESv2.so*' \
  'libGL.so*' 'libmali.so*'; do
  copy_device_family "$family"
done

link_family() {
  local link="$1" target="$2"
  [ -f "$OUTPUT_SYSROOT/usr/lib/$target" ] || return 0
  ln -sfn "$target" "$OUTPUT_SYSROOT/usr/lib/$link"
}
link_family libSDL2.so libSDL2-2.0.so.0.3200.6
link_family libSDL2_image.so libSDL2_image-2.0.so.0.800.2
link_family libSDL2_ttf.so libSDL2_ttf-2.0.so.0.2000.2
link_family libSDL2_mixer.so libSDL2_mixer-2.0.so.0.800.0
link_family libasound.so libasound.so.2.0.0
link_family libbz2.so libbz2.so.1.0.8
link_family libjpeg.so libjpeg.so.8.3.2
link_family libwebp.so libwebp.so.6
link_family libavcodec.so libavcodec.so.60.3.100
link_family libavformat.so libavformat.so.60.3.100
  link_family libavutil.so libavutil.so.58.2.100
  link_family libswresample.so libswresample.so.4.10.100
  link_family libswscale.so libswscale.so.7.1.100
  link_family libdav1d.so libdav1d.so.7.0.0
  link_family libdav1d.so.7 libdav1d.so.7.0.0
link_family libwayland-client.so libwayland-client.so.0.23.1
link_family libwayland-cursor.so libwayland-cursor.so.0.23.1
link_family libwayland-egl.so libwayland-egl.so.1.23.1
link_family libwayland-server.so libwayland-server.so.0.23.1
link_family libxkbcommon.so libxkbcommon.so.0.0.0
link_family libEGL.so libEGL.so.1.1.0
  link_family libGLESv2.so libGLESv2.so.2.1.0
  link_family libmali.so libmali.so.1.9.0
  link_family libmali.so.0 libmali.so.1.9.0

for dir in "$OUTPUT_SYSROOT/lib/aarch64-linux-gnu" \
           "$OUTPUT_SYSROOT/usr/lib/aarch64-linux-gnu"; do
  [ -d "$dir" ] || continue
  rm -f "$dir/libmali.so.0"
  ln -s ../../usr/lib/libmali.so.1.9.0 "$dir/libmali.so.0"
done

# Symlinks synchronized through a Windows filesystem become zero-byte regular
# files. Restore unambiguous same-directory .so links from their real payloads.
repair_windows_links() {
  local dir link target
  while IFS= read -r -d '' link; do
    target="$(find "$(dirname "$link")" -maxdepth 1 -type f \
      -name "$(basename "$link").*" -size +0c -printf '%f\n' | sort -V | tail -n 1)"
    [ -n "$target" ] || continue
    rm -f "$link"
    ln -s "$target" "$link"
  done < <(find "$OUTPUT_SYSROOT/lib" "$OUTPUT_SYSROOT/usr/lib" \
    -type f -size 0c -name '*.so*' -print0 2>/dev/null)
}
repair_windows_links

# Absolute links copied from the device resolve against the build host instead
# of the sysroot. Prefer an existing same-directory payload with the same name.
repair_absolute_links() {
  local link absolute_target local_target
  while IFS= read -r -d '' link; do
    absolute_target="$(readlink "$link")"
    case "$absolute_target" in /*) ;; *) continue ;; esac
    local_target="$(dirname "$link")/$(basename "$absolute_target")"
    [ -f "$local_target" ] || continue
    ln -sfn "$(basename "$local_target")" "$link"
  done < <(find "$OUTPUT_SYSROOT/lib" "$OUTPUT_SYSROOT/usr/lib" \
    -type l -name '*.so*' -print0 2>/dev/null)
}
repair_absolute_links

mkdir -p "$OUTPUT_SYSROOT/usr/lib/pkgconfig"
cat >"$OUTPUT_SYSROOT/usr/lib/pkgconfig/sdl2.pc" <<'EOF'
prefix=/usr
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include
Name: sdl2
Description: GKD350H Ultra SDL2 on the glibc 2.34 build baseline
Version: 2.32.0
Libs: -L${libdir} -lSDL2
Cflags: -I${includedir}/SDL2 -D_REENTRANT
EOF
printf '%s\n' \
  'baseline=ROCreader/H700 Ubuntu 22.04' \
  'required_glibc_max=2.34' \
  'device_runtime=GKD350HUltra ROCKNIX glibc 2.40' \
  >"$OUTPUT_SYSROOT/rocgalgame_glibc234_baseline.txt"
echo "[glibc234] ready: $OUTPUT_SYSROOT"
