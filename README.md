# ROCgalgame

ROCgalgame is a clean SDL2/C++ frontend for Linux handheld visual novel runtimes. The first target is GKD350H Ultra, with a 1600x1440 Wayland-oriented layout copied selectively from ROCreader. The first playable core is OnscripterYuri; KRKR stays planned until the ONS experience is polished.

The frontend owns the game library, covers, settings, and device-friendly controls. ONS runs as a child process managed directly by the still-running frontend process. Before starting ONS, the frontend destroys its SDL renderer, window, audio, and input state so the core can take over the display without a competing black surface. When ONS exits, the frontend rebuilds its SDL state and returns to the cover shelf.

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
  games/
    <game folders>     # each ONS game has its own folder
  covers/
  saves/
  cache/
```

## Controls

- D-pad / left stick: move focus
- A: launch focused game or confirm menu item
- B: close menu
- Menu / Start: open settings menu
- L1 / R1: previous / next page
- Y: rescan library

The current menu already persists the first core-facing settings to `native_config.ini`: virtual mouse on/off, aspect mode, filter mode, and mouse cursor speed. These values are also written to `cache/launch_request.ini` for the runtime core.

## Core Contract

Selecting an ONS cover with A does not exit the frontend or return control to `ROCgalgame.sh`. The frontend records the selected game internally, fully releases its SDL resources, starts `onsyuri` as a child process, and waits for it. This ordering is required on Wayland/KMS devices: keeping the cover window alive as a black surface can obscure the ONS window. After the core exits, the same frontend process initializes SDL again and restores the cover shelf.

`cache/launch_request.ini` and exit code `42` remain only as a fallback contract for future non-ONS cores. A request left by an interrupted older build is discarded at cold startup and is never auto-launched.

The fallback launch file is intentionally simple:

```ini
core=ons
path=/storage/roms/ports/ROCgalgame/games/example
save=/storage/roms/ports/ROCgalgame/saves/ons/example
encoding=gbk
aspect=contain
filter=clean
virtual_mouse=1
mouse_speed=720
mouse_accel=1.6
```

Aspect values currently used by the frontend are:

- `stretch`: scale to the full panel.
- `fit-width`: keep source ratio and fill horizontally.
- `fill-height`: keep source ratio and fill vertically, allowing horizontal crop if needed.
- `contain`: keep source ratio inside the panel without crop.

Filter values are `clean`, `scanline`, `crt-soft`, and `mask`. The frontend records the chosen value now; the actual shader/filter implementation belongs in each core wrapper or in a later shared render layer.

## Core Integration Notes

The frontend launches the ONS core as a child process after releasing its SDL resources, then rebuilds the frontend when the core exits. `ROCgalgame.sh` retains the exit-code fallback only for future non-ONS cores.

Initial expected core path:

- ONS: `ROCgalgame/cores/ons/onsyuri`

The game scanner recognizes ONS markers such as `0.txt`, `00.txt`, `nscript.dat`, `arc.nsa`, and `arc.sar` directly under each folder in `games/`. A legacy `games/ons/` bucket is still scanned, but the intended package layout is flat under `games/`.

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

`game.ini` values are optional. Missing values fall back to `native_config.ini`, and save data still uses `saves/<core>/<folder-name>/` so changing the display title does not move existing saves.

For handheld use, ROCgalgame passes ONS runtime settings through environment variables. `ROCGALGAME_VIRTUAL_MOUSE=1` enables the patched ONS virtual mouse layer. The D-pad navigates between real ONS `ButtonLink` hit regions in four directions and centers the cursor on the selected button. The left stick moves continuously for controls such as sliders and scrollbars; approaching a button snaps to its center, with a short escape threshold so the stick can pull away. The GKD east face button sends a normal left-button down/up pair: tapping a button selects it, tapping empty dialogue advances, and holding while moving the stick drags. The cursor is a white line ring with a center point and contracts while the button is held. The south face button keeps the native cancel/right-click behavior. Aspect settings currently map to OnscripterYuri's existing fullscreen behavior: `stretch` uses `--fullscreen2`, while the other modes use `--fullscreen` and preserve the game's aspect.

## Build

Windows interactive preview (MSYS2 UCRT64):

```text
Windows\run_windows_preview.bat
```

The script incrementally builds the native Windows frontend and OnscripterYuri core, then opens the 1000x900 preview against the real local `games` directory. Arrow keys simulate the D-pad, `A` (or `Z`) simulates A, and `B` (or `X`) simulates B. `I/J/K/L` simulate continuous left-stick movement. Tap A to click or advance dialogue; hold A while using `I/J/K/L` to test dragging. The generated Windows binaries stay under `Windows/build` and `cores/ons/onsyuri.exe`.

Local Linux build:

```sh
make
```

GKD350H Ultra cross build from WSL:

```powershell
powershell -ExecutionPolicy Bypass -File GKD350HUltra\build_low_glibc.ps1
powershell -ExecutionPolicy Bypass -File GKD350HUltra\build_onsyuri.ps1
```

One-command package build:

```powershell
powershell -ExecutionPolicy Bypass -File GKD350HUltra\build_package.ps1
```

The package script builds the frontend, builds OnscripterYuri, verifies the launcher/core files, prints ELF dependency info when `readelf` is available, and writes `GKD350HUltra/Downloads/ROCgalgame_GKD350HUltra_v0.1.zip` plus a `.tar.gz` fallback.

The GKD sysroot and helper scripts were copied from `D:\Works\ROCreader\GKD350HUltra` as the initial target toolchain baseline.
The checked-in `GKD350HUltra/toolchain` folder is currently a placeholder/notes directory. The working compiler used by the build script is the WSL Ubuntu `aarch64-linux-gnu-g++`, unless `CROSS_CXX` or a populated `GKD350HUltra/toolchain/bin/aarch64-linux-gnu-g++` is provided.

## Repository Contents

This repository intentionally excludes game files, game covers, save data, generated core binaries, device sysroots, build outputs, logs, and release archives. Keep private game content under `games/` and optional local covers under `covers/` or `game_covers/`; these paths are ignored by Git.

After cloning, the build and packaging scripts create the empty runtime `games/`, `covers/`, `saves/`, `cache/`, and `cores/ons/` directories as needed. Build OnscripterYuri separately before running a game; no game or third-party core binary is distributed from this source repository.
