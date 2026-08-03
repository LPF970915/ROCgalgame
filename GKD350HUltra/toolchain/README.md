# GKD350H Ultra toolchain

The first GKD350H Ultra build route uses the WSL Ubuntu cross toolchain:

- target triple: `aarch64-linux-gnu`
- compiler prefix: `aarch64-linux-gnu-`
- build env: `CROSS_TOOL_PREFIX=aarch64-linux-gnu`

This matches the target ROCKNIX aarch64/glibc userland and is compatible with
the RK3576S/RK356x Linux development flow. Keep downloaded or experimental
toolchain archives under this folder if a device-specific toolchain is tested
later.

Current local probe:

```text
aarch64-linux-gnu-g++ (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0
target: aarch64-linux-gnu
path: /usr/bin
```

`aarch64-gkd-krkr2.cmake` is the isolated KrKr2 chainload toolchain. The outer
vcpkg toolchain uses `../vcpkg-triplets/arm64-linux-gkd.cmake`; do not reuse the
Android triplet from the KrKr2 source tree for the ROCKNIX glibc target.

Run the low-cost compiler/sysroot probe before installing vcpkg dependencies:

```powershell
.\build_krkr2.ps1 -Mode Probe
```

ROCreader has a fuller synchronized sysroot for the same GKD350H Ultra target.
Its target `libc.so.6` matches this project's sysroot, so it can be selected
without copying files between projects:

```powershell
.\build_krkr2.ps1 -Mode Probe `
  -Sysroot D:\Works\ROCgalgame\build\gkd350h-glibc234\sysroot
```

Use only a GKD sysroot with a matching target libc. H700 supplies many of the
development headers but is not itself the runtime-library authority for this
device.

KrKr2 uses manifest-mode vcpkg. Keep the vcpkg checkout in the ignored tools
directory and bootstrap it from WSL:

```powershell
git clone https://github.com/microsoft/vcpkg.git .\tools\vcpkg
wsl -d Ubuntu -- bash -lc `
  "cd /mnt/d/Works/ROCgalgame/GKD350HUltra/tools/vcpkg && ./bootstrap-vcpkg.sh -disableMetrics"
```

After the probe passes and the WSL network can reach the vcpkg registries, the
first full dependency configure is intentionally guarded as a heavy operation:

```powershell
.\build_krkr2.ps1 -Mode Configure -ConfirmHeavyBuild `
  -Sysroot D:\Works\ROCgalgame\build\gkd350h-glibc234\sysroot
```

After Configure succeeds, use `FastBuild` for normal KRKR2 `.cpp` and `.h`
changes. It builds only the `krkr2` target in the existing tree, so it skips the
toolchain probe, explicit Configure, manifest-mode vcpkg scan, and unrelated
top-level targets while retaining dependency checks for KRKR2's static-library
subtargets. Only changed sources and their affected libraries are rebuilt:

```powershell
.\build_krkr2.ps1 -Mode FastBuild -Jobs 3 -SafeCpuSet 0-2 `
  -WorkSeconds 180 -CoolSeconds 90 -ConfirmHeavyBuild
```

The build script uses the AArch64 GNU gold linker when it is available. This
reduces the fixed relink cost without changing the compiler, target ABI, or
incremental dependency rules.

Use `Build` when generated build metadata should be checked. Use `Configure`
after changing CMake files, triplets, toolchains, or vcpkg ports. `Build`,
`FastBuild`, and `Full` use the same isolated `build/gkd350h-glibc234/krkr2` tree and
install only the resulting executable as `cores/krkr/krkr2` in the staged
package.
