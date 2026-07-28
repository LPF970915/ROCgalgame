# KRKR2 GPU Refactor Validation Result

Updated: 2026-07-28  
Device: GKD350H Ultra (`root@192.168.31.13`)

## Result

The current KRKR2 core and matching frontend have been built, deployed, and
validated on the device through the complete frontend input bridge.

- Display path: native Wayland through SDL2
- Window/context/swap: `SDL_CreateWindow`, `SDL_GL_CreateContext`,
  `SDL_GL_SwapWindow`
- Graphics API: EGL / OpenGL ES 2
- Renderer: Mali-G52
- Swap policy: vsync (`SDL_GL_SetSwapInterval(1)`), paced by Cocos at 60 FPS
- KRKR2 BuildID: `1e58ce164faf34ec100a003e5ba6ccdd1dd3d949`
- KRKR2 SHA-256: `df1a9980777dc1f8b295c989220e7daf8a3aede4739cb4e746414fe1b4889ccc`
- Frontend SHA-256: `b0253156f9e1da6b90989208bcd73bdf7abde89fbfe7226a9e3f780de75e576a`
- Device path: `/storage/roms/ports/ROCgalgame/cores/krkr/krkr2`

Vsync is enabled by default. It can be disabled for a controlled A/B run with:

```text
ROCGALGAME_KRKR_SWAP_INTERVAL=0
```

## Shared FBO Restore And Save/Load Validation

The final popup defect was caused by `RenderUtils.h` declaring the renderer's
post-update callback as `static inline`. Each translation unit could therefore
hold a different callback pointer: the OpenGL renderer registered its local
copy while `MainScene.cpp` and nested `TVPDrawSceneOnce()` redraws frequently
read unset copies. Normal gameplay was often rescued by `GetAdapterTexture()`,
but save/load layers could reach swap with KRKR2's common offscreen FBO still
bound and alternate between complete content and black placeholders.

`krkr2-shared-post-update-fbo-restore.patch` replaces that variable with a
function-local static callback slot shared by every inline caller. Both the
normal frame and nested dialog loop now invoke `TVPRunPostUpdateEvent()`.

Device comparison at `SDL_GL_SwapWindow`:

```text
old core after B: previous_fbo=1 viewport=0,0 2048x1024
new core after B: previous_fbo=0 viewport=0,520 1280x200
```

NEKOPARA Vol.2 was then validated entirely in an isolated save directory:

```text
/tmp/rocgalgame-krkr2-dialog-smoke/games/NEKOPARA_Vol.2/savedata
```

Passed workflow:

1. Title `DATA LOAD` opens the full load page.
2. Story menu `SAVE` opens all pages and slots.
3. Slot `No.01` writes `data0.bmp`, `datasc.ksd`, and `datasu.ksd`.
4. The load page persistently renders the thumbnail, title, timestamp,
   comment, and enabled LOAD button without the former black alternation.
5. Loading `No.01` returns to the saved `Patisserie La Soleil` story scene.
6. The steady-state core returns to approximately 59.1 FPS.

Local evidence:

```text
GKD350HUltra/screenshots/krkr2-shared-postupdate-load-page.png
GKD350HUltra/screenshots/krkr2-shared-postupdate-load-slot1-target.png
GKD350HUltra/screenshots/krkr2-shared-postupdate-load-result.png
GKD350HUltra/screenshots/krkr2-shared-postupdate-b-menu.png
```

Persistent device backup:

```text
/storage/games-external/ROCgalgame_refactor_backups/krkr2_shared_postupdate_20260728_1917/
```

The frontend must be deployed together with the core. The previous frontend
(`715348cb2fe76bafcf489e33d04d03789c4fc5e671a06186e1eb4d83568e6407`)
forced `ROCGALGAME_KRKR_XWAYLAND=1` and `SDL_VIDEODRIVER=x11`, which made the
new SDL2 core fail with `Could not get EGL display`. The current frontend sends
`ROCGALGAME_KRKR_XWAYLAND=0`, `SDL_VIDEODRIVER=wayland`, and places
`/usr/lib/mali` first in `LD_LIBRARY_PATH`.

## Full Input-Bridge Validation

The final test used a synthetic `gkd_atom_joypad` uinput device and the normal
frontend autolaunch path into NEKOPARA Vol.2. It therefore covered the complete
physical-input equivalent path rather than writing directly to the core FIFO:

```text
uinput -> InputManager -> CoreInputBridge -> FIFO -> KRKR2 -> virtual pointer
```

Results:

```text
renderer=Mali-G52
fps=56.5 after startup
frame_ms_p50=16.72
frame_ms_p95=17.70
full-axis hold=2.0 seconds
pointer_dispatches=115 (approximately 59 per second)
parsed_buttons=4 (A/B down and up)
parsed_keys=8 (X/Y and D-pad down and up)
queue_overflows=0
ipc_write_failures=0
Start+Select=exit chord requested
post-test frontend_alive=1 input_alive=0 core_alive=0
```

The core also loaded `Resources/img/mouse_icon.png`, confirming that the
circular virtual-cursor texture is present in the running scene. Automated
capture still cannot show the final cursor pixels on this Mali/Sway image, so
cursor appearance remains a direct-screen check; its update rate, motion
transport, button dispatch, and supervised exit are validated.

Persistent device evidence:

```text
/storage/games-external/ROCgalgame_refactor_backups/krkr2_input_smoke_20260728_1521/
```

## Kisaragi Maya Compatibility Validation

`如月真绫的指导` previously appeared to crash during startup. Its July 26
log shows that the old core had already initialized KAG, then raised an
exception in `mainwindow.tjs:1229 saveSystemVariables` while writing the
compressed system file `savedata/oneyuusc.ksd`. KAG subsequently initialized
again, which explains the visible return/flash rather than a GPU startup
failure.

The final core was tested with read-only links to the real XP3, `arc`, and
`plugin` resources, plus a private save directory under `/tmp`. The real game
saves were not modified. The new run passed:

1. The 626-file entry XP3 and all seven secondary XP3 archives opened.
2. `Startup script ended` was reached without a TJS exception.
3. `oneyuusc.ksd` (3138 bytes) and `oneyuusu.ksd` (9974 bytes) were written.
4. The process and KRKR2 Wayland window remained alive for repeated 30-40
   second observation runs.
5. The renderer was `Mali-G52`; steady-state performance was approximately
   58.0-59.2 FPS.
6. A persistent FIFO writer delivered all six A-button protocol messages.
   The game advanced through `title_logo.ks @waitclick`, loaded `title.ks`,
   started OpenAL BGM, and created Start, Load, Config, Extra, and Exit buttons.

The title transition contained a one-time 1403 ms script/resource stall. That
reduced two five-second reporting windows to approximately 50 FPS, after which
steady state returned to 58 FPS. This is a transition-loading spike, not the
former immediate exit or sustained low frame rate.

Expected compatibility warnings remain for Windows-only DLL names such as
`layerExAlpha.dll`, `layeredwindow.dll`, `wuvorbis.dll`, and `extrans.dll`.
They did not prevent startup or title rendering because the port supplies its
supported plugin behavior internally.

Device logs:

```text
/storage/roms/ports/ROCgalgame/logs/kisaragi-compat-newcore/
/storage/roms/ports/ROCgalgame/logs/kisaragi-fifo-sequence/
```

The reproducible FIFO button writer is:

```text
GKD350HUltra/probes/run_krkr2_button_sequence.sh
```

## Full KRKR2 Library Compatibility Sweep

### Frontend launch correction: `向妈妈撒娇吧！`

The July 28 sweep proved that the KRKR2 core can run this game, but it did not
prove that the frontend could select the same runtime and entry point. The
sweep invoked `krkr2` directly with `data.bin`; the real library entry had no
`game.ini`, so a user launch selected the default `krkrsdl2` runtime and passed
the game directory instead. Both real launches created zero-byte core logs and
returned exit code 1.

An explicit `game.ini` was installed temporarily to prove the diagnosis:

```ini
title=向妈妈撒娇吧！
core=krkr
runtime=krkr2
entry=data.bin
virtual_mouse=true
```

This per-game file is not the shipping fix. Version 0.22 detects project
archives by the 11-byte XP3 signature, regardless of whether the archive uses
an `.xp3`, `.bin`, `.dat`, or another extension. A conventional `data.xp3` or
`startup.tjs` keeps the established SDL2 default. When no conventional entry
exists, a unique non-patch XP3 archive, or an unambiguous `data.*` XP3 archive,
is selected with the KRKR2 runtime. Explicit `game.ini` settings still take
priority, `patch*.xp3` is never selected as the main project, and ambiguous
multi-archive layouts are left unchanged instead of guessed. The temporary
device metadata must be removed before the final 0.22 frontend test.

An isolated end-to-end device test used the installed `rocgalgame_sdl`
frontend, a one-game temporary library, and `ROCGALGAME_AUTOLAUNCH_FIRST=1`.
The frontend resolved `krkr2` plus `data.bin`, mounted all 644 files in the main
archive, initialized the Mali-G52 renderer at 1600x1440, reached `Startup script
ended`, entered `title_logo.ks`, and remained alive after 15 seconds. The test
processes were then terminated and no probe process remained.

Future compatibility results must distinguish two gates:

1. **Core compatibility:** direct core invocation reaches the expected startup
   marker without a native crash.
2. **Frontend playability:** `rocgalgame_sdl` scans the real metadata, selects
   the intended runtime and entry point, creates a non-empty per-game log, and
   remains alive at the same checkpoint.

Passing only the first gate must not be reported as a fully playable library
result.

The device library was tested twice on July 28, 2026: once to collect all
failures before making compatibility changes, and once with the final core.
Every run used a private `/tmp` mirror and private `savedata`; no real game save
was modified. All ten cases initialized the `Mali-G52` renderer.

Final result: 8 complete passes, 2 controlled compatibility exits, and 0 native
crashes. Before these fixes the same set produced 6 passes and 4 `SIGSEGV 139`
exits.

| Game | Final result | Startup | Native crash |
| --- | --- | ---: | ---: |
| NEKOPARA Vol.0 | PASS | yes | no |
| NEKOPARA Vol.2 | PASS | yes | no |
| もっと！孕ませ！炎のおっぱい異世界エロ魔法学園！ | PASS | yes | no |
| 丧服萝莉紧缚奇谭 美少女性奴隶调教 | PASS | yes | no |
| 千恋万花 | PackinOne blocked | no | no |
| 向妈妈撒娇吧！ | PASS | yes | no |
| 吹弹！丰盈！波涛汹涌！异世界魔法学园！ | PackinOne blocked | no | no |
| 如月真绫的指导 | PASS | yes | no |
| 桃色恋恋 ～与姐妹相系的H关系～ | PASS | yes | no |
| 超工口APP学园／全部！怀孕！超色情爆乳▼APP学园！ | PASS | yes | no |

Three common defects were corrected:

1. `krkr2-psb-load-safety.patch` removes an uninitialized file-local
   `PSBMedia` pointer, stops on failed/null PSB parses, and registers decoded
   resources through `PSBMediaRegistry`. Both former PSB `SIGSEGV` cases now
   create a window, reach `Startup script ended`, and remain alive.
2. `krkr2-fstat-delete-missing.patch` stops `fstat.deleteFile` from converting
   an empty `TVPGetPlacedPath` result. A missing first-save temporary file is
   now a normal delete miss; both PSB cases complete startup with zero script
   exceptions and create seven isolated save files.
3. `krkr2-tjs-bytecode-bounds.patch` validates TJS bytecode entry and current
   instruction pointers. A zero-sized script generated after a failed plugin
   dependency now raises `ByteCodeBroken` instead of reading an invalid opcode
   address and crashing in `tTJSInterCodeContext::ExecuteCode`.

The two remaining games require real `PackinOne.dll` behavior. Their logs show
`Loading Plugin: PackinOne.dll Failed`, a syntax error, `code_size=0`, and a
controlled `exit 0`. Registering a dummy plugin name would only hide the
dependency, so no fake PackinOne implementation was added.

Final device logs:

```text
/storage/roms/ports/ROCgalgame/logs/krkr2-library-sweep-20260728-220028/
/storage/roms/ports/ROCgalgame/logs/krkr2-library-sweep-20260728-215912/
```

The reproducible sweep supports all cases by default or a comma-separated
subset through `CASE_FILTER`:

```text
GKD350HUltra/probes/run_krkr2_library_sweep.sh
CASE_FILTER=haramase_isekai,app_gakuen RUN_SECONDS=20 ./run_krkr2_library_sweep.sh
```

## Earlier GLFW Baseline Measurements

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

## NEKOPARA Black-Screen Diagnosis

The audio-with-black-screen failure was below KAG/game loading and above the
EGL window. `Startup script ended`, title audio, the Mali-G52 renderer, and a
stable 58-62 FPS loop were all present while the game image was absent.

The first FBO-restore hypothesis was disproved by device retesting and source
review: the original OpenGL manager already registered `_RestoreGLStatues()` as
a post-update callback. Restoring it earlier did not remove the black screen.

One confirmed configuration fault was that KRKR2's internal RenderManager still
defaulted to `software`. Mali accelerated only the outer Cocos window; the
Kirikiri layer tree never entered `RenderManager_ogl.cpp`. Window creation,
renderer identity, audio, and 60 FPS therefore gave a false GPU-positive result.

The ROCgalgame runtime now defaults the internal renderer to `opengl`. The
legacy path remains available with:

```text
ROCGALGAME_KRKR_RENDERER=software
```

Mali also defaults to the regular FBO/shader paths instead of the old
framebuffer-fetch and clear-texture extension shortcuts. The shortcuts can be
restored for A/B testing with:

```text
ROCGALGAME_KRKR_MALI_FAST_PATHS=1
```

`krkr2-gpu-presentation.patch` now:

1. Synchronizes pending texture data before presentation.
2. Restores the screen framebuffer and Cocos viewport before returning the
   adapter texture.
3. Transfers adapter ownership when same-sized draw buffers exchange their GL
   texture names, avoiding stale or prematurely released textures.

The corrected internal layer path was tested during the earlier GLFW/Xwayland
stage with NEKOPARA's real `data.xp3` entry. It reached `Startup script ended`,
initialized OpenAL, played title voice audio, used `Mali-G52`, and settled at
approximately 62 FPS.

The final-layer probe confirmed that the internal GPU path is now active:

```text
renderer=Mali-G52 framebuffer_fetch=0 clear_texture=0 mali_safe=1
logical=1280x720 internal=2048x1024
fbo=0x8CD5 (GL_FRAMEBUFFER_COMPLETE)
read=0x0000
sample_or=0xFFFFFFFF ... 0xFF121517 (non-black layer pixels)
```

That internal renderer correction did not by itself prove physical
presentation. The final host-side change replaces the Cocos GLFW window/swap
path with SDL2 native Wayland while retaining the corrected OpenGL layer path.

Logs:

```text
/storage/roms/ports/ROCgalgame/logs/nekopara-opengl-project-probe/
/storage/roms/ports/ROCgalgame/logs/nekopara-opengl-default-final/
```

Automated screen capture is not an acceptance signal on this image. `grim`
fails with `failed to copy output DSI-1` while the Mali surface is active, and
`/dev/fb0` retains the static ROCKNIX boot framebuffer after DRM takeover.
Visual content must therefore be confirmed directly on the handheld.

Logs:

```text
/storage/roms/ports/ROCgalgame/logs/krkr2-swap0-hw/
/storage/roms/ports/ROCgalgame/logs/krkr2-pointer-probe/
```

## Build Scope And Cache

The original GPU build and black-screen diagnostics were incremental builds,
not clean rebuilds. The shared callback fix only rebuilt:

1. `RenderManager_ogl.cpp.o`.
2. `MainScene.cpp.o`, `FileSelectorForm.cpp.o`, and `TVPWindow.cpp.o`.
3. `libcore_visual_module.a` and `libcore_environ_module.a`.
4. The final stripped KRKR2 executable.

CMake, vcpkg, dependency objects, and the configured KRKR2 build tree remain
available for later incremental builds. The fix build used two bounded CPU
cores (CPU 0-1), `nice 10`, and low I/O priority.

The library compatibility fixes also used `FastBuild` without cleaning or
configuring. TJS rebuilt once. The PSB and fstat sources were each recompiled
for the plugin targets that inherit them through the existing CMake `PUBLIC`
source graph, then cached. No vcpkg dependency was rebuilt. Build logs:

```text
GKD350HUltra/logs/build_krkr2_FastBuild_20260728_204251.log
GKD350HUltra/logs/build_krkr2_FastBuild_20260728_212402.log
```

## Rollback

The core immediately before the final fstat fix is preserved at:

```text
/storage/games-external/ROCgalgame_refactor_backups/krkr2_fstat_delete_20260728-2156/krkr2.pre-fstat-delete
```

Its SHA-256 is:

```text
6da1a1568f18ec8016077f91fa0f8d7e5c1849de71f0a5a9b787d39238ccd3c5
```

The immediately previous pointer-delta core is preserved at:

```text
/storage/games-external/ROCgalgame_refactor_backups/krkr2_shared_postupdate_20260728_1917/krkr2.pre-shared-postupdate
```

Its SHA-256 is:

```text
e54d7e0f9fba1ce577cad9c011d29e916d38e9c4f75431eee2ac554476cf55ef
```

The previous deployed core is preserved at:

```text
/storage/games-external/ROCgalgame_refactor_backups/krkr2_layerex_20260728-093306/krkr2
```

Previous core SHA-256:

```text
8b76c260fb2b63aafa4116bb8a0d252ee49238f926177d693da0cc0ce375c98a
```

## Remaining Manual Validation

The minimum GPU scene, continuous pointer transport, ABXY/D-pad dispatch,
Start+Select exit, and automated NEKOPARA startup have passed. The remaining
acceptance work is visual and gameplay testing directly on the device:

- `NEKOPARA Vol.2`: confirm the circular cursor pixels and click results on the
  physical panel.
- `如月真绫的指导`: select Start on the physical panel and play into the first
  scene to confirm title-button hitboxes, voice playback, and Emote animation.
- `Senren Banka`: blocked before startup until real PackinOne compatibility is
  implemented; layer cadence and input latency cannot yet be accepted.
- Other representative KRKR2 titles: aspect ratio and cursor coordinates.

NEKOPARA save and load now pass in isolation. Kisaragi Maya now reaches its
interactive title instead of exiting. Senren Banka was launched in both full
library sweeps and now exits cleanly at its unsupported PackinOne dependency.
