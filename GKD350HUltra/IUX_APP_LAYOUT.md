# GKD350H Ultra IUX Application Layout

The GKD350H Ultra image inspected on 2026-07-29 runs ROCKNIX with the IUX
frontend. The external card is exposed as follows:

- `/storage/games-external` is the card root.
- `/storage/roms` is the card's `roms` directory.
- IUX reads external applications from `/storage/games-external/app`.

Each visible application is a first-level directory containing `config.json`.
The ROC packages therefore use this layout:

```text
app/
  ROCgalgame/
    config.json
    launch.sh
    rocgalgame.png
    rocgalgame_sdl
    ...
  ROCreader/
    config.json
    launch.sh
    rocreader.png
    rocreader_sdl
    ...
roms/
  ports/
    ROCgalgame.sh
    ROCreader.sh
```

The two `roms/ports` scripts are ES compatibility entries. On ROCKNIX they
forward to the matching `app/<name>/launch.sh`; when no IUX installation is
present they retain the legacy nested-runtime behavior. Runtime binaries and
user data are not duplicated under `ports`.

IUX recognizes `software_code`, `title`, `description`, `version`, `exec`,
`workdir`, and `icon`. A signed `security_manifest.json` is not required for
third-party apps; the installed `yoocom` app is a working unsigned example.
Each repository keeps its IUX icon source at `ui/common/icon.png`; package
validation checks that this exact source is shipped under an app-specific
name. Do not reference the generic filename `icon.png` from `config.json`.
The tested IUX build first resolves the configured value as a skin icon, so
`icon.png` matches `skins/Default/icons/icon.png` and prevents the external
application file from being used. Unique names such as `rocgalgame.png` and
`rocreader.png` avoid that collision. The current assets are `455x270` RGBA
PNGs, matching the known working third-party application.

## Migration

The old installations are under `roms/ports`. Move the complete runtime
directories before extracting the first IUX-format releases at the card root:

```sh
mkdir -p /storage/games-external/app
mv /storage/roms/ports/ROCgalgame /storage/games-external/app/ROCgalgame
mv /storage/roms/ports/ROCreader /storage/games-external/app/ROCreader
```

Then extract each new release at `/storage/games-external`. Extraction writes
the new launchers, application metadata, icons, and runtime files over the
moved directories without deleting user data. Keep the newly extracted
`roms/ports/*.sh` files: they are the ES entries that forward to the IUX
runtime.

Back up both directories before migration. Do not extract the new archives
under `roms`; their top-level `app` directory is relative to the card root.

## Update Compatibility

Both launchers install downloaded updates from the new `app/<name>` archive
layout and retain readers for the legacy `roms/ports` layouts. Release ZIPs
also carry the two ES forwarding scripts, and each in-app updater refreshes
its ES script when installing a new package. Downloads,
pending markers, and extraction staging remain inside each application
directory, so relocating the complete directory keeps the update workflow
self-contained.

The first move from a legacy installation must be manual. The dual archive
does not duplicate the full runtime under `ports`, so an old launcher cannot
move itself into `app`. Once the IUX-format release is in place, subsequent
in-app updates remain automatic.

ROCgalgame stores normalized absolute paths in `cache/favorites.txt` and
`cache/history.txt`. Its new launcher rewrites legacy
`/storage/roms/ports/rocgalgame` prefixes on first start. ROCreader stores its
books, covers, cache, and downloaded updates below its runtime directory, so
moving the complete directory preserves those paths through `ROCREADER_ROOT`.

IUX also provides a separate web-file-manager update format using root-level
`update.json` plus `payload/`. That format only updates an already installed
app selected by `software_code`; it is independent of the in-app GitHub ZIP
updaters used by ROCgalgame and ROCreader.
