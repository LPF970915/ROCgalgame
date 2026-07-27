# KRKR2 Capability and NEKOPARA Test

Date: 2026-07-23

Device: `root@192.168.31.13` (`aarch64`, Sway/Wayland with private Xwayland `:2`)

## Capability Probe

Project path:

`/storage/roms/ports/ROCgalgame/cache/KRKR2中文路径测试`

Passed:

- The directory project and `startup.tjs` load from a Chinese path.
- The TJS probe creates and keeps a 640x480 window alive.
- PNG loading reports `KRKR2_PROBE_IMAGE_OK`.
- OpenAL Soft initializes successfully.
- WAV open and playback report `KRKR2_PROBE_AUDIO_OPEN_OK` and
  `KRKR2_PROBE_AUDIO_PLAY_OK`.
- Sway reports the private Xwayland window.

Evidence:

- Device log: `logs/krkr2-capability-20260723-125014.device.log`
- Sway tree: `logs/krkr2-capability-20260723-125014.sway-tree.json`

## Input Status

Input is not accepted yet.

- Sway accepts synthetic cursor movement and button commands.
- A temporary `/dev/uinput` relative-pointer device can be created and removed.
- Neither compositor injection nor uinput generated a TJS mouse marker through
  the current rootful Xwayland host.
- `TVPWindowLayer::onMouseDownEvent` in the current source handles right and
  middle buttons but drops Cocos `BUTTON_LEFT` in its default branch. This is
  a confirmed core defect even though the right-button uinput probe also did
  not reach TJS.

Evidence:

- Device log: `logs/krkr2-capability-uinput-20260723-125812.device.log`

## XP3 and NEKOPARA

Game: `games/NEKOPARA Vol.2`

Directly passing the game directory is not supported by the current host. It
tries to open the directory as one archive. Passing `data.xp3` directly works:

- XP3 metadata is read successfully.
- The archive contains 1343 files and 1343 segments.
- The path containing a space is normalized and opened correctly.

The unmodified game then exits with signal 11 / exit code 139 before creating
its game window. Symbolized gdb frames identify the first blocker as:

`EmotePlayerPreRegist -> ncbAutoRegister::LoadModule(motionplayer.dll) ->
PostRegistCallback -> TVPExecuteExpression ->
TJS::tTJSVariantString::GetLength`

This is an Emote/motionplayer registration and exception-handling defect, not
an XP3 failure.

An isolated copy containing only `data.xp3`, without the external `patch.tjs`,
gets substantially further:

- Loads KAG EX 3.27-dev.20070519.
- Reports Kirikiri 2.32.2.426.
- Loads the main KAG system and many game scripts.
- Initializes the built-in PSB plugin.
- Reports missing `layerExImage.dll`, `layerExRaster.dll`, and
  `layerExBtoA.dll` compatibility modules.

It then reaches a second signal 11 in a TJS string-function call chain ending
at `TJS::tTJSVariantString::GetLength`. Fixing Emote alone will therefore not
make this title playable yet.

Evidence:

- Initial XP3 run: `logs/nekopara-data-xp3-20260723-123940.device.log`
- Data-only run: `logs/nekopara-data-only-20260723-124310.device.log`
- Original-game gdb: `logs/nekopara-gdb-20260723.log`
- Original-game mappings: `logs/nekopara-gdb-map-20260723.log`
- Data-only gdb: `logs/nekopara-data-only-gdb-20260723.log`

## Next Fix Order

1. Fix Cocos left-button dispatch and verify a physical/uinput event reaches TJS.
2. Replace or harden the motionplayer registration expression that crashes
   while `emoteplayer.dll` loads.
3. Add graceful TJS null-string handling so a script error is logged instead
   of terminating the process.
4. Implement common `layerExImage`, `layerExRaster`, and `layerExBtoA`
   compatibility behavior.
5. Add directory project auto-selection/XP3 mounting and isolated save paths.
6. Re-run NEKOPARA through title, audio, input, and save checkpoints.

## 2026-07-23 19:23 Deployment Recheck

The current rebuilt `krkr2` was deployed to `root@192.168.31.13` with an
atomic rename after matching SHA-256 verification. The existing `krkrsdl2`
core was not changed.

### Runtime Dependencies

- Device ELF: AArch64 PIE, interpreter `/lib/ld-linux-aarch64.so.1`.
- `ldd` resolved all dependencies, including GLX/OpenGL, X11, Xi, Xrandr,
  OpenAL-related runtime paths, libstdc++, libc and the target loader.
- No `not found` entries were reported.

### Capability Recheck

The existing Chinese-path probe was rerun for 8 seconds:

- Window alive and visible through private Xwayland.
- TJS startup reached `KRKR2_PROBE_READY`.
- `KRKR2_PROBE_IMAGE_OK`.
- `KRKR2_PROBE_AUDIO_OPEN_OK` and `KRKR2_PROBE_AUDIO_PLAY_OK` through OpenAL Soft.
- Device log: `logs/krkr2-capability-new/krkr2-capability-new-20260723-192613.log`.

The unified input probe kept the window alive, but Sway/uinput injection did
not reach `KRKR2_PROBE_WINDOW_MOUSE_DOWN`. This does not prove the physical
controller path is broken; it identifies the current rootful Xwayland test
injection boundary as unable to deliver the synthetic event.

### NEKOPARA Recheck

Launching the isolated `data.xp3` for 8 seconds succeeded in mounting all 1343
files/segments and loading KAG, PSB and the main script set. A 30-second run
then ended with exit code 139 after reporting the missing compatibility modules
`layerExImage.dll`, `layerExRaster.dll` and `layerExBtoA.dll`.

- 8-second log: `logs/nekopara-new/nekopara-new-20260723-192705.log`.
- 30-second log: `logs/nekopara-new-30s/nekopara-new-30s-20260723-192737.log`.
- This older run predates the layerEx compatibility bundle; it was not an XP3
  or ELF dependency failure.

## 2026-07-24 LayerEx Compatibility Recheck

The common compatibility bundle is now built into the AArch64 core:

- `layerExImage.dll`
- `layerExRaster.dll`
- `layerExBtoA.dll`

The probe links all three modules before creating its `Layer` instance, which
matches KRKR2's native class registration order. It passes image operations,
same-layer `copyRaster`, redraw, and the three BtoA operations, and emits
`LAYEREX_COMPAT_OK`.

The final stripped core deployed to `root@192.168.31.13` has SHA-256
`003393774459a60276d3ea6e309545d66712cf2c0227cbf19aec1efa0f611142`.
The 12-second window harness observed a live window and ended it deliberately
with exit code 143; no segmentation fault occurred.

The next common blockers are motionplayer/emote registration and defensive
TJS string-error handling before the NEKOPARA regression can be repeated.

## 2026-07-24 NEKOPARA Stable Startup

The TJS null-string ABI fixes are now built into the deployed AArch64 core.
They preserve legacy empty-string behavior under GCC `-O3` and make null
variant `Release()` a no-op. The stripped core has BuildID
`e597d425399df5793887d7997f1120c7cfcd4744` and SHA-256
`b2923e01f9a98161ee426e457d5dc5cc557ad9f6833a18a62267ec09f68377f7`.

NEKOPARA Vol.2 passed a 90-second direct-core run and a 75-second launch
through `rocgalgame_sdl`. Both runs completed the startup script, constructed
`KAGMainWindow`, retained a visible window, initialized OpenAL Soft, loaded the
title UI, and selected OGG random voice assets. The harness ended the direct
run with SIGTERM after confirming the process was still alive; no SIGSEGV or
exit-code-1 failure occurred.

- Direct-core log: `logs/nekopara-null-release-fixed/nekopara-null-release-fixed-20260724-111631.log`.
- Frontend core log: `logs/krkr/NEKOPARA Vol.2_20260724_112020.log`.
- Missing optional modules still logged: `wuvorbis.dll`, `wuopus.dll`,
  `extrans.dll`, and `getLangName.dll`.

## 2026-07-24 Manual Frontend Launch Fix

Manual launches originally failed immediately with exit code 1 and only
`Gtk-WARNING: cannot open display` in the core log. The production frontend was
running natively on Wayland and did not provide an X11 display to its KRKR2
child; the earlier automated run had supplied `DISPLAY=:2` externally.

`KrkrCoreAdapter` now requests the private Xwayland host only for `runtime=krkr2`,
and `CoreProcessRunner` creates it through the active Sway session immediately
before launching the core. The child receives `DISPLAY=:2`, X11 GTK/SDL
backends, and the KRKR2 private GL library path. The frontend itself and the
shared controller/virtual-cursor input layer remain on their existing Wayland
path.

The deployed frontend has BuildID `dc396982a06b0331e2d2856ad09c2c68f28d21d3`
and SHA-256
`25235eb1de2512af56e3d664e0272d5a60e1c37e89a83feebe7edac2689e84bb`.
A cold 75-second run through the real `ROCgalgame.sh` entry created Xwayland,
kept KRKR2 alive through the title voice sequence, and cleaned up the frontend,
core, Xwayland process, and X11 socket when the harness ended.

- Production-chain log: `logs/krkr/NEKOPARA Vol.2_20260724_114203.log`.
