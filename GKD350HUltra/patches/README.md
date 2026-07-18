# KRKR Source Patches

These patches reproduce the changes currently applied to
`D:\Works\Tyranor\krkrsdl2` and its `external/krkrz` submodule. Apply them
only to clean source at the commits recorded in the porting plan.

From PowerShell:

```powershell
git -C D:\Works\Tyranor\krkrsdl2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkrsdl2-gkd-cmake.patch
git -C D:\Works\Tyranor\krkrsdl2\external\krkrz apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkrz-default-font-file.patch
git -C D:\Works\Tyranor\krkrsdl2\external\krkrz apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkrz-xp3-project-automount.patch
git -C D:\Works\Tyranor\krkrsdl2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkrsdl2-xp3-project-automount.patch
```

The first patch adds system SDL2 selection, MSVC flag compatibility and GNU
static-library grouping. The second registers a `-deffont` TTF/TTC/OTF path
and uses its internal face name, which is required for `ui_font_02.ttf`.
The XP3 patches mount every archive and its internal directories before
`startup.tjs`, keep numbered patch archives last, and expose a directory-based
project as `System.exePath` for Kirikiroid-compatible games.
