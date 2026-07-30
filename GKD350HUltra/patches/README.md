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
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-pointer-engine-delta.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-input-transport-hup-backoff.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-input-transport-reconnect-sequence.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-linux-wayland-gles2.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-linux-wayland-messagebox.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-tjs-empty-string.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-tjs-bytecode-bounds.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-tjs-regexp-legacy-hex.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-text-stream-cipher-header.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-linux-command-line.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-kag-emb-escape.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-psb-load-safety.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-fstat-delete-missing.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-gpu-presentation.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-shared-post-update-fbo-restore.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-mali-safe-render.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-presentation-capture.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-pointer-request.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-motion-source-metadata.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-motion-texture-cache.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-motion-texture-cpu-release.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-motion-parameter-cycle-guard.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-motion-camera-psbv4-compat.patch
git -C D:\Works\Tyranor\krkr2 apply --recount D:\Works\ROCgalgame\GKD350HUltra\patches\krkr2-rocgalgame-opengl-default.patch
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

The TJS bytecode-bounds patch validates interpreter entry and instruction
pointers. Incompatible exception targets now produce a controlled
`ByteCodeBroken` script error with diagnostics instead of reading an invalid
instruction address and raising `SIGSEGV`.

The TJS legacy RegExp hex patch translates JavaScript-style two-digit
`\\xHH` and legacy KiriKiri four-digit `\\xHHHH` escapes to UTF-16-safe
`\\u00HH` and `\\uHHHH` before invoking Oniguruma. Escaped backslashes and
the already supported `\\x{...}` form are unchanged.

The encrypted-text header patch consumes the complete three-byte cipher
signature and two-byte UTF-16LE BOM before decoding mode-0 and mode-1 TJS text.
It also validates the BOM and code-unit length and reads little-endian units
without unaligned pointer access. This fixes standard encrypted scripts that
were previously shifted by one byte and misreported as broken bytecode.

The KAG emb escape patch implements the standard `escape` attribute for
`[emb]`. Escaping remains enabled by default, while `escape=false` permits an
expression to generate executable KAG tags such as `[call storage=...]`.

The PSB load-safety patch removes the uninitialized file-local media pointer,
stops immediately after a failed parse, and registers successfully decoded
resources through the existing process-owned `PSBMediaRegistry`.

The fstat delete patch treats a missing first-save temporary file as a normal
delete miss. It normalizes the requested target directly instead of passing an
empty `TVPGetPlacedPath` result to the storage-media dispatcher.

The GPU presentation patch restores Cocos' screen framebuffer and viewport
before the Kirikiri layer texture is attached to the final sprite. It also
updates the adapter texture owner when same-sized draw buffers are exchanged,
preventing a stale or prematurely released OpenGL texture from being shown.

The shared post-update patch replaces the translation-unit-local render
callback with one function-local static slot shared by every inline caller.
Both the normal Cocos frame and nested dialog redraw loop now restore the
screen framebuffer before drawing, preventing save/load layers from flickering
between valid content and an offscreen render target.

The Mali safe-render patch defaults Mali GPUs to the regular FBO and shader
paths instead of framebuffer-fetch and clear-texture extension shortcuts. Set
`ROCGALGAME_KRKR_MALI_FAST_PATHS=1` for an A/B run of the old shortcuts. Set
`ROCGALGAME_KRKR_PRESENTATION_PROBE=1` to log sampled final-layer pixels and GL
status without enabling per-frame readback.

The presentation-capture patch adds an opt-in test hook for real-device visual
verification. Set `ROCGALGAME_KRKR_PRESENTATION_CAPTURE_REQUEST` to a request
file path, then write a destination PPM path into that file. The core consumes
one request and captures the current final Kirikiri texture. With the variable
unset, the hook does no polling or framebuffer readback.

The pointer-request patch adds an opt-in real-device interaction hook. Set
`ROCGALGAME_KRKR_POINTER_REQUEST` to a request file, then atomically write
`X Y CLICK` using normalized top-left coordinates. The core consumes the
request, positions the virtual pointer, and optionally clicks. The hook performs
no file access when the variable is unset.

The motion source metadata patch prevents geometry refreshes from decoding and
cropping complete PSB textures when they only need width, height, and anchor
metadata. Pixel decoding remains enabled for SourceCache bitmap creation, while
per-frame node updates use the metadata-only path.

The motion texture cache patch keeps render-source textures untinted in the
cache. Animated packed colors are already applied by the PrivateMotionGLL and
D3D draw methods, so including those per-frame colors in the cache key created
unbounded full-bitmap and GPU-texture variants and applied the tint twice.

The Motion camera and PSB v4 compatibility patch exposes the legacy
`EmotePlayer.setCameraOffset(x, y)` API, retains game-provided PSB decrypt
closures for their full lifetime, and forwards `ResourceManager` callbacks to
the PSB loader. PSB v4 files with absent or encrypted optional extra-chunk
offsets are parsed through their valid base chunk tables instead of seeking
outside the stream.

The motion missing-source cache patch remembers failed static bitmap lookups
and omits unresolved placeholder items from the PrivateMotionGLL queue. This
prevents repeated path/PSB scans and keeps a placeholder from truncating all
later drawable parts in the same frame.

The motion missing-source fast-hit patch returns directly from cached failed
lookups before rebuilding storage candidates. It removes the remaining
per-frame path normalization cost for static placeholder sources.

The ROCgalgame renderer-default patch selects KRKR2's `opengl` RenderManager
when the ROCgalgame runtime environment is present. Set
`ROCGALGAME_KRKR_RENDERER=software` to force the legacy software path.

`krkr2-linux-mali-xr24-surface.patch` requests an opaque RGB EGL surface on
Linux GLES. The GKD Mali driver exports `XR24` Wayland buffers; requesting the
desktop Cocos alpha/stencil defaults produces an incompatible surface that the
compositor displays as black. Other platforms retain the original RGBA8 and
stencil configuration.

The layerEx compatibility bundle adds native, game-independent implementations
of `layerExImage.dll`, `layerExRaster.dll`, and `layerExBTOA.dll`. The apply
script is idempotent and only copies the four bundled compatibility sources and
registers the three translation units in KRKR2's plugin target. Games must link
these modules before constructing their Layer instances so KRKR2's native class
registration is visible to those instances.
