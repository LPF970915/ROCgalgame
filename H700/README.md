# ROCgalgame H700 / RG34XX SP

This target packages ROCgalgame for the Allwinner H700 34xxSP profile at
`720x480`. The generated archive uses the stock H700 application layout:

```text
Roms/APPS/ROCgalgame.sh
Roms/APPS/ROCgalgame/
```

The frontend already contains the `h700-34xxsp` input map and 720x480 layout.
`build_package.ps1` reuses the low-glibc runtime produced by the existing
GKD350HUltra build, copies the H700 device libraries, and adds the FFmpeg
libraries required by `krkrsdl2`. It does not include games, covers, or saves.

The package launcher defaults to `KMSDRM` for the frontend and `x11` for the
KRKR2 host. Set `ROCGALGAME_KRKR_DISPLAY_BACKEND=wayland` on firmware that
provides a native Wayland compositor, or set it to `x11` when the stock X11
display is available.

Build from PowerShell after the three low-glibc cores and frontend exist:

```powershell
.\H700\build_package.ps1 -Output Zip -Version 0.01
```

The output is written to `H700/Downloads/ROCgalgame verX.XX for H700 34xxSP.zip`.
Use `-Output Stage` to inspect the unpacked tree without creating an archive.

