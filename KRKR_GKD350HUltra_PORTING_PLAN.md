# KRKR to GKD350H Ultra Porting Plan

Status: Candidate A selected; host compatibility and ARM64 package complete; awaiting SSH deployment and device validation

Date: 2026-07-17

Target: ROCgalgame on GKD350H Ultra / ROCKNIX / aarch64

## Current Implementation Progress

- Candidate A, `krkrsdl2` with built-in compatibility modules, is the selected
  core and the decision is recorded in `GKD350HUltra/KRKR_CORE_SELECTION_ADR.md`.
- The core-neutral ONS/KRKR frontend launch contract, shelf separation, logging,
  focus restoration and per-game launch overrides are implemented.
- The Windows host core loads the target game's XP3 content, registers its
  packaged font through `System.addFont`, passes the compatibility-plugin smoke
  project and remains stable in Senren Banka for 60 seconds.
- Target-required PSB portability and WebP image loading are implemented. The
  former Windows-plugin, UTF conversion and disguised-WebP blockers are cleared.
- The current AArch64 KRKR core is built at one job/low priority, and the fast
  release package includes its `libwebp.so.6` dependency. Offline ELF dependency
  validation and a no-user-payload archive audit pass.
- The remaining immediate milestone is SSH deployment with backup/preservation
  checks followed by real-device renderer, input, audio, animation and memory
  validation. FFmpeg movie playback remains a later phase.

## 1. Objective

Add a maintainable KRKR runtime to ROCgalgame that can:

- launch KRKR2/KRKRZ games from the existing shelf;
- release and reacquire SDL/Wayland resources without display conflicts;
- provide complete handheld controls without requiring a mouse or keyboard;
- run 1280x720 content smoothly on the 1600x1440 panel;
- isolate saves under `saves/krkr/<game-id>`;
- support common commercial-game plugins through built-in native modules or compatibility stubs;
- play common full-screen and layer video through FFmpeg;
- return cleanly to the same frontend process with actionable error reporting.

The first compatibility target is the local `games/千恋万花` package. It is a stress target, not the only acceptance sample. A minimal plugin-free KRKR/KAG project must also be kept as the engine baseline.

## 2. Device Constraints

Known target facts:

- SoC/device family: RK3576S, aarch64;
- memory: about 2 GB;
- display: 1600x1440 landscape through Wayland/Sway, physical panel 1440x1600 at 60 Hz;
- input: `gkd_atom_joypad`, one analog stick, D-pad and standard face/shoulder buttons;
- operating system: ROCKNIX with a low-glibc-compatible runtime requirement;
- preferred display path: SDL on Wayland;
- storage: games and runtime normally live under `/storage/roms/ports`.

For 16:9 games, the product target is a 1280x720 engine surface scaled by the GPU to 1600x900, centered vertically with black bars. Rendering the game into a CPU-side 1600x1440 or 1920x1080 surface is not the default target.

## 3. Reference Source Inventory

The following repositories are checked out under `D:\Works\Tyranor`. All checkouts were clean and complete when this plan was written. Clones are shallow current-source snapshots, so the commit IDs below are the reproducible reference points.

| Repository | Local path | Commit | Primary use |
| --- | --- | --- | --- |
| krkrsdl2 | `D:\Works\Tyranor\krkrsdl2` | `ecfd1979d202088540f606098603d159d267aca4` | Current lightweight SDL2/Linux baseline and process core candidate |
| krkr2 | `D:\Works\Tyranor\krkr2` | `dca7264572a63e753c1b07ca7053513d1751ce70` | Commercial plugin implementations, FFmpeg player, Cocos render behavior |
| AetherKiri | `D:\Works\Tyranor\AetherKiri` | `d36d2ef9a35e32df8142e2eba98bd427efb315e7` | Broadest plugin compatibility, verified-game profiles, modern host API and performance probes |
| Kirikiroid2 | `D:\Works\Tyranor\Kirikiroid2` | `d1c2b1259423542c893e0b65eaeb46c848848f2b` | Proven Android ARM design, NEON pixel operations, mature Kodi-derived FFmpeg video path |
| krkrsdl3 | `D:\Works\Tyranor\krkrsdl3` | `e3853a6f709f4dcd6563490bf4884e2aa77b637b` | Compact native SDL3/FFmpeg architecture reference |
| krkrz | `D:\Works\Tyranor\krkrz` | `fd5c4baa6a2ef5978db1bd043634351f48667daf` | Canonical KRKRZ behavior and interface reference |
| krmpv | `D:\Works\Tyranor\krmpv` | `004f523e0da5c49cb3f86c8f0e711da20f39a74b` | TJS/libmpv interface reference only; current implementation is Win32-oriented |
| krmovie_ffmpeg | `D:\Works\Tyranor\krmovie_ffmpeg` | `d69449e7fbc455db2cc544f48889f9739ebf374e` | Small FFmpeg `krmovie.dll` behavior reference; current target is Windows |

`krkrsdl2` now has all 19 recursive submodules initialized, including its KRKRZ fork, SDL, FAudio, SIMDe, ANGLE, image/audio codecs and zlib.

## 4. What to Reuse

### 4.1 krkrsdl2

Use directly where possible:

- SDL2 window and event integration;
- KRKRZ/TJS runtime and storage model;
- FAudio and built-in image/audio codecs;
- Linux process and filesystem behavior;
- existing `KRKRSDL2_PATH` storage search path;
- current ROCgalgame child-process lifecycle.

Known gaps to resolve:

- Linux render path performs CPU layer composition and full-screen final copies;
- no generic virtual mouse for handheld use;
- only controller buttons are handled; controller axes and raw joystick events are missing;
- native Linux video overlay is absent;
- external Windows DLLs cannot execute on aarch64 Linux;
- Linux CMake currently builds its bundled SDL rather than using the proven device SDL explicitly.

### 4.2 krkr2

Primary compatibility source:

- `cpp/plugins/motionplayer`;
- `cpp/plugins/psbfile` and `psdfile`;
- `cpp/plugins/layerex_draw`;
- KAGParserEx, TextRender, windowEx and getSample;
- `cpp/core/movie/ffmpeg` and `layerExMovie.cpp`;
- render-manager behavior required by motion and layer plugins.

Do not import its Linux host unchanged. It currently depends on GTK, Cocos2d-x/GLFW, vcpkg and desktop Linux assumptions, and its documented Linux target is x86_64.

### 4.3 AetherKiri

Primary plugin coverage and test source:

- AlphaMovie;
- extNagano and extrans;
- multiimage and PackinOne;
- textrender;
- compatibility modules and dummy plugin stubs;
- motionplayer, PSB, PSD, layerExDraw and KAGParserEx integration;
- verified-game records and repeatable smoke/performance profiles;
- engine C API design for decoupling a runtime from its UI host.

AetherKiri has verified Yuzusoft-family titles on Android arm64, which is strong evidence for the local target game. Its Godot 4.7 product shell is not the initial GKD runtime target.

License boundary: AetherKiri is GPL-3.0-or-later. Copying or deriving its implementation can impose GPL source-distribution obligations on the combined product. Use it as a behavioral reference until the licensing decision is explicit. Track every imported file and its original license.

### 4.4 Kirikiroid2

Primary ARM and low-level performance source:

- aarch64/ARM NEON implementations in `src/core/visual/ARM`;
- FFmpeg demux, audio/video clocks, decoding and presentation;
- Android input and lifecycle behavior;
- internal plugin loading conventions;
- memory behavior proven on mobile-class devices.

The old Android/Cocos build system should not be adopted wholesale.

### 4.5 krkrsdl3

Use as a focused reference for:

- SDL-native FFmpeg integration;
- small Linux platform boundary;
- SDL texture/video presentation;
- dependency inventory.

It is currently version 0.0.6 and describes itself as KRKR-like. It is not the compatibility authority and is not the initial replacement core.

### 4.6 Video-only repositories

`krmpv` and `krmovie_ffmpeg` are interface and decoder references. Their current code uses Win32 handles, messages, libraries or DLL output. They are not drop-in aarch64 Linux dependencies.

## 5. Core Selection Strategy

Do not commit to a full engine replacement before a comparative spike.

### Candidate A: krkrsdl2 plus integrated compatibility modules

Advantages:

- smallest runtime and closest match to the existing SDL2/Wayland packaging;
- least frontend integration work;
- current source is already located where build scripts expect external cores;
- easier control over memory and frame pacing.

Risks:

- plugin sources from KrKr2/AetherKiri depend on a different render abstraction;
- video and motionplayer integration may become a large backport;
- current ARM graphics path needs measurement and likely NEON work.

### Candidate B: stripped KrKr2-derived core

Advantages:

- commercial plugins and video are already integrated;
- Android arm64 support demonstrates core-level ARM portability;
- closer to the target game's compatibility requirements.

Risks:

- Cocos2d-x, GTK, GLFW, vcpkg and desktop GL increase the porting surface;
- Linux aarch64 and Wayland/GLES are not first-class current targets;
- dependency and memory footprint may be materially larger.

### Decision gate

Run both candidates on a development PC against:

1. a minimal plugin-free KRKR/KAG test project;
2. the local `千恋万花` package;
3. a synthetic 1280x720 alpha-layer and transition stress scene.

Choose Candidate A only if the required built-in plugin layer can be integrated without replacing its renderer. Choose Candidate B if it reaches the target game's title and normal scene flow substantially earlier and its host/window layer can be isolated from Cocos/GTK.

Record the result in a short architecture decision record before Phase 2.

## 6. Target Runtime Architecture

```text
ROCgalgame frontend
  -> release renderer, window, input and audio
  -> resolve CoreLaunchSpec for selected game
  -> fork/exec KRKR child with isolated cwd, env and save path
  -> KRKR SDL/Wayland window owns display and controller
  -> wait for exit and capture exit reason/log summary
  -> rebuild frontend and restore shelf selection
```

Introduce a core-neutral launch specification with at least:

- core kind;
- executable;
- working directory;
- entry point (`data.xp3`, project directory or explicit override);
- argument vector;
- environment map;
- save path;
- log path;
- capability flags for aspect, filter, virtual mouse, video and overlays.

ONS and KRKR should use the same in-process parent/wait lifecycle. Exit code 42 and the shell request file remain recovery compatibility only, not the normal KRKR route.

## 7. Development Phases

### Phase 0: Source and compatibility baseline

Tasks:

- preserve the reference commits listed in this document;
- create a minimal redistributable KRKR/KAG test project;
- build current krkrsdl2 on Windows or desktop Linux;
- build krkr2 or AetherKiri on a supported host platform;
- run the same target game and capture every `Plugins.link` request;
- classify each requested DLL as built-in, stub-safe, script-patchable or missing;
- document licenses for every source file selected for import.

Exit criteria:

- minimal project reaches interactive text;
- one compatibility-rich core reaches the target game's title or produces a precise first blocker;
- core-selection ADR is complete.

### Phase 1: Frontend launch contract

Tasks:

- replace the ONS-only branch in `LaunchGameAndWait` with core dispatch;
- store a detected or configured KRKR entry point in `GameEntry`;
- stop assuming every KRKR game has `data.xp3`;
- preserve shelf focus and page across a core session;
- return structured launch results: missing core, exec failure, script failure, signal and normal exit;
- show a concise error after returning to the shelf;
- keep complete core logs under `logs/krkr/`.

Exit criteria:

- both ONS and a dummy KRKR executable use the same lifecycle;
- display ownership returns reliably after normal exit and crash;
- no stale request auto-launch occurs.

### Phase 2: GKD aarch64 build

Tasks:

- restore/sync `GKD350HUltra/sysroot_device`;
- add `GKD350HUltra/build_krkr.ps1` and `build_krkr.sh`;
- add a CMake aarch64 Linux toolchain file using the device sysroot;
- use Release or RelWithDebInfo with explicit `-O2` initially;
- emit `readelf -d`, RPATH and unresolved-library reports;
- add KRKR to `build_package.sh`, executable checks and package permissions;
- stage the binary under `cores/krkr/`;
- keep the external source path configurable through `KRKR_ROOT`.

Dependency policy:

- prefer the known device/system SDL2 for the first krkrsdl2 build;
- bundle a complete, isolated PortsMaster runtime closure only when the device library is missing or incompatible;
- never mix arbitrary SDL/Wayland libraries from multiple roots in one process;
- use `$ORIGIN`-relative RPATH and an engine-specific library directory.

Exit criteria:

- KRKR binary starts on-device through Wayland;
- `readelf` shows only intended runtime dependencies;
- missing libraries fail at package validation rather than on-device launch.

### Phase 3: Display and render path

Tasks:

- force fullscreen desktop on GKD without changing the physical display mode;
- retain the game's native 1280x720 layer surface;
- implement `contain` as the first supported aspect policy;
- scale with the GPU to 1600x900 and clear letterbox regions;
- verify mouse/touch coordinate translation through the viewport;
- report SDL renderer name, acceleration state, vsync state and logical/output size;
- expose `-contfreq=60` and a 30 fps performance mode;
- test `-drawthread=1`, `2`, `4` and `auto` rather than assuming more threads are faster;
- start with `-gclim=96` or `128` on the 2 GB target;
- compare async image loading on and off.

Exit criteria:

- no crop or stretch at 1280x720;
- input coordinates match rendered controls at all viewport edges;
- renderer remains accelerated and vsynced;
- idle dialogue does not run an unrestricted busy loop.

### Phase 4: Handheld input and core overlay

Implement both SDL GameController and raw SDL joystick paths. The GKD device must remain usable even when SDL does not classify it as a mapped GameController.

Default game profile:

| GKD control | KRKR action |
| --- | --- |
| Left stick | Continuous virtual mouse |
| D-pad | Arrow keys; optional pointer-step mode per game |
| Logical A / east face | Left click, advance text, hold to drag |
| Logical B / south face | Right click / cancel |
| L1 / R1 | Wheel up / wheel down |
| X | Configurable Ctrl/skip hold |
| Y | Configurable Space/auto action |
| Start | Escape or game menu |
| Menu/function | ROC core overlay |

The overlay must provide continue, pointer speed, 30/60 fps, control profile, log status and exit confirmation. It must release every synthesized key/button when opened or when the controller disconnects.

Exit criteria:

- title, settings, save/load, backlog and dialogue are fully usable without keyboard/touch;
- press, hold, drag and release cannot leave stuck input;
- reconnecting the controller restores operation;
- exiting cannot corrupt saves.

### Phase 5: Built-in plugin compatibility

Implement an internal plugin registry that maps Windows DLL names requested by scripts to native built-in modules. Do not attempt to load PE DLL files on aarch64 Linux.

Initial target-game priority:

1. motionplayer;
2. KAGParserEx;
3. layerExDraw;
4. PSB/PSD;
5. TextRender and windowEx;
6. AlphaMovie;
7. extNagano and extrans;
8. multiimage and PackinOne;
9. getSample and menu/dialog compatibility;
10. safe stubs for platform-only Steam, OLE and touch integrations.

For every plugin alias, record:

- requested DLL name and case variants;
- source repository and commit;
- exported TJS classes/functions;
- whether it is a full implementation, compatibility implementation or no-op stub;
- games and flows used to verify it;
- license and attribution requirements.

Exit criteria:

- target game reaches normal dialogue without loading any Windows binary;
- missing plugin errors identify the exact requested symbol/module;
- no-op stubs are used only for behavior proven nonessential.

### Phase 6: Video

Start with the KrKr2/Kirikiroid2 FFmpeg implementation as the compatibility reference and the krkrsdl3 path as the SDL presentation reference.

Required first implementation:

- open video from KRKR storage/XP3 streams;
- decode WMV, MP4 and common audio tracks;
- present opaque 720p video through an SDL YUV texture where possible;
- support play, pause, stop, rewind, loop and end events;
- preserve audio/video clock synchronization;
- allow A/B/Menu to skip where the game permits;
- support full-screen OP/ED playback before alpha-layer video.

Deferred work:

- RKMPP/V4L2 hardware decode;
- transparent/dual-stream video;
- arbitrary video-as-layer blending;
- subtitle systems not requested by a target game.

An external PortsMaster `mpv` process may be retained as a temporary fallback for opaque full-screen movies, but it is not the final layer-video API.

Exit criteria:

- all local 720p WMV samples play with synchronized audio;
- playback can be skipped and returns control to the script;
- 30 minutes of repeated playback produces no leak or audio-device conflict.

### Phase 7: Performance and memory

Add runtime counters before optimization:

- CPU frame time and present time;
- p50/p95/p99 frame interval;
- dirty/upload rectangle pixels and bytes;
- process RSS and peak RSS;
- image cache size;
- decode queue depth;
- audio underruns;
- device temperature/frequency when readable.

Initial product budgets:

- native engine surface: 1280x720 by default;
- normal dialogue and UI: responsive 60 Hz input/presentation;
- heavy transitions/motion: p95 frame interval below 33.3 ms in 30 fps mode;
- peak process RSS: target below 1.2 GB, hard investigation threshold 1.4 GB;
- no sustained unrestricted idle loop;
- no audio underruns during transitions or video;
- one-hour play session without OOM or thermal failure.

Optimization order:

1. stop unnecessary continuous-event spinning;
2. cap frame production independently of script clock correctness;
3. retain 720p surfaces and minimize full-frame uploads;
4. use Kirikiroid2 NEON operations for measured hot blend functions;
5. tune draw-thread count;
6. tune graphic and async-image caches;
7. consider GPU layer composition only after profiling proves it necessary.

### Phase 8: Packaging and release

Tasks:

- include `cores/krkr/<binary>` and any engine-specific runtime libraries;
- include FFmpeg/PortsMaster notices and all imported source licenses;
- add package-time dependency validation;
- prevent game/plugin DLL files from leaking into the distributed runtime;
- preserve user saves during core upgrades;
- document per-game overrides in `game.ini`;
- version core compatibility independently from the frontend version.

Suggested per-game keys:

```ini
core=krkr
entry=data.xp3
aspect=contain
virtual_mouse=1
mouse_speed=720
frame_limit=60
draw_threads=auto
graphic_cache_mb=96
async_image_load=1
plugin_profile=auto
```

Exit criteria:

- clean package installs without network access;
- core starts with the package's own validated runtime closure;
- upgrading the package preserves saves and game configuration;
- license/attribution files match the shipped binary contents.

## 8. Test Matrix

| Test | Purpose | Required result |
| --- | --- | --- |
| Minimal KAG project | Core script/render baseline | Boot, text, choices, save/load |
| 1280x720 alpha stress scene | Blend and upload pressure | Stable 30/60 modes, no corruption |
| Target game title | Plugin discovery | No Windows binary load attempt |
| Target game normal scene | Motion/text/audio | Correct animation and voice |
| Target game save/load | Writable path isolation | No writes required in game folder |
| WMV OP/ED loop | Video and audio clock | Sync, skip, cleanup |
| Controller disconnect/reconnect | Input robustness | No stuck input, recovery succeeds |
| Core crash/forced exit | Frontend lifecycle | Shelf returns with useful error |
| One-hour session | Memory/thermal stability | No OOM, leak trend or audio loss |

## 9. Principal Risks

| Risk | Mitigation |
| --- | --- |
| Plugin API divergence between engines | Compile one plugin at a time behind an internal registry; keep behavior tests |
| 2 GB memory pressure | 720p surface, explicit cache cap, async-load A/B tests, RSS telemetry |
| CPU-bound alpha animation | NEON hot paths, frame cap, dirty-region measurement, draw-thread tuning |
| Wayland/SDL ABI conflicts | One engine-specific runtime closure and package-time `readelf` validation |
| Linux arm64 unsupported by a candidate build | Keep the source portable behind a CMake toolchain and avoid desktop-only host code |
| Video scope expands into full media framework | Deliver opaque full-screen FFmpeg playback first; defer alpha and hardware decode |
| GPL contamination from AetherKiri | Treat it as reference unless the project explicitly adopts GPL; maintain source provenance |
| Commercial game-specific patches become global hacks | Put exceptions in per-game profiles and require a reproducible failing test |

## 10. Recommended First Milestone

The first milestone should not be a polished package. It should prove the following vertical slice:

1. Build complete krkrsdl2 on a host system.
2. Build one compatibility-rich KrKr2/AetherKiri-derived host runtime.
3. Launch the minimal KAG project in both.
4. Launch `千恋万花` and capture the real plugin request sequence.
5. Select the core using the decision gate in Section 5.
6. Cross-compile the selected core to aarch64.
7. Display the title screen at 1280x720 contained on GKD Wayland.
8. Operate it using the virtual mouse and exit cleanly to ROCgalgame.

Only after this milestone should full plugin coverage, video and performance tuning proceed in parallel.
