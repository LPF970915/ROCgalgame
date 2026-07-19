# GKD350H Ultra packaging

This folder tracks the GKD350H Ultra / ROCKNIX porting workspace.

Observed target facts from the first device dump:

- OS: ROCKNIX 20260613 community
- Kernel: Linux 6.1.118 aarch64
- Reported SoC/device name: RK3576S
- Userland: aarch64 glibc, dynamic loader `/usr/lib/ld-linux-aarch64.so.1`
- RAM: about 2 GB
- Display stack: Wayland/Sway on DRM card `/dev/dri/card0`
- Framebuffer: `/dev/fb0`
- Panel mode: `1440x1600p60`, presented as `1600x1440` landscape by rotation
- ROM/TF mount: `/storage/roms`
- Writable internal storage: `/storage`
- Main joypad: `/dev/input/event3`, `/dev/input/js0`, `gkd_atom_joypad`

## Display Probe

The screen test confirmed:

- Wayland/Sway output name: `DSI-1`
- Wayland logical rect: `1600x1440`
- Sway transform: `270`
- `wlr-randr` transform: `90`
- DRM connector: `DSI-1`, connector id `179`
- DRM mode: `1440x1600@60`
- DRM panel orientation: `Left Side Up`
- Framebuffer: `/dev/fb0`, `rockchipdrmfb`
- Framebuffer virtual size: `1440,1600`
- Framebuffer bpp/stride: `32 bpp`, `5760`

Prefer SDL2 on Wayland for the first ROCreader port. In that path the app
should see the already-rotated `1600x1440` landscape output. Direct fb/DRM
paths must handle the underlying `1440x1600` portrait buffer and rotation.

## Input Probe

Main controller:

```text
/dev/input/event3
/dev/input/js0
name: gkd_atom_joypad
bus/vendor/product/version: 0x19 / 0x1 / 0x2103 / 0x100
```

Observed event mapping and the ROCreader logical mapping used by the
`gkd350h-ultra` input profile:

| Linux control | Linux event code | ROCreader button | Notes |
| --- | --- | --- |
| D-pad up | `BTN_DPAD_UP` / `544` | Up | digital button |
| D-pad down | `BTN_DPAD_DOWN` / `545` | Down | digital button |
| D-pad left | `BTN_DPAD_LEFT` / `546` | Left | digital button |
| D-pad right | `BTN_DPAD_RIGHT` / `547` | Right | digital button |
| South face | `BTN_SOUTH` / `304` | B | SDL joy button `0` |
| East face | `BTN_EAST` / `305` | A | SDL joy button `1` |
| North face | `BTN_NORTH` / `307` | X | SDL joy button `2` |
| West face | `BTN_WEST` / `308` | Y | SDL joy button `3` |
| L1 | `BTN_TL` / `310` | L1 | |
| R1 | `BTN_TR` / `311` | R1 | |
| L2 | `BTN_TL2` / `312` | L2 | |
| R2 | `BTN_TR2` / `313` | R2 | |
| Select | `BTN_SELECT` / `314` | Select | pressed after Start in first probe |
| Start | `BTN_START` / `315` | Start | pressed before Select in first probe |
| Function/Menu | `BTN_MODE` / `316` | Menu | also exported as `DEVICE_FUNC_KEYB_MODIFIER` |
| Left stick click | `BTN_THUMBL` / `317` | unmapped | |
| Extra system key | `BTN_TRIGGER_HAPPY1` / `704` | unmapped | observed once |
| Left analog X | `ABS_X` / `0`, range about `-899..900` | Left/Right | |
| Left analog Y | `ABS_Y` / `1`, range about `-886..899` | Up/Down | |

The first probe exposed only `ABS_X` and `ABS_Y`; no right analog axes were
reported by `evtest` for `gkd_atom_joypad`.

The cross-built SDL controller probe later confirmed that the device SDL2
recognizes this pad as a GameController without extra environment variables:

```text
guid=1900c3cb010000000321000000010000
is_controller=1
```

The mapping exposes A/B/X/Y, Back/Start, both shoulders, D-pad, left stick and
both triggers. `krkrsdl2` already translates these controller buttons to its
`VK_PAD*` input events. Per-game confirm/back/menu behavior still requires an
on-device gameplay test.

Volume keys are on `gpio-keys`:

```text
/dev/input/event2
KEY_VOLUMEDOWN / 114
KEY_VOLUMEUP / 115
```

The `gkd350h-ultra` profile reads `ABS_X/ABS_Y` only from the
`gkd_atom_joypad` evdev device. Other evdev devices such as `gsensor` also
report absolute axes and must not be treated as navigation input. SDL
controller events are ignored for this profile to avoid duplicate
controller/joystick reports turning one physical key into a Start+Select
quit chord.

## UI Layout Notes

The reader/image modes remain fullscreen `1600x1440`. The settings layout
continues to use the 720x720-at-2x placement rules, while the shelf uses native
1600x1440 artwork geometry:

- Shelf covers: `314x471`, starting at `(74, 222)`
- Shelf grid step: `386px` horizontally and `541px` vertically
- Shelf selection/shadow frame: `394x551`
- Navigation content: four `300px` equal slots starting at `x=196`
- URL shelves: the same `1200px` navigation width divided by the active sub-tab count
- Left/right navigation icons: `(48, 104)` and `(1480, 104)`
- Dynamic battery body: `(1249, 26)`, `47x29`, with a `3px` outline and `4x10` terminal
- Dynamic battery percentage starts at `x=1308`; the clock is right-aligned to `x=1468`
- Dynamic avatar badge: `(1512, 8)`, `64x64`

Current 1600x1440 UI asset expectations:

| Asset role | Expected size |
| --- | --- |
| `background_main.png` | `1600x1440` |
| `top_status_bar.png` | `1600x80` |
| `bottom_hint_bar.png` | `1600x80` |
| Shelf cover/title assets | `314x471` cover/title, `394x551` frame |
| Settings preview images | `1120x1440`, paired with a 480px sidebar |

Suggested local workspace layout:

- `gkd350h_ultra_layout.h`: 1600x1440 layout metrics
- `gkd350h_ultra_profile.h`: model/profile constants and aliases
- `sysroot_device/`: synced from the target over SSH
- `dist_lowglibc/`: staged package output
- `Downloads/`: final release zip files
- `logs/`: build logs

## Docker Release Build

The canonical release command is:

```powershell
.\GKD350HUltra\build_release_docker.ps1
```

The first run builds the cached `rocgalgame-gkd350h-release:22.04` image. Later runs normally finish in a few minutes because only the ROCgalgame frontend is rebuilt. ONS and KRKR are never compiled by this command: their staged binaries must match `release_core_hashes.sha256` before the frontend build, after the frontend build, and after packaging. ZIP files are written through Python's UTF-8-aware standard library so Chinese avatar and UI filenames remain intact when extracted on Windows.

Without `-Version`, the PowerShell wrapper scans `Downloads/` and advances the release number from `0.01` to `0.02`, `0.03`, and so on. A fixed version can be requested explicitly:

```powershell
.\GKD350HUltra\build_release_docker.ps1 -Version 0.01 -Jobs 2
```

The output name is `ROCgalgame verX.XX for GKD350H Ultra.zip`, with this archive layout:

```text
roms/
  ports/
    ROCgalgame.sh
    ROCgalgame/
      rocgalgame_sdl
      cores/
      fonts/
      lib/
      sounds/
      ui/
```

Mutable `games/`, `covers/`, `saves/`, and `cache/` directories are included empty. Local games, covers, cache files, and saves are never copied into a release.

The first build route should reuse the repository low-glibc flow with a target
sysroot:

```sh
DEVICE_HOST=root@192.168.31.12 ./GKD350HUltra/sync_sysroot.sh
./GKD350HUltra/prepare_headers_overlay.sh
./GKD350HUltra/build_low_glibc.sh
```

From PowerShell on Windows:

```powershell
.\GKD350HUltra\sync_sysroot.ps1 -DeviceHost root@192.168.31.12
.\GKD350HUltra\prepare_headers_overlay.ps1
.\GKD350HUltra\build_low_glibc.ps1
```

GKD builds now require a real PDF backend. The observed ROCKNIX sysroot ships
`libpoppler.so.128` but not `libpoppler-cpp`, so the current test sysroot uses
the existing TrimuiBrick aarch64 Poppler-C++ overlay for:

- `usr/include/poppler`
- `libpoppler-cpp.so.0`
- `libpoppler.so.58`
- `libpng12.so.0`
- `libjbig.so.0`
- `libtiff.so.5`

Without that overlay, PDF files fall back to ROCreader's mock backend and render
as gray/white placeholder stripes.

The ROCKNIX image observed on GKD350H Ultra ships runtime libraries but no
`/usr/include`. `prepare_headers_overlay.*` prefers an existing H700 header
overlay and otherwise uses the installed aarch64 cross headers plus the SDL2
headers bundled with krkrsdl2. Linking still uses the synchronized GKD runtime.

## KRKR Cross-Build

KRKR host preparation, frontend launch integration and the first aarch64
cross-build are complete. The current results and remaining runtime tests are in
`KRKR_PRE_CROSS_BUILD_STATUS.md`; the provisional core decision is in
`KRKR_CORE_SELECTION_ADR.md`.

The successful standalone build sequence was:

```powershell
.\GKD350HUltra\sync_sysroot.ps1 -DeviceHost root@192.168.31.13
.\GKD350HUltra\prepare_headers_overlay.ps1
.\GKD350HUltra\build_krkr.ps1 -Jobs 1 -ConfirmHeavyBuild
```

The explicit confirmation and one-job limit remain intentional. Earlier
eight-job host builds coincided with Kernel-Power 41 shutdowns. The aarch64
build completed successfully with one job and `nice 10`.

WebP support is part of the KRKR core build. The Windows build statically links
MSYS2 `libwebpdecoder.a`; the aarch64 build takes `webp/decode.h` from the
prepared header overlay and packages the target `libwebp.so` beside the app.
The image loader checks the `RIFF....WEBP` signature, so games that store WebP
content under a `.png` name are handled without per-game resource changes.

Some KRKR games also store AAC-in-MP4 audio under an `.ogg` name. The ARM64
core detects the file signature and streams it through the FFmpeg 6 libraries
already present on the tested ROCKNIX image (`libavformat.so.60`,
`libavcodec.so.60`, `libswresample.so.4`, and `libavutil.so.58`). Build headers
come from the sibling `D:\Works\Tyranor\FFmpeg-n6.0` reference checkout plus
`GKD350HUltra/ffmpeg_headers_overlay`. This is a source-file-local build option,
so later AAC decoder edits do not invalidate every KRKR object file.

The repeatable offline ARM64 playback probe takes a real M4A/AAC sample, names
it `.ogg`, runs the target core under QEMU, and requires the playback position
to advance:

```powershell
.\GKD350HUltra\test_krkr_aac_qemu.ps1
```

KRKR movie playback is also backed by the target FFmpeg 6 runtime. The SDL2
port previously compiled `VideoOverlay` to no-ops outside Windows; the new
backend streams MP4/WMV/WebM/etc. directly from KRKR storage/XP3, decodes video
with `libavcodec`, converts frames to BGRA with `libswscale.so.7`, and sends
movie audio through the engine-owned FAudio device. Overlay/mixer modes render
as an SDL texture above the normal KRKR surface, while `vomLayer` continues to
update the script-provided Layer. Decode threading is capped at two FFmpeg
threads to keep memory, temperature and frame latency predictable on the GKD.
When a layer movie is larger than the game window, `libswscale` now downsizes
the decoded BGRA frame to the window bounds while preserving the source aspect
ratio. The Senren Banka probe is 1920x1080 but is delivered as 1280x720 on the
target-sized window, reducing the eight-frame queue from about 63 MB to 28 MB
and avoiding oversized-layer rendering faults.

The target image already contains all required video libraries, including
`libswscale.so.7`; no PortsMaster overlay is currently required. Re-run the
real Senren Banka WMV probe after rebuilding:

```powershell
.\GKD350HUltra\test_krkr_video_qemu.ps1
```

The probe requires `VIDEO_PLAY_OK` with a positive frame number. QEMU only
validates demux/decode, frame delivery and audio queue construction; final
audio/video smoothness, Wayland presentation and thermal behavior still need
the next on-device SSH test.

When the device is online, deploy only the KRKR executable with an on-device
hash check, timestamped backup, and ONS/frontend invariants:

```powershell
.\GKD350HUltra\deploy_krkr_core.ps1 -DeviceHost root@192.168.31.13
```

After a Windows core build, run the repeatable host smoke tests with the
required Chinese font explicitly selected:

```powershell
.\Windows\test_krkr_windows.ps1 -SenrenSeconds 60
```

## Build And Package Modes

Normal UI/config staging reuses the already validated binaries, synchronizes
assets, validates dependencies and does not start a compiler or archive pass:

```powershell
.\GKD350HUltra\build_package.ps1 -Mode Fast -Output Stage -Version 0.1-test
```

During the planned UI/ONS work, build only those targets and leave the staged
KRKR binary untouched:

```powershell
.\GKD350HUltra\build_package.ps1 -Mode Incremental -BuildTargets Frontend,ONS -Output Stage -Version 0.1-test
```

When KRKR source changes, its default `Fast` mode skips the manual CMake
configure pass and lets the existing build tree compile only invalidated
objects:

```powershell
.\GKD350HUltra\build_krkr.ps1 -Mode Fast -Jobs 1 -ConfirmHeavyBuild
```

Use `-Mode Configure` after changing CMake options or after installing ccache.
`-Ccache Auto` uses ccache during a new/configured build when available;
`-Ccache On` requires it. Only use `-Mode Full` when the toolchain or ABI
changed, the CMake cache is incompatible/corrupt, or a major source
restructure invalidated most objects.

Create release archives explicitly so day-to-day staging avoids redundant
copying and compression:

```powershell
.\GKD350HUltra\build_package.ps1 -Mode Fast -Output Zip -Version 0.1-test
```

Release archives intentionally contain empty `games`, `covers`, `saves`, and
`cache` directories rather than private content. Preserve `build/gkd350h/`,
`GKD350HUltra/sysroot_device/`, `GKD350HUltra/dist_lowglibc/`, and the ignored
local CMake tool directory. Removing them turns later work into recovery or a
full rebuild. Each successful PowerShell build/package run refreshes the
ignored `build/gkd350h/build_checkpoint.json` artifact/cache record.
