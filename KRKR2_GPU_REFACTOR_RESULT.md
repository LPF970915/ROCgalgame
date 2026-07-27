# KRKR2 GPU Refactor Validation Result

Updated: 2026-07-28  
Device: GKD350H Ultra (`root@192.168.31.13`)

## Result

The first usable native GPU KRKR2 core has been built, deployed, and validated
on the device.

- Display path: native Wayland
- Graphics API: EGL / OpenGL ES
- Renderer: Mali-G52
- Swap policy: `glfwSwapInterval(0)`, paced by the existing Cocos 60 FPS loop
- KRKR2 BuildID: `fb7022ce15dbdaa53312d2e03ae8b8f35178ef81`
- SHA-256: `44b3020fdf30c9c8d17da2c72be99f1d2d14ed66cfa8de63b559d8a9d881b48d`
- Device path: `/storage/roms/ports/ROCgalgame/cores/krkr/krkr2`

The swap interval can be restored to `1` for A/B diagnostics with:

```text
ROCGALGAME_KRKR_SWAP_INTERVAL=1
```

## Device Measurements

Static KRKR2 window:

```text
fps=61.5-61.7
frame_ms_p50=16.13-16.14
frame_ms_p95=16.98-16.99
frame_ms_max=17.19-17.21
```

Continuous FIFO axis input:

```text
fps=61.7-61.8
frame_ms_p50=16.15-16.22
frame_ms_p95=16.72-16.93
frame_ms_max=17.27
pointer_dispatches=310 per 5 seconds
queue_overflows=0
```

No swap operation at or above 50 ms was reported during either test. The old
approximately 1 FPS / 1000 ms frame behavior is no longer present.

Logs:

```text
/storage/roms/ports/ROCgalgame/logs/krkr2-swap0-hw/
/storage/roms/ports/ROCgalgame/logs/krkr2-pointer-probe/
```

## Build Scope And Cache

This was an incremental build, not a clean rebuild:

1. One Cocos object was compiled: `CCGLViewImpl-desktop.cpp.o`.
2. `libcocos2d.a` was re-archived and synchronized to the existing vcpkg cache.
3. KRKR2 was relinked once.

CMake, vcpkg, dependency objects, and the configured KRKR2 build tree remain
available for later incremental builds. The build used one CPU core (CPU 0),
`nice 10`, and low I/O priority.

## Rollback

The previous deployed core is preserved at:

```text
/storage/games-external/ROCgalgame_refactor_backups/
krkr2_swapinterval_44b3020f_20260728/krkr2
```

Previous core SHA-256:

```text
c6cea542462a8157c1bb1bffa64692c3f111e442a09a3d595c1ed6179147580f
```

## Remaining Manual Validation

The minimum GPU scene and continuous pointer transport have passed. The
remaining acceptance work is visual and gameplay testing on the device:

- `NEKOPARA Vol.2`: analog pointer continuity, speed, centering, and clicks.
- `Senren Banka`: layer loading cadence, transitions, and input latency.
- Other representative KRKR2 titles: aspect ratio, cursor coordinates, ABXY,
  D-pad, and Start+Select exit.

These games were not launched automatically because doing so can update their
existing `savedata` or preference files.
