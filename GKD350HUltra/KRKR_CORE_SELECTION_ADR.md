# KRKR Core Selection ADR

Status: selected for the first device vertical slice; pending target-game measurements

Date: 2026-07-15

## Decision

Use `krkrsdl2` as the first GKD350H Ultra vertical-slice core. Keep AetherKiri,
Kirikiroid2, KrKr2 and krkrsdl3 as compatibility, SIMD and video references;
do not import their GPL or platform-specific implementation code yet.

## Reasons

- It already owns an SDL2 window and supports Linux fullscreen desktop mode.
- Its logical-size renderer provides the correct starting point for containing
  a 1280x720 surface on the 1600x1440 output.
- It already accepts `-contfreq`, `-drawthread`, `-gclim` and `-datapath`.
- It contains a SIMDe path intended to translate x86 SIMD code on ARM.
- Its runtime boundary matches the frontend's existing SDL/Wayland lifecycle.

## Evidence Collected

- The complete source and all recursive submodules are present locally.
- Windows/MSVC configuration succeeded but compilation failed in code that is
  oriented toward MinGW and the Windows DirectShow path.
- Windows/MinGW completed the full source build and produced a 64-bit host core
  using one job, BelowNormal priority and two-CPU affinity.
- The host core loaded the plugin-free project, created its 1280x720 layer,
  rendered English and Chinese text with `fonts/ui_font_02.ttf`, and closed
  cleanly. Observed working set was about 69 MB.
- Earlier interrupted parallel builds left zero-filled object files and
  coincided with unexpected Windows power-offs. The successful low-load build
  demonstrates that the source can build without high host concurrency.
- Frontend launch-spec tests cover directory and non-`data.xp3` entry points.

## Open Decision Gate

The device decision remains open until the aarch64 build runs the minimal TJS
project and the target game reaches a useful compatibility checkpoint. Switch
to a KrKr2-derived core if plugin integration requires
replacing most of the krkrsdl2 renderer or movie stack.
