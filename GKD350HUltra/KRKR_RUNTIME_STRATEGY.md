# KRKR Runtime Strategy

Status: accepted and deployed

Date: 2026-08-05

## Decision

Keep KRKR as one frontend core family and support multiple runtime profiles:

1. `krkr2` is the default runtime for KRKR games.
2. `krkrsdl2` is a compatibility fallback selected explicitly per game.
3. Wine is a later fallback for games that require original Windows binaries or
   plugins and cannot be covered by a native runtime.

Do not create one runtime per plugin or game. Add compatibility behavior to a
runtime profile, and select the profile per game only when automatic selection
is not yet reliable.

## Existing ROCgalgame Capability

- The frontend launches cores as child processes and tears down/rebuilds SDL
  around them, so another native process does not need frontend embedding.
- The launch contract already provides per-game working directories, isolated
  save paths, timestamped logs, aspect/filter settings and virtual-mouse input.
- `krkrsdl2` already has the device-specific SDL/Wayland, controller, font,
  WebP, FFmpeg audio/video, PSB and compatibility-plugin work.
- The build pipeline has a verified AArch64 sysroot and deliberately limits
  heavy KRKR builds to one job.

This means the new work is primarily a runtime-host port and compatibility
effort, not a new launcher or input product.

## Why KrKr2 Is Next

The local KrKr2 source has native Linux and Android hosts and builds many
plugins into the executable. Its current plugin set includes `xp3filter`,
`csvParser`, `addFont`, `dirlist`, `layerExMovie`, `saveStruct`, `windowEx`,
`getSample`, `fftgraph`, `psdfile`, `psbfile`, `layerExDraw`, `motionplayer`
and `fstat`. Loading a named DLL is mapped to an internal module rather than
attempting to load a Windows PE DLL on ARM64.

The port is not drop-in. The Linux host currently uses GTK and the Cocos2d-x
GLFW/OpenGL backend, its dependency closure is much larger than `krkrsdl2`, and
the documented Linux target is x86_64. Save isolation and the ROCgalgame input
contract must be wired into the host before it is package-ready.

The 2026-07-21 sysroot preflight found target AArch64 runtime libraries for
GTK3, GLFW, OpenGL/GLES, OpenAL, FFmpeg, libarchive, OpenMP, fmt, Ogg/Vorbis and
WebP. The synchronized sysroot does not contain the matching complete
development-header set. A KrKr2 configure therefore needs either a dedicated
AArch64 Linux vcpkg triplet or a deliberately synchronized development sysroot;
the existing Android triplet cannot be reused for glibc Linux.

`D:\Works\ROCreader\GKD350HUltra\sysroot_device` is a compatible fuller
development sysroot: its target `libc.so.6` hash matches ROCgalgame's GKD
sysroot and it includes GTK3 and Boost headers/package metadata. It does not
contain Cocos2d-x, GLFW, OpenCV or a prebuilt vcpkg installed tree, so those
KrKr2-specific dependencies remain isolated under this project's vcpkg setup.

## Why Wine Is Deferred

Wine has the highest theoretical compatibility with original KRKR executables
and DLLs, but the target also needs a verified x86 execution layer. Legacy
KRKR2 games are commonly 32-bit, so Wine alone is insufficient on ARM64.
Graphics, DirectShow/GStreamer video, audio, Wayland fullscreen, controller
mapping, prefix isolation and memory use all become part of the product.

The synchronized device sysroot contains Wine libraries, including Wayland and
WoW64-related trees, but the device-side `wine`, Box86/Box64 or FEX launchers
have not yet been verified. Wine should therefore be treated as a separately
measured third backend, not as the foundation of KRKR support.

## Runtime Contract

Games remain `core=krkr`. An optional `game.ini` setting selects a runtime:

```ini
core=krkr
runtime=krkr2
```

Supported values are `auto`, `krkrsdl2`, `krkr2` and `wine`. Starting with
release 0.33, `auto` resolves to `krkr2`. An explicit `runtime=krkrsdl2`
continues to take precedence. The frontend exports `ROCGALGAME_KRKR_RUNTIME` and
`ROCGALGAME_KRKR_SAVE_PATH`; the KrKr2 host must consume the latter before it
is considered ready for real saves.

## Default Selection Evidence

The 2026-08-05 device sweep forced KRKR2 for the remaining KRKR library. Of 27
games, 26 remained alive for the complete test window with valid GL/FBO/swap
telemetry and a non-black final frame. `TIME TO STAR II` exceeded the memory
limit and also failed under KRKRSDL2, so it is not evidence in favor of the
fallback runtime. `7 days with Death` remains an explicit KRKRSDL2-compatible
exception. `Criss Cross` remains isolated backlog.

This evidence is sufficient to prefer KRKR2 globally while preserving explicit
per-game fallback. Future compatibility work must be based on repeated failure
signatures across multiple games rather than changing shared behavior for one
title.

Game-specific workarounds must be opt-in rather than changing shared engine
semantics. A game's `game.ini` may declare comma-separated compatibility flags:

```ini
compat_flags=legacy_transition,mali_xr24
```

The frontend forwards these as `ROCGALGAME_KRKR_COMPAT_FLAGS`. New engine
workarounds should remain disabled unless their flag is present, and each flag
must have a regression case in the sweep set.

Expected package layout:

```text
cores/krkr/krkrsdl2
cores/krkr/krkr2
```

Wine will get a separate launcher and prefix layout only after device probing.

## Delivery Stages

1. Add and test the frontend runtime selector without changing the default.
2. Configure KrKr2 for the existing AArch64 sysroot; inventory or replace GTK,
   GLFW, OpenGL, OpenMP and large vcpkg dependencies before compiling.
3. Adapt the KrKr2 host for fullscreen, save isolation, virtual cursor, exit
   chord, logs and direct project launch.
4. Validate a minimal TJS project, then the legacy KRKR2 sample, then Senren
   Banka. Record failures by capability instead of adding game-name hacks.
5. Add conservative packaging and optional automatic runtime probing.
6. Probe Wine/x86 availability on the device and add it only for remaining
   Windows-only plugin cases.

## Acceptance Gates

- Games with `runtime=krkrsdl2` continue to launch through KRKRSDL2.
- Games with no runtime configured launch through KRKR2.
- The KrKr2 core returns cleanly to the same frontend process.
- Both native cores use per-game saves and logs.
- Controller, virtual cursor, aspect behavior, audio and video are verified on
  the device, not inferred from desktop builds.
- A missing optional runtime produces a structured launch error rather than a
  frontend crash.
