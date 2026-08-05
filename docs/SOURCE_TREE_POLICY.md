# Source Tree Policy

This project uses five separate working trees. They have different owners and
must not be merged by copying files between them.

| Role | Authoritative path | Repository | Release lock |
| --- | --- | --- | --- |
| ROCgalgame frontend and packaging | `D:\Works\ROCgalgame` | `LPF970915/ROCgalgame` | `GKD350HUltra/*.lock` |
| ONScripterYuri port | `D:\Works\Tyranor\OnscripterYuri` | `LPF970915/ROCgalgame-onsyuri-port` | `GKD350HUltra/onsyuri-port.lock` |
| Native KRKR2 port | `D:\Works\ROCgalgame-krkr2-port` | `LPF970915/ROCgalgame-krkr2-port` | `GKD350HUltra/krkr2-port.lock` |
| KRKRSDL2 port | `D:\Works\Tyranor\krkrsdl2` | `LPF970915/ROCgalgame-krkrsdl2-port` | `GKD350HUltra/krkrsdl2-port.lock` |
| FFmpeg 6 header input | `D:\Works\ROCgalgame-ffmpeg-n6-headers` | `FFmpeg/FFmpeg` | `GKD350HUltra/ffmpeg-headers.lock` |

`D:\Works\Tyranor\krkr2` is an obsolete checkout. It is not a build input,
patch target, or recovery source.

## Rules

1. A release build consumes the exact commit recorded in its lock file. A dirty
   source tree is a diagnostic state, not a releasable source.
2. ONS changes are committed in `ROCgalgame-onsyuri-port`, KRKR2 changes in
   `ROCgalgame-krkr2-port`, and KRKRSDL2 changes in `ROCgalgame-krkrsdl2-port`. The main repository stores build
   scripts, locks, tests, and package metadata, not copied engine source.
3. `GKD350HUltra/patches/` is retained only as legacy reconstruction material.
   Do not apply those patches to the active KRKR2 or KRKRSDL2 trees unless a
   recovery procedure explicitly says so.
4. CMake build directories are caches, never source-of-truth. The expected
   container paths are `/sources/ons`, `/sources/krkr2`, `/sources/krkrsdl2`,
   `/sources/ffmpeg`, and `/workspace`.
   A cache whose source path differs must be discarded or rebuilt in a clean
   directory; it must not be repaired by editing `CMakeCache.txt`.
5. Release binaries are copied only from `GKD350HUltra/dist_glibc234` after
   dependency and provenance checks. Device files and sweep logs are evidence,
   not build inputs.

## Current Triage

Run this before changing any engine source:

```powershell
.\GKD350HUltra\verify_source_trees.ps1
```

The report distinguishes repository drift from generated files. It does not
delete or reset anything. Use `-RequireClean` only before a release build.

Known generated/diagnostic locations currently left in place for review include
`tmp_sweep/`, `.tmp-*`, `scripts/__pycache__/`, and temporary probe patchers.
They must not be copied into a source tree or package.

## Recovery Order

1. Preserve the current main-repository worktree and diagnostics.
2. Verify the five source trees and lock commits.
3. Commit or intentionally discard one logical change at a time in the owning
   engine repository.
4. Configure a fresh fixed-path build only if the existing cache fails the
   source-path check.
5. Build, package, deploy, and sweep from the resulting provenance record.
