# ROCgalgame

ROCgalgame is an SDL2/C++ frontend for Linux handheld visual novel runtimes. Its frontend structure and common behavior follow ROCreader, while game-specific differences are isolated in game models, callbacks, core adapters, and composition code. The first verified package target is GKD350H Ultra with a 1600x1440 Wayland-oriented layout. OnscripterYuri is the stable gameplay path; KRKRSDL2 remains an experimental core.

The frontend owns the game library, covers, settings, and device-friendly controls. ONS and KRKR run as child processes managed by the still-running frontend process. Before starting a core, the frontend destroys its SDL renderer, window, audio, and input state so the core can take over the display without a competing black surface. When the core exits, the frontend rebuilds its SDL state and returns to the matching cover shelf. See [CURRENT_PORT_STATUS.md](CURRENT_PORT_STATUS.md) for the current capability boundary and known KRKR issues.

## Runtime Layout

```text
Roms/ports/ROCgalgame.sh
Roms/ports/ROCgalgame/
  rocgalgame_sdl
  native_config.ini
  native_keymap.ini
  fonts/
  sounds/
  ui/
  cores/
    ons/onsyuri        # OnscripterYuri binary
    krkr/krkrsdl2      # experimental KRKRSDL2 binary
  games/
    <game folders>     # flat ONS and KRKR game folders
  covers/
  saves/
  cache/
```

## Controls

- D-pad / left stick: move focus
- A: launch focused game or confirm menu item
- B: return from a panel or close the menu
- Menu / Start: open or close the settings menu
- L1 / R1: previous / next category
- X: add the focused game to favorites
- Y: remove the focused game from favorites

Game Settings persist virtual mouse, aspect mode, filter mode, mouse speed, and mouse acceleration to `native_config.ini`. The selected game and optional `game.ini` overrides are converted directly into an immutable core launch spec.

## Core Contract

Selecting an ONS or KRKR cover with A does not exit the frontend. The frontend records the selected game internally, fully releases its SDL resources, starts the selected core as a child process, and waits for it. This ordering is required on Wayland/KMS devices: keeping the cover window alive as a black surface can obscure the core window. After the core exits, the same frontend process initializes SDL again and restores the cover shelf.

There is one launch pipeline: `GameLaunchService` selects an `IGameCoreAdapter`, builds a typed `CoreLaunchSpec`, and passes it to `CoreProcessRunner`. The old `cache/launch_request.ini` and exit-code `42` launcher protocol have been removed.

Aspect values currently used by the frontend are:

- `stretch`: scale to the full panel.
- `fill-height`: keep source ratio and fill vertically, allowing horizontal crop if needed.
- `contain`: keep source ratio inside the panel without crop.

Filter values are `clean`, `scanline`, `crt-soft`, and `mask`. The frontend records the chosen value now; the actual shader/filter implementation belongs in each core wrapper or in a later shared render layer.

## Core Integration Notes

The frontend launches either core as a child process after releasing its SDL resources, then rebuilds the frontend when the core exits. `ROCgalgame.sh` only prepares the runtime environment and starts the frontend.

Expected core paths:

- ONS: `ROCgalgame/cores/ons/onsyuri`
- KRKR: `ROCgalgame/cores/krkr/krkrsdl2`

The game scanner recognizes ONS markers such as `0.txt`, `00.txt`, `nscript.dat`, `arc.nsa`, and `arc.sar`, plus KRKR markers such as `startup.tjs`, `Config.tjs`, `data.xp3`, and other XP3 archives. The intended package layout is flat under `games/`; legacy `games/ons/` and `games/krkr/` buckets remain compatible. The ONS and KRKR navigation tabs filter the same library by detected core.

External covers are read from the configured `covers_root` and also from `game_covers` as a compatibility fallback. Each game may include an optional `game.ini` to override global launch settings:

```ini
title=Game Title
core=ons
encoding=gbk
aspect=contain
filter=clean
virtual_mouse=1
mouse_speed=720
mouse_accel=1.6
```

`game.ini` values are optional. Missing values fall back to `native_config.ini`, and save data still uses `saves/<core>/<folder-name>/` so changing the display title does not move existing saves. Legacy `fit-width` in the global config is migrated to `contain`.

For handheld use, ROCgalgame passes ONS runtime settings through environment variables. `ROCGALGAME_VIRTUAL_MOUSE=1` enables the patched ONS virtual mouse layer. The D-pad navigates between real ONS `ButtonLink` hit regions in four directions and centers the cursor on the selected button. The left stick moves continuously for controls such as sliders and scrollbars; approaching a button snaps to its center, with a short escape threshold so the stick can pull away. The GKD east face button sends a normal left-button down/up pair: tapping a button selects it, tapping empty dialogue advances, and holding while moving the stick drags. The cursor is a white line ring with a center point and contracts while the button is held. The south face button keeps the native cancel/right-click behavior. For ONS, `stretch` uses `--fullscreen2`, `contain` preserves the complete game frame, and `fill-height` fills the panel vertically while cropping the scene horizontally. Standard ONS dialogue text is reflowed inside the visible horizontal safe area in `fill-height` mode.

## Build

Windows frontend-only build (MSYS2 UCRT64):

```powershell
$env:MSYSTEM = "UCRT64"
$env:CHERE_INVOKING = "1"
& "D:\Program Files\MSYS2\usr\bin\bash.exe" -lc `
  'export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Works/ROCgalgame; make -j2 TARGET=Windows/build/rocgalgame_sdl.exe OBJDIR=Windows/build/frontend-obj'
```

This command builds only the frontend and does not rebuild ONS or KRKR. The executable supports deterministic screenshot capture through `ROCGALGAME_CAPTURE_FRAME`, `ROCGALGAME_EXIT_AFTER_CAPTURE`, `ROCGALGAME_OPEN_MENU`, `ROCGALGAME_MENU_INDEX`, `ROCGALGAME_OPEN_PANEL`, `ROCGALGAME_PREVIEW_VOLUME`, and `ROCGALGAME_NAV_INDEX`. `Windows\run_windows_preview.bat` also builds OnscripterYuri and should only be used when core validation is explicitly intended.

Local Linux build:

```sh
make
```

GKD350H Ultra cross build from WSL:

```powershell
powershell -ExecutionPolicy Bypass -File GKD350HUltra\build_low_glibc.ps1
powershell -ExecutionPolicy Bypass -File GKD350HUltra\build_onsyuri.ps1
powershell -ExecutionPolicy Bypass -File GKD350HUltra\build_krkr.ps1 -Mode Fast -Jobs 1 -ConfirmHeavyBuild
```

Canonical GKD350H Ultra release build (Docker, frontend-only):

```powershell
.\GKD350HUltra\build_release_docker.ps1
```

The script automatically advances `0.01`, `0.02`, and so on based on existing release archives. It performs a clean frontend cross-build in Docker, verifies the preserved ONS/KRKR binaries against `release_core_hashes.sha256`, and writes `GKD350HUltra\Downloads\ROCgalgame verX.XX for GKD350H Ultra.zip`. The archive root is `roms/ports`, with `ROCgalgame.sh` and the complete `ROCgalgame/` runtime beneath it. Pass `-Version 0.01` to reproduce a specific release number.

For UI/config-only work, synchronize and validate the staged runtime without compiling or creating an archive:

```powershell
powershell -ExecutionPolicy Bypass -File GKD350HUltra\build_package.ps1 -Mode Fast -Output Stage
```

For the next UI/ONS development pass, build only those targets and reuse the staged KRKR core:

```powershell
powershell -ExecutionPolicy Bypass -File GKD350HUltra\build_package.ps1 -Mode Incremental -BuildTargets Frontend,ONS -Output Stage
```

Create a distributable zip only when needed by adding `-Output Zip`. Both the manual and Docker package paths use the same `roms/ports` release layout and package naming. Building KRKR is always explicit and uses the preserved `build/gkd350h/krkrsdl2` CMake tree. Full rebuilds are reserved for toolchain/ABI changes, incompatible CMake option changes, cache corruption, or major KRKR restructuring.

The GKD sysroot and helper scripts were copied from `D:\Works\ROCreader\GKD350HUltra` as the initial target toolchain baseline.
The checked-in `GKD350HUltra/toolchain` folder is currently a placeholder/notes directory. The working compiler used by the build script is the WSL Ubuntu `aarch64-linux-gnu-g++`, unless `CROSS_CXX` or a populated `GKD350HUltra/toolchain/bin/aarch64-linux-gnu-g++` is provided.

See [ARCHITECTURE.md](ARCHITECTURE.md) for maintained module boundaries, the adapter contract, platform capabilities, protected paths, and intentional differences from ROCreader.

## Repository Contents

This repository intentionally excludes game files, game covers, save data, generated core binaries, device sysroots, build outputs, logs, and release archives. Keep private game content under `games/` and optional local covers under `covers/` or `game_covers/`; these paths are ignored by Git.

After cloning, the build and packaging scripts create empty mutable runtime directories and both core directories as needed. Build the third-party cores separately before running games; no game or third-party core binary is distributed from this source repository.
