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
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-rocgalgame-input.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-gkd-display-input.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-gkd-input-continuous.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-gkd-resolution.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-frontend-input-bridge.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-rocgalgame-console-throttle.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-performance-pointer-incremental.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-input-transport-refactor.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-input-transport-hup-backoff.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-input-transport-reconnect-sequence.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-linux-wayland-gles2.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-linux-wayland-messagebox.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-tjs-empty-string.patch
& D:\Works\ROCgalgame\GKD350HUltra\apply_krkr2_layerex_compat.ps1
```

The first patch adds system SDL2 selection, MSVC flag compatibility and GNU
static-library grouping. The second registers a `-deffont` TTF/TTC/OTF path
and uses its internal face name, which is required for `ui_font_02.ttf`.
The XP3 patches mount every archive and its internal directories before
`startup.tjs`, keep numbered patch archives last, and expose a directory-based
project as `System.exePath` for Kirikiroid-compatible games.

The KRKR2 input patch consumes ROCgalgame's existing virtual mouse, speed,
acceleration, A/B swap, and exit-chord environment contract. Standalone KRKR2
launches keep their original preference-based key mapping.

The GKD display/input follow-ups normalize the raw `gkd_atom_joypad` button and
axis numbers selected by the existing `gkd350h-ultra` input profile. Joystick
axis events update retained state, and the shared mouse speed/acceleration
settings then move KRKR2's own cursor continuously once per rendered frame.

The frontend bridge sends the unified `InputManager` output as latest-axis
state (`A x y sequence`) rather than queued pixel deltas. The performance
follow-up consumes only the newest state per Cocos frame, disables duplicate
native controller handling while the bridge is configured, removes forced
console redraws, and logs GL/frame-time diagnostics every five seconds.

The input transport follow-up moves FIFO reads and protocol parsing to a
dedicated reader thread. The thread only publishes an atomic-style snapshot
and a bounded ordered button/key queue; Cocos and TJS calls remain on the main
thread. Pointer integration uses a steady clock, so a 700 ms render frame does
not turn valid movement into zero, while explicit disconnect/background reset
still prevents a recovery jump. Queue overflow triggers button/key state
reconciliation instead of silently leaving a key stuck.

The transport reliability follow-ups close and back off a FIFO that reports
`POLLHUP`/`POLLERR`, preventing a disconnected reader thread from busy-looping.
They also reset the latest-axis sequence on an explicit disconnect so a
recreated frontend writer can restart its sequence and resume pointer input.

The TJS empty-string patch keeps the engine's legacy null-string ABI valid under
GCC `-O3`, makes empty string property access safe, and prevents error handling
and asynchronous script operations from crashing in `GetLength`, string
conversion, or `Release`.

The layerEx compatibility bundle adds native, game-independent implementations
of `layerExImage.dll`, `layerExRaster.dll`, and `layerExBTOA.dll`. The apply
script is idempotent and only copies the four bundled compatibility sources and
registers the three translation units in KRKR2's plugin target. Games must link
these modules before constructing their Layer instances so KRKR2's native class
registration is visible to those instances.
