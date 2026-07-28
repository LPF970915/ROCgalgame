# KRKR2 Minimal Device Test

Date: 2026-07-23

Device: `root@192.168.31.13` (`aarch64`, Linux 6.1.118)

## Passed

- The staged `krkr2` ELF starts on the device.
- OpenGL initializes and reports GLSL/OpenGL 2.0 readiness.
- The core loads the directory project's `startup.tjs`.
- `KRKR2_MINIMAL_TJS_BEGIN` and `KRKR2_MINIMAL_TJS_OK` reach the device log.
- A modal `System.inform` keeps the process alive for the ten-second window check.
- Sway reports the rootful test window `Xwayland on :2`.
- With the non-blocking script, KRKR2 naturally returns exit code `0`.
- With the modal window test, the harness terminates the healthy process and
  records exit code `143` from its deliberate `SIGTERM`.

Passing evidence:

- Device log: `logs/krkr2-minimal/krkr2-minimal-20260723-122608.log`
- Sway tree: `logs/krkr2-minimal/krkr2-minimal-20260723-122608.sway-tree.json`

Self-starting Xwayland repeat verification:

- Device command: `RUN_SECONDS=8 /bin/sh /storage/roms/ports/ROCgalgame/run_krkr2_minimal_test.sh`
- Result: `alive_after_8s=1`, `window_found=1`, `tjs_marker_found=1`, `exit_code=143`
- Device log: `logs/krkr2-minimal/krkr2-minimal-20260723-123025.log`
- Sway tree: `logs/krkr2-minimal/krkr2-minimal-20260723-123025.sway-tree.json`
- Xwayland log: `logs/krkr2-minimal/xwayland-20260723-123025.log`
- Cleanup verified: no `krkr2` or `Xwayland :2` process and no `/tmp/.X11-unix/X2` socket remained.

## Platform Findings

- The device Sway session is native Wayland. The current Cocos2d-x/GLFW build
  is X11/GLX based and cannot open the system `DISPLAY=:0` directly.
- Device `/usr/lib/libGL.so.1.7.0` is a character device rather than an ELF
  library, so the system Xwayland binary cannot start without a private GLVND
  `libGL.so.1`.
- Ubuntu Noble arm64 `libgl1` 1.7.0 supplies a matching AArch64 GLVND library.
- A private rootful `Xwayland :2` using that library plus
  `LIBGL_ALWAYS_SOFTWARE=1`, `-glamor off`, and `-shm` provides a working test
  window without changing `/usr/lib`.

## Remaining Work

- The log reports the data path as
  `/storage/roms/ports/ROCgalgame/cache/savedata/`; KRKR2 does not yet consume
  `ROCGALGAME_KRKR_SAVE_PATH` from `KrkrCoreAdapter`.
- Production launch now creates the tested private Xwayland instance on demand
  inside `CoreProcessRunner` when `runtime=krkr2`. The Wayland frontend remains
  unchanged, while the KRKR2 child receives its X11 display and private GLVND
  search path. The compatibility path still uses software rendering.

## 2026-07-23 Incremental Deployment

The rebuilt core was deployed atomically to the device after SHA-256
verification:

- Local and device SHA-256: `8970B046AAB0CB04C4548F1F9B1C6F85E780247614615CFCE4A3F072AB7DCEA0`
- Previous core backup:
  `/storage/games-external/ROCgalgame_refactor_backups/krkr2_incremental_20260723_192309/krkr2`
- Device backup SHA-256: `d6e7f771e847e17a175cb4eb910034dccada27726deb9c4c88e49883d4760b81`

The 8-second minimal test was repeated with the new binary and again passed
window creation, TJS execution and cleanup. No runtime dependency was reported
missing by `ldd` on the device.
