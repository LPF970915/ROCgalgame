# KRKR Cross-Build Status

Date: 2026-07-17

The first GKD350HUltra aarch64 KRKR cross-build completed successfully against
a sysroot synchronized from the device at `192.168.31.13`.

## Current Pre-SSH Gate

All host-side work required before the next SSH deployment is complete. The
current package includes the KRKR plugin-compatibility work, `System.addFont`
support, PSB portability fixes, WebP decoding and FFmpeg movie playback used by
Senren Banka. The movie-capable build has not been deployed to the device yet.

The ready-to-deploy archive is:

```text
GKD350HUltra/Downloads/ROCgalgame_GKD350HUltra_v0.1-krkr-video.tar.gz
SHA256=DD7594BC68C61D23D3B8C955C679399697FDBE61F31501FB77A18FA36CED608D
```

The archive contains no game, save or cover payload files. The next action is
therefore an SSH deployment with backup and preservation checks, not another
host build.

## Completed

- Added core-neutral ONS/KRKR launch specifications and structured results.
- Both cores now use frontend shutdown, child wait and frontend reinitialization.
- Shelf tab, page and focus are restored after the child exits.
- KRKR scanning supports project directories, `data.xp3`, arbitrary XP3 files
  and an explicit `game.ini` `entry` value.
- Added `frame_limit`, `draw_threads` and `graphic_cache_mb` overrides.
- Core output is redirected to timestamped `logs/<core>/` files.
- Added a plugin-free 1280x720 TJS smoke project.
- Added compatibility implementations for the Senren plugin set, including
  `WIN32Dialog.dll`, `PackinOne.dll`, `k2compat.dll`, `layerExImage.dll`,
  `KAGParserEx.dll`, `textrender.dll`, `getSample.dll`, `systemEx.dll`,
  `scriptsEx.dll`, `psbfile.dll`, `motionplayer_nod3d.dll`, `menu.dll` and
  `csvParser.dll`.
- Implemented `System.addFont`, `System.AddFont` and `System.AddTrueTypeFont`.
  Font registration now accepts game storage paths and successfully loads
  `patch2.xp3>adobeheiti.otf` from Senren Banka.
- Added RIFF-signature-based WebP loading for synchronous, asynchronous and
  extension-disguised images. Normal output uses BGRA and `glmNormalRGBA` uses
  RGBA, with grayscale conversion handled explicitly.
- Added FFmpeg 6 movie demuxing and decoding through KRKR binary streams,
  including XP3-backed content, H.264/WMV-class video, movie audio through the
  engine-owned FAudio device, `vomLayer` delivery and SDL overlay delivery.
- Capped FFmpeg decoding at two threads and eight queued frames. Layer movies
  larger than the game window are downscaled during BGRA conversion; the real
  1920x1080 Senren movie is delivered at 1280x720 for a roughly 28 MB queue.
- Fixed the PSB plugin's non-Windows buffer allocation by replacing unavailable
  AetherKiri-only mmap helpers with RAII-owned heap storage.
- Added a system-SDL CMake option, GNU linker grouping, aarch64 toolchain,
  dependency validation and KRKR packaging checks.
- Added explicit heavy-build confirmation and a default parallelism of one.
- Completed a full Windows/MinGW host core build at one job, BelowNormal
  priority and two-logical-CPU affinity; installed `cores/krkr/tvpwin64.exe`.
- The minimal project rendered English and Chinese text using
  `fonts/ui_font_02.ttf`, held a valid window at about 69 MB working set and
  accepted a normal close request.
- Verified the real flat `games/` layout without launching commercial content:
  the KRKR shelf showed only `千恋万花`, detected from its `data.xp3`; the ONS
  shelf remains filtered to ONS entries. Evidence: `build/krkr-shelf.bmp`.
- Added a regression test covering flat sibling ONS/KRKR directories, including
  a Chinese KRKR directory name, so games do not need to move into core buckets.
- Completed the frontend lifecycle smoke test from the flat `games/` layout:
  the minimal KRKR child accepted a normal window close, then the same frontend
  PID returned to the KRKR shelf. Evidence: `build/krkr-lifecycle-return.bmp`.
- Synchronized the device glibc 2.40 runtime libraries, reconstructed the
  missing development overlay from the installed aarch64 cross headers and
  matching bundled SDL2 headers, and generated sysroot SDL2 pkg-config data.
- Installed a local CMake 3.31.8 tool under the ignored `GKD350HUltra/tools/`
  directory because WSL sudo was not available non-interactively.
- Completed the latest full aarch64 build and subsequent incremental video
  rebuilds at one job and `nice 10`; installed the 9,607,464-byte PIE executable at
  `GKD350HUltra/dist_lowglibc/ROCgalgame/cores/krkr/krkrsdl2`.
- Added reproducible patches under `GKD350HUltra/patches/` for the krkrsdl2
  CMake changes and KRKRZ font-file registration.
- Frontend Windows build, core launch tests, shell syntax and PowerShell syntax
  passed. A final lightweight rerun is recorded in the handoff summary.

## Safety Stop

Two `krkrsdl2` full builds using eight parallel jobs coincided with unexpected
Windows power-offs. Subsequent builds must use one job, reduced priority and a
restricted CPU affinity until cooling and power delivery are verified. All
KRKR build entry points require explicit confirmation.

Suggested host checks before compiling:

```powershell
Get-WinEvent -FilterHashtable @{LogName='System'; Id=41,6008,1001} -MaxEvents 20 |
  Format-List TimeCreated,Id,ProviderName,Message
```

## Cross-Build Result

The completed build used:

```powershell
cd D:\Works\ROCgalgame
.\GKD350HUltra\sync_sysroot.ps1 -DeviceHost root@192.168.31.13
.\GKD350HUltra\prepare_headers_overlay.ps1
.\GKD350HUltra\build_krkr.ps1 -Jobs 1 -ConfirmHeavyBuild
```

The output is ELF64 AArch64 PIE for GNU/Linux 3.7.0 or newer. Its SHA-256 is:

```text
8D6E57E0F1E9A5EDF9F4CE603FF0374BE19C473283F4859D12FD26FFDF8D4761
```

The KRKR executable now directly requires `libwebp.so.6`, `libavformat.so.60`,
`libavcodec.so.60`, `libswresample.so.4`, `libavutil.so.58` and
`libswscale.so.7`. The FFmpeg libraries are already present in the synchronized
device runtime, so no PortsMaster overlay is required. The packaged WebP target
library is 288,992 bytes at `GKD350HUltra/dist_lowglibc/ROCgalgame/lib/libwebp.so.6`
with SHA-256:

```text
982A296F5F7A4557CA8CD5C8FED16952AAFD28C7C2D7BAFFA7216FF384B90367
```

Device-side `ldd` from tmpfs resolved SDL2, libstdc++, libm, libgcc, libc and
the aarch64 loader. Maximum referenced versions are `GLIBC_2.34` and
`GLIBCXX_3.4.32`; the device provides glibc 2.40 and GLIBCXX 3.4.33.

## Next Device Runtime Test

When SSH is available again, first back up the current program-only runtime and
record hashes for the active ONS core, configuration and launch scripts. Keep
the existing `games`, `covers`, `game_covers` and `saves` directories in place.
Deploy only the fresh application/KRKR files required by the package and
`lib/libwebp.so.6`; do not replace the device's known-good ONS executable.

Then validate, in order: all ELF dependencies, the minimal KRKR project, Senren
Banka startup past plugin/font/PSB/WebP loading, controller confirm/back/menu
and safe exit behavior, frontend return to the KRKR shelf, audio, renderer log,
RSS and frame pacing during animation-heavy scenes.

## Device Deployment

On 2026-07-16 a program-only deployment was installed at
`/storage/roms/ports/ROCgalgame`. It replaced the frontend executable, added
the KRKR core and updated only the KRKR portion of the outer launcher.

The existing ONS core was not replaced. Its active and backup SHA-256 remained
`f2ac3787a6459eb187e6c524b5424fba08e0be4bda564483a0623ea4f362ca31`.
The active `native_config.ini` and `native_keymap.ini` also match their backup
copies byte-for-byte by SHA-256. Device-side `ldd` resolved every dependency
for the new frontend, the unchanged ONS core and the new KRKR core.

The deployment preserved the real game and save directories in place:

- `games`: inode 198, mtime 1783956679, 14 top-level game directories.
- `saves`: inode 229, mtime 1783956679, 7 save directories in the checked depth.

The program-only rollback backup is
`/storage/roms/ports/ROCgalgame-program-backup-20260716-010123`. It intentionally
does not contain or replace game resources or saves. At that time no `千恋万花`
directory was found under `/storage/roms`; it was added before the later clean
redeployment described below.

### Clean Redeployment

Later on 2026-07-16 the application was cleanly redeployed from the complete
test package. The new runtime directory contains fresh frontend, UI, fonts,
configuration and KRKR files. The known-good device ONS executable and its
unchanged launcher branch were retained, together with the existing device
runtime library directories required by all three ELF executables.

The following user directories were moved atomically into the clean runtime,
without copying or modifying their contents:

- `games`: inode 272, 14 top-level directories, 26.3 GB.
- `covers`: inode 875.
- `game_covers`: inode 947, 12.6 MB.
- `saves`: inode 909, 7 save directories in the checked depth, 5.6 MB.

`games/千恋万花/data.xp3` is present at 64,764,045 bytes and is the single
top-level KRKR `data.xp3` entry detected during deployment. The ONS hash remains
`f2ac3787a6459eb187e6c524b5424fba08e0be4bda564483a0623ea4f362ca31`.
All frontend, ONS and KRKR dependencies resolve on the device.

The clean-deployment rollback backup is
`/storage/roms/ports/ROCgalgame-clean-backup-20260716-110800`. It is 73.6 MB
and contains the superseded program, configuration, cache and logs, but no
game, cover or save directories because those remain active in place.

## Remaining Risks

- The new plugin/font/PSB/WebP/video build has passed Windows host tests,
  ARM64 dependency checks and QEMU demux/decode/frame-delivery probes, but the
  movie-capable build has not yet been exercised on the device.
- Device renderer, audio and interactive gameplay controls for real KRKR content
  still require a fresh interactive pass.
- SDL recognizes `gkd_atom_joypad` as a mapped GameController and krkrsdl2 has
  a `VK_PAD*` event path, but game-script confirm/back/menu behavior and a safe
  exit combination still require an interactive test.
- Movie smoothness, audio/video sync, Wayland presentation, RSS and thermals
  still require a real-device pass through the full opening movie.
- The fast package is a deployment test package, not a production release;
  save paths, upgrade behavior and failure recovery still need device tests.

## Final Host Verification

- `make test`: passed (`core launch tests passed`).
- Windows frontend: single-job incremental compile and link passed.
- Windows KRKR core: incremental MinGW build passed at one job, BelowNormal
  priority and affinity mask `3`; the current output is 20,436,540 bytes.
- Plugin compatibility smoke test passed with exit code 0 using
  `fonts/ui_font_02.ttf`; its stderr log is empty.
- Senren Banka loaded its XP3 font and WebP assets without the former plugin,
  PSB or PNG-decoder failures and remained running for 60 seconds on the host.
- Minimal TJS runtime: window title changed to `KRKR smoke test`, English and
  Chinese glyphs rendered from `ui_font_02.ttf`, and normal close succeeded.
- Frontend KRKR shelf: actual library scan selected the KRKR tab and displayed
  only the `千恋万花` card with its Chinese title rendered correctly.
- Frontend/core lifecycle: a temporary minimal TJS entry launched from the KRKR
  shelf, closed normally, and returned to the same frontend process and tab;
  the temporary directory was removed after verification.
- AArch64 KRKR core: full single-job cross-build passed in about 44 minutes;
  FAudio, FreeType, image codecs, Opus and SIMDe-backed SSE/AVX sources built.
- Device `ldd`: all direct and SDL2-transitive dependencies resolved from the
  actual GKD350HUltra runtime; the temporary verification binary was removed.
- Complete package dependency validation passed for the frontend, ONS and KRKR.
  Incremental packaging took about 49 seconds and compiler-free fast packaging
  took about 16 seconds. The final movie package contains no local game, save or
  cover data and contains the KRKR core, ONS core, `ui_font_02.ttf` and
  `libwebp.so.6`.
- ARM64 AAC-in-MP4 regression passed at 48 kHz with playback position advancing.
- The real Senren H.264-in-WMV probe passed under Cortex-A53 QEMU in `vomLayer`:
  source 1920x1080, 24 fps, positive frame/position, one audio stream, with
  output downscaled to the 1280x720 target window.
- The final movie package contains no game, save or cover payload files. Its
  packaged KRKR core matches SHA-256
  `8d6e57e0f1e9a5edf9f4ce603ff0374be19c473283f4859d12fd26ffdf8d4761`.
- The no-window device controller probe reported one joystick with
  `is_controller=1` and a complete SDL mapping; its temporary binary was removed.
- Peak observed smoke-test working set: about 69 MB.
- Power event audit: unexpected shutdowns were Kernel-Power 41 / EventLog 6008;
  no BugCheck 1001 was present in the queried events and no system shutdown
  command exists in the project build scripts.
- On 2026-07-17, shell syntax, PowerShell parser, all relevant `git diff --check`
  runs, offline ELF dependency validation and final archive content audit passed.
